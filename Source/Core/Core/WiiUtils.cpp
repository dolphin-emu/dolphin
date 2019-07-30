// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/WiiUtils.h"

#include <algorithm>
#include <bitset>
#include <cinttypes>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>
#ifndef ANDROID
#include <fmt/format.h>
#include <pugixml.hpp>
#endif
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#ifndef ANDROID
#include "Common/HttpRequest.h"
#endif
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
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/SysConf.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeFileBlobReader.h"
#include "DiscIO/VolumeWad.h"

namespace WiiUtils
{
static bool ImportWAD(IOS::HLE::Kernel& ios, const DiscIO::VolumeWAD& wad)
{
  if (!wad.GetTicket().IsValid() || !wad.GetTMD().IsValid())
  {
    PanicAlertT("WAD installation failed: The selected file is not a valid WAD.");
    return false;
  }

  const auto tmd = wad.GetTMD();
  const auto es = ios.GetES();

  IOS::HLE::Device::ES::Context context;
  IOS::HLE::ReturnCode ret;
  const bool checks_enabled = SConfig::GetInstance().m_enable_signature_checks;

  // Ensure the common key index is correct, as it's checked by IOS.
  IOS::ES::TicketReader ticket = wad.GetTicketWithFixedCommonKey();

  while ((ret = es->ImportTicket(ticket.GetBytes(), wad.GetCertificateChain(),
                                 IOS::HLE::Device::ES::TicketImportType::Unpersonalised)) < 0 ||
         (ret = es->ImportTitleInit(context, tmd.GetBytes(), wad.GetCertificateChain())) < 0)
  {
    if (checks_enabled && ret == IOS::HLE::IOSC_FAIL_CHECKVALUE &&
        AskYesNoT("This WAD has not been signed by Nintendo. Continue to import?"))
    {
      SConfig::GetInstance().m_enable_signature_checks = false;
      continue;
    }

    if (ret != IOS::HLE::IOSC_FAIL_CHECKVALUE)
      PanicAlertT("WAD installation failed: Could not initialise title import (error %d).", ret);
    SConfig::GetInstance().m_enable_signature_checks = checks_enabled;
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

bool InstallWAD(IOS::HLE::Kernel& ios, const DiscIO::VolumeWAD& wad, InstallType install_type)
{
  if (!wad.GetTMD().IsValid())
    return false;

  SysConf sysconf{ios.GetFS()};
  SysConf::Entry* tid_entry = sysconf.GetOrAddEntry("IPL.TID", SysConf::Entry::Type::LongLong);
  const u64 previous_temporary_title_id = Common::swap64(tid_entry->GetData<u64>(0));
  const u64 title_id = wad.GetTMD().GetTitleId();

  // Skip the install if the WAD is already installed.
  const auto installed_contents = ios.GetES()->GetStoredContentsFromTMD(wad.GetTMD());
  if (wad.GetTMD().GetContents() == installed_contents)
  {
    // Clear the "temporary title ID" flag in case the user tries to permanently install a title
    // that has already been imported as a temporary title.
    if (previous_temporary_title_id == title_id && install_type == InstallType::Permanent)
      tid_entry->SetData<u64>(0);
    return true;
  }

  // If a different version is currently installed, warn the user to make sure
  // they don't overwrite the current version by mistake.
  const IOS::ES::TMDReader installed_tmd = ios.GetES()->FindInstalledTMD(title_id);
  const bool has_another_version =
      installed_tmd.IsValid() && installed_tmd.GetTitleVersion() != wad.GetTMD().GetTitleVersion();
  if (has_another_version &&
      !AskYesNoT("A different version of this title is already installed on the NAND.\n\n"
                 "Installed version: %u\nWAD version: %u\n\n"
                 "Installing this WAD will replace it irreversibly. Continue?",
                 installed_tmd.GetTitleVersion(), wad.GetTMD().GetTitleVersion()))
  {
    return false;
  }

  // Delete a previous temporary title, if it exists.
  if (previous_temporary_title_id)
    ios.GetES()->DeleteTitleContent(previous_temporary_title_id);

  if (!ImportWAD(ios, wad))
    return false;

  // Keep track of the title ID so this title can be removed to make room for any future install.
  // We use the same mechanism as the System Menu for temporary SD card title data.
  if (!has_another_version && install_type == InstallType::Temporary)
    tid_entry->SetData<u64>(Common::swap64(title_id));
  else
    tid_entry->SetData<u64>(0);

  return true;
}
#ifndef ANDROID
bool InstallWAD(const std::string& wad_path)
{
  std::unique_ptr<DiscIO::VolumeWAD> wad = DiscIO::CreateWAD(wad_path);
  if (!wad)
    return false;

  IOS::HLE::Kernel ios;
  return InstallWAD(ios, *wad, InstallType::Permanent);
}

bool UninstallTitle(u64 title_id)
{
  IOS::HLE::Kernel ios;
  return ios.GetES()->DeleteTitleContent(title_id) == IOS::HLE::IPC_SUCCESS;
}

bool IsTitleInstalled(u64 title_id)
{
  IOS::HLE::Kernel ios;
  const auto entries = ios.GetFS()->ReadDirectory(0, 0, Common::GetTitleContentPath(title_id));

  if (!entries)
    return false;

  // Since this isn't IOS and we only need a simple way to figure out if a title is installed,
  // we make the (reasonable) assumption that having more than just the TMD in the content
  // directory means that the title is installed.
  return std::any_of(entries->begin(), entries->end(),
                     [](const std::string& file) { return file != "title.tmd"; });
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
    static const std::map<DiscIO::Region, std::string> regions = {{DiscIO::Region::NTSC_J, "JPN"},
                                                                  {DiscIO::Region::NTSC_U, "USA"},
                                                                  {DiscIO::Region::PAL, "EUR"},
                                                                  {DiscIO::Region::NTSC_K, "KOR"},
                                                                  {DiscIO::Region::Unknown, "EUR"}};
    return regions.at(region);
  }
  return "";
}

std::string SystemUpdater::GetDeviceId()
{
  u32 ios_device_id;
  if (m_ios.GetES()->GetDeviceId(&ios_device_id) < 0)
    return "";
  return std::to_string((u64(1) << 32) | ios_device_id);
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
  ASSERT(result);

  // Nintendo does not really care about the device ID or verify that we *are* that device,
  // as long as it is a valid Wii device ID.
  const std::string device_id = GetDeviceId();
  ASSERT(doc.select_node("//DeviceId").node().text().set(device_id.c_str()));

  // Write the correct device region.
  const std::string region = m_requested_region.empty() ? GetDeviceRegion() : m_requested_region;
  ASSERT(doc.select_node("//RegionId").node().text().set(region.c_str()));

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
  const std::string url = (title.version == 0) ?
                              fmt::format("{}/{:016x}/tmd", prefix_url, title.id) :
                              fmt::format("{}/{:016x}/tmd.{}", prefix_url, title.id, title.version);
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
  const std::string url = fmt::format("{}/{:016x}/cetk", prefix_url, title.id);
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
  const std::string url = fmt::format("{}/{:016x}/{:08x}", prefix_url, title.id, cid);
  return m_http.Get(url);
}

class DiscSystemUpdater final : public SystemUpdater
{
public:
  DiscSystemUpdater(UpdateCallback update_callback, const std::string& image_path)
      : m_update_callback{std::move(update_callback)}, m_volume{DiscIO::CreateDisc(image_path)}
  {
  }
  UpdateResult DoDiscUpdate();

private:
#pragma pack(push, 1)
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
    u16 unused1[3];
    char name[0x40];
    char info[0x40];
    u8 unused2[0x120];
  };
  static_assert(sizeof(Entry) == 512, "Wrong size");
#pragma pack(pop)

  UpdateResult UpdateFromManifest(std::string_view manifest_name);
  UpdateResult ProcessEntry(u32 type, std::bitset<32> attrs, const TitleInfo& title,
                            std::string_view path);

  UpdateCallback m_update_callback;
  std::unique_ptr<DiscIO::VolumeDisc> m_volume;
  DiscIO::Partition m_partition;
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

  m_partition = *update_partition;

  return UpdateFromManifest("__update.inf");
}

UpdateResult DiscSystemUpdater::UpdateFromManifest(std::string_view manifest_name)
{
  const DiscIO::FileSystem* disc_fs = m_volume->GetFileSystem(m_partition);
  if (!disc_fs)
  {
    ERROR_LOG(CORE, "Could not read the update partition file system");
    return UpdateResult::DiscReadFailed;
  }

  const std::unique_ptr<DiscIO::FileInfo> update_manifest = disc_fs->FindFileInfo(manifest_name);
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
    if (entry.size() != DiscIO::ReadFile(*m_volume, m_partition, update_manifest.get(),
                                         entry.data(), entry.size(), offset))
    {
      ERROR_LOG(CORE, "Failed to read update information from update manifest");
      return UpdateResult::DiscReadFailed;
    }

    const u32 type = Common::swap32(entry.data() + offsetof(Entry, type));
    const std::bitset<32> attrs = Common::swap32(entry.data() + offsetof(Entry, attribute));
    const u64 title_id = Common::swap64(entry.data() + offsetof(Entry, title_id));
    const u16 title_version = Common::swap16(entry.data() + offsetof(Entry, title_version));
    const char* path_pointer = reinterpret_cast<const char*>(entry.data() + offsetof(Entry, path));
    const std::string_view path{path_pointer, strnlen(path_pointer, sizeof(Entry::path))};

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
                                             const TitleInfo& title, std::string_view path)
{
  // Skip any unknown type and boot2 updates (for now).
  if (type != 2 && type != 3 && type != 6 && type != 7)
    return UpdateResult::AlreadyUpToDate;

  const IOS::ES::TMDReader tmd = m_ios.GetES()->FindInstalledTMD(title.id);
  const IOS::ES::TicketReader ticket = m_ios.GetES()->FindSignedTicket(title.id);

  // Optional titles can be skipped if the ticket is present, even when the title isn't installed.
  if (attrs.test(16) && ticket.IsValid())
    return UpdateResult::AlreadyUpToDate;

  // Otherwise, the title is only skipped if it is installed, its ticket is imported,
  // and the installed version is new enough. No further checks unlike the online updater.
  if (tmd.IsValid() && tmd.GetTitleVersion() >= title.version)
    return UpdateResult::AlreadyUpToDate;

  // Import the WAD.
  auto blob = DiscIO::VolumeFileBlobReader::Create(*m_volume, m_partition, path);
  if (!blob)
  {
    ERROR_LOG(CORE, "Could not find %s", std::string(path).c_str());
    return UpdateResult::DiscReadFailed;
  }
  const DiscIO::VolumeWAD wad{std::move(blob)};
  return ImportWAD(m_ios, wad) ? UpdateResult::Succeeded : UpdateResult::ImportFailed;
}

UpdateResult DoOnlineUpdate(UpdateCallback update_callback, const std::string& region)
{
  OnlineSystemUpdater updater{std::move(update_callback), region};
  return updater.DoOnlineUpdate();
}

UpdateResult DoDiscUpdate(UpdateCallback update_callback, const std::string& image_path)
{
  DiscSystemUpdater updater{std::move(update_callback), image_path};
  return updater.DoDiscUpdate();
}

static NANDCheckResult CheckNAND(IOS::HLE::Kernel& ios, bool repair)
{
  NANDCheckResult result;
  const auto es = ios.GetES();

  // Check for NANDs that were used with old Dolphin versions.
  const std::string sys_replace_path =
      Common::RootUserPath(Common::FROM_CONFIGURED_ROOT) + "/sys/replace";
  if (File::Exists(sys_replace_path))
  {
    ERROR_LOG(CORE, "CheckNAND: NAND was used with old versions, so it is likely to be damaged");
    if (repair)
      File::Delete(sys_replace_path);
    else
      result.bad = true;
  }

  for (const u64 title_id : es->GetInstalledTitles())
  {
    const std::string title_dir = Common::GetTitlePath(title_id, Common::FROM_CONFIGURED_ROOT);
    const std::string content_dir = title_dir + "/content";
    const std::string data_dir = title_dir + "/data";

    // Check for missing title sub directories.
    for (const std::string& dir : {content_dir, data_dir})
    {
      if (File::IsDirectory(dir))
        continue;

      ERROR_LOG(CORE, "CheckNAND: Missing dir %s for title %016" PRIx64, dir.c_str(), title_id);
      if (repair)
        File::CreateDir(dir);
      else
        result.bad = true;
    }

    // Check for incomplete title installs (missing ticket, TMD or contents).
    const auto ticket = es->FindSignedTicket(title_id);
    if (!IOS::ES::IsDiscTitle(title_id) && !ticket.IsValid())
    {
      ERROR_LOG(CORE, "CheckNAND: Missing ticket for title %016" PRIx64, title_id);
      result.titles_to_remove.insert(title_id);
      if (repair)
        File::DeleteDirRecursively(title_dir);
      else
        result.bad = true;
    }

    const auto tmd = es->FindInstalledTMD(title_id);
    if (!tmd.IsValid())
    {
      if (File::ScanDirectoryTree(content_dir, false).children.empty())
      {
        WARN_LOG(CORE, "CheckNAND: Missing TMD for title %016" PRIx64, title_id);
      }
      else
      {
        ERROR_LOG(CORE, "CheckNAND: Missing TMD for title %016" PRIx64, title_id);
        result.titles_to_remove.insert(title_id);
        if (repair)
          File::DeleteDirRecursively(title_dir);
        else
          result.bad = true;
      }
      // Further checks require the TMD to be valid.
      continue;
    }

    const auto installed_contents = es->GetStoredContentsFromTMD(tmd);
    const bool is_installed = std::any_of(installed_contents.begin(), installed_contents.end(),
                                          [](const auto& content) { return !content.IsShared(); });

    if (is_installed && installed_contents != tmd.GetContents() &&
        (tmd.GetTitleFlags() & IOS::ES::TitleFlags::TITLE_TYPE_DATA) == 0)
    {
      ERROR_LOG(CORE, "CheckNAND: Missing contents for title %016" PRIx64, title_id);
      result.titles_to_remove.insert(title_id);
      if (repair)
        File::DeleteDirRecursively(title_dir);
      else
        result.bad = true;
    }
  }

  return result;
}

NANDCheckResult CheckNAND(IOS::HLE::Kernel& ios)
{
  return CheckNAND(ios, false);
}

bool RepairNAND(IOS::HLE::Kernel& ios)
{
  return !CheckNAND(ios, true).bad;
}
#endif // ANDROID
}  // namespace WiiUtils
