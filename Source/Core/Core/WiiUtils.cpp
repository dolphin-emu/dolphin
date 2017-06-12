// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiUtils.h"

#include <algorithm>
#include <cinttypes>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/SettingsHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/Enums.h"
#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/WiiWad.h"

namespace WiiUtils
{
bool InstallWAD(const std::string& wad_path)
{
  const DiscIO::WiiWAD wad{wad_path};
  if (!wad.IsValid())
  {
    PanicAlertT("WAD installation failed: The selected file is not a valid WAD.");
    return false;
  }

  const auto tmd = wad.GetTMD();
  IOS::HLE::Kernel ios;
  const auto es = ios.GetES();

  IOS::HLE::Device::ES::Context context;
  IOS::HLE::ReturnCode ret;
  const bool checks_disabled = SConfig::GetInstance().m_disable_signature_checks;
  while ((ret = es->ImportTicket(wad.GetTicket().GetBytes(), wad.GetCertificateChain())) < 0 ||
         (ret = es->ImportTitleInit(context, tmd.GetBytes(), wad.GetCertificateChain())) < 0)
  {
    if (!checks_disabled && ret == IOS::HLE::IOSC_FAIL_CHECKVALUE &&
        AskYesNoT("This WAD does not have a valid signature. Continue to import?"))
    {
      SConfig::GetInstance().m_disable_signature_checks = true;
      continue;
    }

    SConfig::GetInstance().m_disable_signature_checks = checks_disabled;
    PanicAlertT("WAD installation failed: Could not initialise title import.");
    return false;
  }
  SConfig::GetInstance().m_disable_signature_checks = checks_disabled;

  const bool contents_imported = [&]() {
    const u64 title_id = tmd.GetTitleId();
    for (const IOS::ES::Content& content : tmd.GetContents())
    {
      const std::vector<u8> data = wad.GetContent(content.index);

      if (es->ImportContentBegin(context, title_id, content.id) < 0 ||
          es->ImportContentData(context, 0, data.data(), static_cast<u32>(data.size())) < 0 ||
          es->ImportContentEnd(context, 0) < 0)
      {
        PanicAlertT("WAD installation failed: Could not import content %08x.", content.id);
        return false;
      }
    }
    return true;
  }();

  if ((contents_imported && es->ImportTitleDone(context) < 0) ||
      (!contents_imported && es->ImportTitleCancel(context) < 0))
  {
    PanicAlertT("WAD installation failed: Could not finalise title import.");
    return false;
  }

  DiscIO::NANDContentManager::Access().ClearCache();
  return true;
}

class SystemUpdater final
{
public:
  UpdateResult DoOnlineUpdate(UpdateCallback update_callback);

private:
  struct TitleInfo
  {
    u64 id;
    u16 version;
  };

  struct Response
  {
    std::string content_prefix_url;
    std::vector<TitleInfo> titles;
  };

  std::string GetDeviceRegion();
  std::string GetDeviceId();

  Response GetSystemTitles();
  Response ParseTitlesResponse(const std::vector<u8>& response) const;

  bool InstallTitleFromNUS(const std::string& prefix_url, const TitleInfo& title,
                           std::unordered_set<u64>* updated_titles);

  // Helper functions to download contents from NUS.
  std::pair<IOS::ES::TMDReader, std::vector<u8>> DownloadTMD(const std::string& prefix_url,
                                                             const TitleInfo& title);
  std::pair<std::vector<u8>, std::vector<u8>> DownloadTicket(const std::string& prefix_url,
                                                             const TitleInfo& title);
  std::optional<std::vector<u8>> DownloadContent(const std::string& prefix_url,
                                                 const TitleInfo& title, u32 cid);

  UpdateCallback m_update_callback;
  IOS::HLE::Kernel m_ios;
  Common::HttpRequest m_http{0};
};

std::string SystemUpdater::GetDeviceRegion()
{
  // Try to determine the region from an installed system menu.
  const auto tmd = m_ios.GetES()->FindInstalledTMD(TITLEID_SYSMENU);
  if (tmd.IsValid())
  {
    const DiscIO::Region region = tmd.GetRegion();
    static const std::map<DiscIO::Region, std::string> regions = {
        {DiscIO::Region::NTSC_J, "JPN"},
        {DiscIO::Region::NTSC_U, "USA"},
        {DiscIO::Region::PAL, "EUR"},
        {DiscIO::Region::NTSC_K, "KOR"},
        {DiscIO::Region::UNKNOWN_REGION, "EUR"}};
    return regions.at(region);
  }

  // Try to determine the region from the setting.txt file.
  // We check that file after the system menu, because Dolphin likes to change the region
  // automatically without warning the user when launching a disc game from the game list.
  SettingsHandler gen;
  const std::string settings_file_path =
      Common::GetTitleDataPath(TITLEID_SYSMENU, Common::FROM_CONFIGURED_ROOT) + WII_SETTING;
  if (File::Exists(settings_file_path) && gen.Open(settings_file_path))
  {
    const std::string region = gen.GetValue("AREA");
    if (!region.empty())
      return region;
  }

  // Fall back to EUR.
  return "EUR";
}

std::string SystemUpdater::GetDeviceId()
{
  u32 ios_device_id;
  m_ios.GetES()->GetDeviceId(&ios_device_id);
  return StringFromFormat("%" PRIu64, (u64(1) << 32) | ios_device_id);
}

constexpr const char* GET_SYSTEM_TITLES_REQUEST_PAYLOAD = R"END(\
<?xml version="1.0" encoding="UTF-8"?>
<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/"
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <soapenv:Body>
    <GetSystemUpdateRequest xmlns="urn:nus.wsapi.broadon.com">
      <Version>1.0</Version>
      <MessageId>0</MessageId>
      <DeviceId></DeviceId>
      <RegionId></RegionId>
    </GetSystemUpdateRequest>
  </soapenv:Body>
</soapenv:Envelope>
)END";

SystemUpdater::Response SystemUpdater::ParseTitlesResponse(const std::vector<u8>& response) const
{
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(response.data(), response.size());
  if (!result)
    return {};

  const pugi::xml_node node = doc.select_node("//*[local-name()='GetSystemUpdateResponse']").node();
  if (!node)
  {
    ERROR_LOG(CORE, "ParseTitlesResponse: Could not find response node");
    return {};
  }

  const int code = node.child("ErrorCode").text().as_int();
  if (code != 0)
  {
    ERROR_LOG(CORE, "ParseTitlesResponse: Non-zero error code (%d)", code);
    return {};
  }

  // libnup uses the uncached URL, not the cached one. However, that one is way, way too slow,
  // so let's use the cached endpoint.
  SystemUpdater::Response info;
  info.content_prefix_url = node.child("ContentPrefixURL").text().as_string();
  // Disable HTTPS because we can't use it without a device certificate.
  info.content_prefix_url = ReplaceAll(info.content_prefix_url, "https://", "http://");
  if (info.content_prefix_url.empty())
  {
    ERROR_LOG(CORE, "ParseTitlesResponse: Empty content prefix URL");
    return {};
  }

  for (const pugi::xml_node& title_node : node.children("TitleVersion"))
  {
    const u64 title_id = std::stoull(title_node.child("TitleId").text().as_string(), nullptr, 16);
    const u16 title_version = static_cast<u16>(title_node.child("Version").text().as_uint());
    info.titles.push_back({title_id, title_version});
  }
  return info;
}

SystemUpdater::Response SystemUpdater::GetSystemTitles()
{
  // Construct the request by import the template first, then updating some fields.
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(GET_SYSTEM_TITLES_REQUEST_PAYLOAD);
  _assert_(result);

  // Nintendo does not really care about the device ID or verify that we *are* that device,
  // as long as it is a valid Wii device ID.
  const std::string device_id = GetDeviceId();
  _assert_(doc.select_node("//DeviceId").node().text().set(device_id.c_str()));

  // Write the correct device region.
  const std::string region = GetDeviceRegion();
  _assert_(doc.select_node("//RegionId").node().text().set(region.c_str()));

  std::ostringstream stream;
  doc.save(stream);
  const std::string request = stream.str();

  // Note: We don't use HTTPS because that would require the user to have
  // a device certificate which cannot be redistributed with Dolphin.
  // This is fine, because IOS has signature checks.
  const Common::HttpRequest::Response response =
      m_http.Post("http://nus.shop.wii.com/nus/services/NetUpdateSOAP", request,
                  {
                      {"SOAPAction", "urn:nus.wsapi.broadon.com/GetSystemUpdate"},
                      {"User-Agent", "wii libnup/1.0"},
                      {"Content-Type", "text/xml; charset=utf-8"},
                  });

  if (!response)
    return {};
  return ParseTitlesResponse(*response);
}

UpdateResult SystemUpdater::DoOnlineUpdate(UpdateCallback update_callback)
{
  m_update_callback = std::move(update_callback);

  const SystemUpdater::Response info = GetSystemTitles();
  if (info.titles.empty())
    return UpdateResult::Failed;

  // Download and install any title that is older than the NUS version.
  // The order is determined by the server response, which is: boot2, System Menu, IOSes, channels.
  // As we install any IOS required by titles, the real order is boot2, SM IOS, SM, IOSes, channels.
  std::unordered_set<u64> updated_titles;
  size_t processed = 0;
  for (const TitleInfo& title : info.titles)
  {
    if (m_update_callback)
      m_update_callback(processed++, info.titles.size(), title.id);

    if (!InstallTitleFromNUS(info.content_prefix_url, title, &updated_titles))
    {
      ERROR_LOG(CORE, "Failed to update %016" PRIx64 " -- aborting update", title.id);
      return UpdateResult::Failed;
    }

    if (m_update_callback)
      m_update_callback(processed, info.titles.size(), title.id);
  }

  if (updated_titles.empty())
  {
    NOTICE_LOG(CORE, "Update finished - Already up-to-date");
    return UpdateResult::AlreadyUpToDate;
  }
  NOTICE_LOG(CORE, "Update finished - %zu updates installed", updated_titles.size());
  return UpdateResult::Succeeded;
}

bool SystemUpdater::InstallTitleFromNUS(const std::string& prefix_url, const TitleInfo& title,
                                        std::unordered_set<u64>* updated_titles)
{
  // We currently don't support boot2 updates at all, so ignore any attempt to install it.
  if (title.id == 0x0000000100000001)
    return true;

  const auto es = m_ios.GetES();
  // Nothing to do if the installed version is already up-to-date or already updated.
  // Download any title that is newer than the installed version.
  const auto installed_tmd = es->FindInstalledTMD(title.id);
  if (installed_tmd.IsValid() && installed_tmd.GetTitleVersion() >= title.version)
    return true;

  if (updated_titles->find(title.id) != updated_titles->end())
    return true;

  NOTICE_LOG(CORE, "Updating title %016" PRIx64, title.id);

  // Download the ticket and certificates.
  const auto ticket = DownloadTicket(prefix_url, title);
  if (ticket.first.empty() || ticket.second.empty())
  {
    ERROR_LOG(CORE, "Failed to download ticket and certs");
    return false;
  }

  // Import the ticket.
  IOS::HLE::ReturnCode ret = IOS::HLE::IPC_SUCCESS;
  if ((ret = es->ImportTicket(ticket.first, ticket.second)) < 0)
  {
    ERROR_LOG(CORE, "Failed to import ticket: error %d", ret);
    return false;
  }

  // Download the TMD.
  const auto tmd = DownloadTMD(prefix_url, title);
  if (!tmd.first.IsValid())
  {
    ERROR_LOG(CORE, "Failed to download TMD");
    return false;
  }

  // Download and import any required system title first.
  const u64 ios_id = tmd.first.GetIOSId();
  if (ios_id != 0 && IOS::ES::IsTitleType(ios_id, IOS::ES::TitleType::System))
  {
    if (!es->FindInstalledTMD(ios_id).IsValid())
    {
      WARN_LOG(CORE, "Importing required system title %016" PRIx64 " first", ios_id);
      if (!InstallTitleFromNUS(prefix_url, {ios_id, 0}, updated_titles))
      {
        ERROR_LOG(CORE, "Failed to import required system title %016" PRIx64, ios_id);
        return false;
      }
    }
  }

  // Initialise the title import.
  IOS::HLE::Device::ES::Context context;
  if ((ret = es->ImportTitleInit(context, tmd.first.GetBytes(), tmd.second)) < 0)
  {
    ERROR_LOG(CORE, "Failed to initialise title import: error %d", ret);
    return false;
  }

  // Now download and install contents listed in the TMD.
  const std::vector<IOS::ES::Content> stored_contents = es->GetStoredContentsFromTMD(tmd.first);
  const bool contents_imported = [&]() {
    for (const IOS::ES::Content& content : tmd.first.GetContents())
    {
      const bool is_already_installed = std::find_if(stored_contents.begin(), stored_contents.end(),
                                                     [&content](const auto& stored_content) {
                                                       return stored_content.id == content.id;
                                                     }) != stored_contents.end();

      // Do skip what is already installed on the NAND.
      if (is_already_installed)
        continue;

      if ((ret = es->ImportContentBegin(context, title.id, content.id)) < 0)
      {
        ERROR_LOG(CORE, "Failed to initialise import for content %08x: error %d", content.id, ret);
        return false;
      }

      const std::optional<std::vector<u8>> data = DownloadContent(prefix_url, title, content.id);
      if (!data)
      {
        ERROR_LOG(CORE, "Failed to download content %08x", content.id);
        return false;
      }

      if (es->ImportContentData(context, 0, data->data(), static_cast<u32>(data->size())) < 0 ||
          es->ImportContentEnd(context, 0) < 0)
      {
        ERROR_LOG(CORE, "Failed to import content %08x", content.id);
        return false;
      }
    }
    return true;
  }();

  if ((contents_imported && (ret = es->ImportTitleDone(context)) < 0) ||
      (!contents_imported && (ret = es->ImportTitleCancel(context)) < 0))
  {
    ERROR_LOG(CORE, "Failed to finalise title import: error %d", ret);
    return false;
  }

  if (!contents_imported)
    return false;

  updated_titles->emplace(title.id);
  return true;
}

std::pair<IOS::ES::TMDReader, std::vector<u8>>
SystemUpdater::DownloadTMD(const std::string& prefix_url, const TitleInfo& title)
{
  const std::string url =
      (title.version == 0) ?
          prefix_url + StringFromFormat("/%016" PRIx64 "/tmd", title.id) :
          prefix_url + StringFromFormat("/%016" PRIx64 "/tmd.%u", title.id, title.version);
  const Common::HttpRequest::Response response = m_http.Get(url);
  if (!response)
    return {};

  // Too small to contain both the TMD and a cert chain.
  if (response->size() <= sizeof(IOS::ES::TMDHeader))
    return {};
  const size_t tmd_size =
      sizeof(IOS::ES::TMDHeader) +
      sizeof(IOS::ES::Content) *
          Common::swap16(response->data() + offsetof(IOS::ES::TMDHeader, num_contents));
  if (response->size() <= tmd_size)
    return {};

  const auto tmd_begin = response->begin();
  const auto tmd_end = tmd_begin + tmd_size;

  return {IOS::ES::TMDReader(std::vector<u8>(tmd_begin, tmd_end)),
          std::vector<u8>(tmd_end, response->end())};
}

std::pair<std::vector<u8>, std::vector<u8>>
SystemUpdater::DownloadTicket(const std::string& prefix_url, const TitleInfo& title)
{
  const std::string url = prefix_url + StringFromFormat("/%016" PRIx64 "/cetk", title.id);
  const Common::HttpRequest::Response response = m_http.Get(url);
  if (!response)
    return {};

  // Too small to contain both the ticket and a cert chain.
  if (response->size() <= sizeof(IOS::ES::Ticket))
    return {};

  const auto ticket_begin = response->begin();
  const auto ticket_end = ticket_begin + sizeof(IOS::ES::Ticket);
  return {std::vector<u8>(ticket_begin, ticket_end), std::vector<u8>(ticket_end, response->end())};
}

std::optional<std::vector<u8>> SystemUpdater::DownloadContent(const std::string& prefix_url,
                                                              const TitleInfo& title, u32 cid)
{
  const std::string url = prefix_url + StringFromFormat("/%016" PRIx64 "/%08x", title.id, cid);
  return m_http.Get(url);
}

UpdateResult DoOnlineUpdate(UpdateCallback update_callback)
{
  SystemUpdater updater;
  const UpdateResult result = updater.DoOnlineUpdate(std::move(update_callback));
  DiscIO::NANDContentManager::Access().ClearCache();
  return result;
}
}
