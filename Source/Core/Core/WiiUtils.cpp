// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiUtils.h"

#include <algorithm>
#include <bitset>
#include <cinttypes>
#include <cstddef>
#include <cstring>
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
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeFileBlobReader.h"
#include "DiscIO/VolumeWii.h"
#include "DiscIO/WiiWad.h"

namespace WiiUtils
{
bool InstallWAD(IOS::HLE::Kernel& ios, const DiscIO::WiiWAD& wad)
{
  if (!wad.IsValid())
  {
    PanicAlertT("WAD installation failed: The selected file is not a valid WAD.");
    return false;
  }

  const auto tmd = wad.GetTMD();
  const auto es = ios.GetES();

  IOS::HLE::Device::ES::Context context;
  IOS::HLE::ReturnCode ret;
  const bool checks_enabled = SConfig::GetInstance().m_enable_signature_checks;

  IOS::ES::TicketReader ticket = wad.GetTicket();
  // Ensure the common key index is correct, as it's checked by IOS.
  ticket.FixCommonKeyIndex();

  while ((ret = es->ImportTicket(ticket.GetBytes(), wad.GetCertificateChain())) < 0 ||
         (ret = es->ImportTitleInit(context, tmd.GetBytes(), wad.GetCertificateChain())) < 0)
  {
    if (checks_enabled && ret == IOS::HLE::IOSC_FAIL_CHECKVALUE &&
        AskYesNoT("This WAD has not been signed by Nintendo. Continue to import?"))
    {
      SConfig::GetInstance().m_enable_signature_checks = false;
      continue;
    }

    SConfig::GetInstance().m_enable_signature_checks = checks_enabled;
    PanicAlertT("WAD installation failed: Could not initialise title import.");
    return false;
  }
  SConfig::GetInstance().m_enable_signature_checks = checks_enabled;

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

  return true;
}

bool InstallWAD(const std::string& wad_path)
{
  IOS::HLE::Kernel ios;
  const DiscIO::WiiWAD wad{wad_path};
  const bool result = InstallWAD(ios, wad);

  DiscIO::NANDContentManager::Access().ClearCache();
  return result;
}

// Common functionality for system updaters.
class SystemUpdater
{
public:
  virtual ~SystemUpdater() = default;

protected:
  struct TitleInfo
  {
    u64 id;
    u16 version;
  };

  std::string GetDeviceRegion();
  std::string GetDeviceId();

  IOS::HLE::Kernel m_ios;
};

std::string SystemUpdater::GetDeviceRegion()
{
  // Try to determine the region from an installed system menu.
  const auto tmd = m_ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);
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
  return "";
}

std::string SystemUpdater::GetDeviceId()
{
  u32 ios_device_id;
  if (m_ios.GetES()->GetDeviceId(&ios_device_id) < 0)
    return "";
  return StringFromFormat("%" PRIu64, (u64(1) << 32) | ios_device_id);
}

class OnlineSystemUpdater final : public SystemUpdater
{
public:
  OnlineSystemUpdater(UpdateCallback update_callback, const std::string& region);
  UpdateResult DoOnlineUpdate();

private:
  struct Response
  {
    std::string content_prefix_url;
    std::vector<TitleInfo> titles;
  };

  Response GetSystemTitles();
  Response ParseTitlesResponse(const std::vector<u8>& response) const;
  bool ShouldInstallTitle(const TitleInfo& title);

  UpdateResult InstallTitleFromNUS(const std::string& prefix_url, const TitleInfo& title,
                                   std::unordered_set<u64>* updated_titles);

  // Helper functions to download contents from NUS.
  std::pair<IOS::ES::TMDReader, std::vector<u8>> DownloadTMD(const std::string& prefix_url,
                                                             const TitleInfo& title);
  std::pair<std::vector<u8>, std::vector<u8>> DownloadTicket(const std::string& prefix_url,
                                                             const TitleInfo& title);
  std::optional<std::vector<u8>> DownloadContent(const std::string& prefix_url,
                                                 const TitleInfo& title, u32 cid);

  UpdateCallback m_update_callback;
  std::string m_requested_region;
  Common::HttpRequest m_http{std::chrono::minutes{3}};
};

OnlineSystemUpdater::OnlineSystemUpdater(UpdateCallback update_callback, const std::string& region)
    : m_update_callback(std::move(update_callback)), m_requested_region(region)
{
}

OnlineSystemUpdater::Response
OnlineSystemUpdater::ParseTitlesResponse(const std::vector<u8>& response) const
{
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_buffer(response.data(), response.size());
  if (!result)
  {
    ERROR_LOG(CORE, "ParseTitlesResponse: Could not parse response");
    return {};
  }

  // pugixml doesn't fully support namespaces and ignores them.
  const pugi::xml_node node = doc.select_node("//GetSystemUpdateResponse").node();
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
  Response info;
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

bool OnlineSystemUpdater::ShouldInstallTitle(const TitleInfo& title)
{
  const auto es = m_ios.GetES();
  const auto installed_tmd = es->FindInstalledTMD(title.id);
  return !(installed_tmd.IsValid() && installed_tmd.GetTitleVersion() >= title.version &&
           es->GetStoredContentsFromTMD(installed_tmd).size() == installed_tmd.GetNumContents());
}

constexpr const char* GET_SYSTEM_TITLES_REQUEST_PAYLOAD = R"(<?xml version="1.0" encoding="UTF-8"?>
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
)";

OnlineSystemUpdater::Response OnlineSystemUpdater::GetSystemTitles()
{
  // Construct the request by loading the template first, then updating some fields.
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(GET_SYSTEM_TITLES_REQUEST_PAYLOAD);
  _assert_(result);

  // Nintendo does not really care about the device ID or verify that we *are* that device,
  // as long as it is a valid Wii device ID.
  const std::string device_id = GetDeviceId();
  _assert_(doc.select_node("//DeviceId").node().text().set(device_id.c_str()));

  // Write the correct device region.
  const std::string region = m_requested_region.empty() ? GetDeviceRegion() : m_requested_region;
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

UpdateResult OnlineSystemUpdater::DoOnlineUpdate()
{
  const Response info = GetSystemTitles();
  if (info.titles.empty())
    return UpdateResult::ServerFailed;

  // Download and install any title that is older than the NUS version.
  // The order is determined by the server response, which is: boot2, System Menu, IOSes, channels.
  // As we install any IOS required by titles, the real order is boot2, SM IOS, SM, IOSes, channels.
  std::unordered_set<u64> updated_titles;
  size_t processed = 0;
  for (const TitleInfo& title : info.titles)
  {
    if (!m_update_callback(processed++, info.titles.size(), title.id))
      return UpdateResult::Cancelled;

    const UpdateResult res = InstallTitleFromNUS(info.content_prefix_url, title, &updated_titles);
    if (res != UpdateResult::Succeeded)
    {
      ERROR_LOG(CORE, "Failed to update %016" PRIx64 " -- aborting update", title.id);
      return res;
    }

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

UpdateResult OnlineSystemUpdater::InstallTitleFromNUS(const std::string& prefix_url,
                                                      const TitleInfo& title,
                                                      std::unordered_set<u64>* updated_titles)
{
  // We currently don't support boot2 updates at all, so ignore any attempt to install it.
  if (title.id == Titles::BOOT2)
    return UpdateResult::Succeeded;

  if (!ShouldInstallTitle(title) || updated_titles->find(title.id) != updated_titles->end())
    return UpdateResult::Succeeded;

  NOTICE_LOG(CORE, "Updating title %016" PRIx64, title.id);

  // Download the ticket and certificates.
  const auto ticket = DownloadTicket(prefix_url, title);
  if (ticket.first.empty() || ticket.second.empty())
  {
    ERROR_LOG(CORE, "Failed to download ticket and certs");
    return UpdateResult::DownloadFailed;
  }

  // Import the ticket.
  IOS::HLE::ReturnCode ret = IOS::HLE::IPC_SUCCESS;
  const auto es = m_ios.GetES();
  if ((ret = es->ImportTicket(ticket.first, ticket.second)) < 0)
  {
    ERROR_LOG(CORE, "Failed to import ticket: error %d", ret);
    return UpdateResult::ImportFailed;
  }

  // Download the TMD.
  const auto tmd = DownloadTMD(prefix_url, title);
  if (!tmd.first.IsValid())
  {
    ERROR_LOG(CORE, "Failed to download TMD");
    return UpdateResult::DownloadFailed;
  }

  // Download and import any required system title first.
  const u64 ios_id = tmd.first.GetIOSId();
  if (ios_id != 0 && IOS::ES::IsTitleType(ios_id, IOS::ES::TitleType::System))
  {
    if (!es->FindInstalledTMD(ios_id).IsValid())
    {
      WARN_LOG(CORE, "Importing required system title %016" PRIx64 " first", ios_id);
      const UpdateResult res = InstallTitleFromNUS(prefix_url, {ios_id, 0}, updated_titles);
      if (res != UpdateResult::Succeeded)
      {
        ERROR_LOG(CORE, "Failed to import required system title %016" PRIx64, ios_id);
        return res;
      }
    }
  }

  // Initialise the title import.
  IOS::HLE::Device::ES::Context context;
  if ((ret = es->ImportTitleInit(context, tmd.first.GetBytes(), tmd.second)) < 0)
  {
    ERROR_LOG(CORE, "Failed to initialise title import: error %d", ret);
    return UpdateResult::ImportFailed;
  }

  // Now download and install contents listed in the TMD.
  const std::vector<IOS::ES::Content> stored_contents = es->GetStoredContentsFromTMD(tmd.first);
  const UpdateResult import_result = [&]() {
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
        return UpdateResult::ImportFailed;
      }

      const std::optional<std::vector<u8>> data = DownloadContent(prefix_url, title, content.id);
      if (!data)
      {
        ERROR_LOG(CORE, "Failed to download content %08x", content.id);
        return UpdateResult::DownloadFailed;
      }

      if (es->ImportContentData(context, 0, data->data(), static_cast<u32>(data->size())) < 0 ||
          es->ImportContentEnd(context, 0) < 0)
      {
        ERROR_LOG(CORE, "Failed to import content %08x", content.id);
        return UpdateResult::ImportFailed;
      }
    }
    return UpdateResult::Succeeded;
  }();
  const bool all_contents_imported = import_result == UpdateResult::Succeeded;

  if ((all_contents_imported && (ret = es->ImportTitleDone(context)) < 0) ||
      (!all_contents_imported && (ret = es->ImportTitleCancel(context)) < 0))
  {
    ERROR_LOG(CORE, "Failed to finalise title import: error %d", ret);
    return UpdateResult::ImportFailed;
  }

  if (!all_contents_imported)
    return import_result;

  updated_titles->emplace(title.id);
  return UpdateResult::Succeeded;
}

std::pair<IOS::ES::TMDReader, std::vector<u8>>
OnlineSystemUpdater::DownloadTMD(const std::string& prefix_url, const TitleInfo& title)
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
OnlineSystemUpdater::DownloadTicket(const std::string& prefix_url, const TitleInfo& title)
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

std::optional<std::vector<u8>> OnlineSystemUpdater::DownloadContent(const std::string& prefix_url,
                                                                    const TitleInfo& title, u32 cid)
{
  const std::string url = prefix_url + StringFromFormat("/%016" PRIx64 "/%08x", title.id, cid);
  return m_http.Get(url);
}

class DiscSystemUpdater final : public SystemUpdater
{
public:
  DiscSystemUpdater(UpdateCallback update_callback, const std::string& image_path)
      : m_update_callback{std::move(update_callback)},
        m_volume{DiscIO::CreateVolumeFromFilename(image_path)}
  {
  }
  UpdateResult DoDiscUpdate();

private:
#pragma pack(push, 8)
  struct ManifestHeader
  {
    char timestamp[0x10];  // YYYY/MM/DD
    // There is a u32 in newer info files to indicate the number of entries,
    // but it's not used in older files, and it's not always at the same offset.
    // Too unreliable to use it.
    u32 padding[4];
  };
  static_assert(sizeof(ManifestHeader) == 32, "Wrong size");

  struct Entry
  {
    u32 type;
    u32 attribute;
    u32 unknown1;
    u32 unknown2;
    char path[0x40];
    u64 title_id;
    u16 title_version;
    char name[0x40];
    char info[0x40];
    u8 unused[0x120];
  };
  static_assert(sizeof(Entry) == 512, "Wrong size");
#pragma pack(pop)

  UpdateResult UpdateFromManifest(const std::string& manifest_name);
  UpdateResult ProcessEntry(u32 type, std::bitset<32> attrs, const TitleInfo& title,
                            const std::string& path);

  UpdateCallback m_update_callback;
  std::unique_ptr<DiscIO::Volume> m_volume;
  std::unique_ptr<DiscIO::FileSystem> m_disc_fs;
};

UpdateResult DiscSystemUpdater::DoDiscUpdate()
{
  if (!m_volume)
    return UpdateResult::DiscReadFailed;

  // Do not allow mismatched regions, because installing an update will automatically change
  // the Wii's region and may result in semi/full system menu bricks.
  const IOS::ES::TMDReader system_menu_tmd = m_ios.GetES()->FindInstalledTMD(Titles::SYSTEM_MENU);
  if (system_menu_tmd.IsValid() && m_volume->GetRegion() != system_menu_tmd.GetRegion())
    return UpdateResult::RegionMismatch;

  const auto partitions = m_volume->GetPartitions();
  const auto update_partition =
      std::find_if(partitions.cbegin(), partitions.cend(), [&](const DiscIO::Partition& partition) {
        return m_volume->GetPartitionType(partition) == 1u;
      });

  if (update_partition == partitions.cend())
  {
    ERROR_LOG(CORE, "Could not find any update partition");
    return UpdateResult::MissingUpdatePartition;
  }

  m_disc_fs = DiscIO::CreateFileSystem(m_volume.get(), *update_partition);
  if (!m_disc_fs || !m_disc_fs->IsValid())
    return UpdateResult::DiscReadFailed;

  return UpdateFromManifest("__update.inf");
}

UpdateResult DiscSystemUpdater::UpdateFromManifest(const std::string& manifest_name)
{
  const std::unique_ptr<DiscIO::FileInfo> update_manifest = m_disc_fs->FindFileInfo(manifest_name);
  if (!update_manifest ||
      (update_manifest->GetSize() - sizeof(ManifestHeader)) % sizeof(Entry) != 0)
  {
    ERROR_LOG(CORE, "Invalid or missing update manifest");
    return UpdateResult::DiscReadFailed;
  }

  const u32 num_entries = (update_manifest->GetSize() - sizeof(ManifestHeader)) / sizeof(Entry);
  if (num_entries > 200)
    return UpdateResult::DiscReadFailed;

  std::vector<u8> entry(sizeof(Entry));
  size_t updates_installed = 0;
  for (u32 i = 0; i < num_entries; ++i)
  {
    const u32 offset = sizeof(ManifestHeader) + sizeof(Entry) * i;
    if (entry.size() != DiscIO::ReadFile(*m_volume, m_disc_fs->GetPartition(),
                                         update_manifest.get(), entry.data(), entry.size(), offset))
    {
      ERROR_LOG(CORE, "Failed to read update information from update manifest");
      return UpdateResult::DiscReadFailed;
    }

    const u32 type = Common::swap32(entry.data() + offsetof(Entry, type));
    const std::bitset<32> attrs = Common::swap32(entry.data() + offsetof(Entry, attribute));
    const u64 title_id = Common::swap64(entry.data() + offsetof(Entry, title_id));
    const u16 title_version = Common::swap16(entry.data() + offsetof(Entry, title_version));
    const char* path_pointer = reinterpret_cast<const char*>(entry.data() + offsetof(Entry, path));
    const std::string path{path_pointer, strnlen(path_pointer, sizeof(Entry::path))};

    if (!m_update_callback(i, num_entries, title_id))
      return UpdateResult::Cancelled;

    const UpdateResult res = ProcessEntry(type, attrs, {title_id, title_version}, path);
    if (res != UpdateResult::Succeeded && res != UpdateResult::AlreadyUpToDate)
    {
      ERROR_LOG(CORE, "Failed to update %016" PRIx64 " -- aborting update", title_id);
      return res;
    }

    if (res == UpdateResult::Succeeded)
      ++updates_installed;
  }
  return updates_installed == 0 ? UpdateResult::AlreadyUpToDate : UpdateResult::Succeeded;
}

UpdateResult DiscSystemUpdater::ProcessEntry(u32 type, std::bitset<32> attrs,
                                             const TitleInfo& title, const std::string& path)
{
  // Skip any unknown type and boot2 updates (for now).
  if (type != 2 && type != 3 && type != 6 && type != 7)
    return UpdateResult::AlreadyUpToDate;

  const IOS::ES::TMDReader tmd = m_ios.GetES()->FindInstalledTMD(title.id);
  const IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(title.id);

  // Optional titles can be skipped if the ticket is present, even when the title isn't installed.
  if (attrs.test(16) && ticket.IsValid())
    return UpdateResult::AlreadyUpToDate;

  // Otherwise, the title is only skipped if it is installed, its ticket is imported,
  // and the installed version is new enough. No further checks unlike the online updater.
  if (tmd.IsValid() && tmd.GetTitleVersion() >= title.version)
    return UpdateResult::AlreadyUpToDate;

  // Import the WAD.
  const std::unique_ptr<DiscIO::FileInfo> wad_file = m_disc_fs->FindFileInfo(path);
  if (!wad_file)
  {
    ERROR_LOG(CORE, "Failed to get info for %s", path.c_str());
    return UpdateResult::DiscReadFailed;
  }

  const DiscIO::WiiWAD wad{DiscIO::VolumeFileBlobReader::Create(*m_volume, *m_disc_fs, path)};
  return InstallWAD(m_ios, wad) ? UpdateResult::Succeeded : UpdateResult::ImportFailed;
}

UpdateResult DoOnlineUpdate(UpdateCallback update_callback, const std::string& region)
{
  OnlineSystemUpdater updater{std::move(update_callback), region};
  const UpdateResult result = updater.DoOnlineUpdate();
  DiscIO::NANDContentManager::Access().ClearCache();
  return result;
}

UpdateResult DoDiscUpdate(UpdateCallback update_callback, const std::string& image_path)
{
  DiscSystemUpdater updater{std::move(update_callback), image_path};
  const UpdateResult result = updater.DoDiscUpdate();
  DiscIO::NANDContentManager::Access().ClearCache();
  return result;
}
}
