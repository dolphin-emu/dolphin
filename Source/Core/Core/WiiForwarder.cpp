// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/WiiForwarder.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace WiiForwarder
{
// --- Pending forwarder boot state ---

static std::mutex s_pending_boot_mutex;
static std::string s_pending_boot_path;

void SetPendingForwarderBoot(const std::string& disc_image_path)
{
  std::lock_guard lock(s_pending_boot_mutex);
  s_pending_boot_path = disc_image_path;
}

std::string ConsumePendingForwarderBoot()
{
  std::lock_guard lock(s_pending_boot_mutex);
  std::string path = std::move(s_pending_boot_path);
  s_pending_boot_path.clear();
  return path;
}

bool HasPendingForwarderBoot()
{
  std::lock_guard lock(s_pending_boot_mutex);
  return !s_pending_boot_path.empty();
}

// --- Mapping file I/O ---

std::string GetForwarderMappingFilePath()
{
  return File::GetUserPath(D_WIIROOT_IDX) + "dolphin_forwarders.ini";
}

static Common::IniFile LoadMappingFile()
{
  Common::IniFile ini;
  ini.Load(GetForwarderMappingFilePath());
  return ini;
}

static bool SaveMappingFile(Common::IniFile& ini)
{
  return ini.Save(GetForwarderMappingFilePath());
}

// --- Title ID generation ---

u64 GenerateForwarderTitleID(const std::string& game_id)
{
  // Hash the game ID to produce a deterministic lower-32 value.
  // Use first 4 bytes of SHA1 XOR'd with our magic to avoid collisions with real channels.
  const auto hash = Common::SHA1::CalculateDigest(
      reinterpret_cast<const u8*>(game_id.data()), game_id.size());
  u32 low32 = 0;
  low32 |= static_cast<u32>(hash[0]) << 24;
  low32 |= static_cast<u32>(hash[1]) << 16;
  low32 |= static_cast<u32>(hash[2]) << 8;
  low32 |= static_cast<u32>(hash[3]);
  low32 ^= FORWARDER_MAGIC;

  return (static_cast<u64>(FORWARDER_TITLE_TYPE) << 32) | low32;
}

bool IsForwarderTitle(u64 title_id)
{
  const auto forwarders = GetInstalledForwarders();
  return forwarders.find(title_id) != forwarders.end();
}

std::optional<std::string> GetDiscImagePath(u64 forwarder_title_id)
{
  const auto forwarders = GetInstalledForwarders();
  auto it = forwarders.find(forwarder_title_id);
  if (it != forwarders.end())
    return it->second;
  return std::nullopt;
}

bool IsForwarderInstalled(const std::string& disc_image_path)
{
  const auto forwarders = GetInstalledForwarders();
  for (const auto& [tid, path] : forwarders)
  {
    if (path == disc_image_path)
      return true;
  }
  return false;
}

std::map<u64, std::string> GetInstalledForwarders()
{
  std::map<u64, std::string> result;
  Common::IniFile ini = LoadMappingFile();
  const Common::IniFile::Section* section = ini.GetSection("Forwarders");
  if (!section)
    return result;

  for (const auto& [key, value] : section->GetValues())
  {
    u64 title_id = 0;
    if (TryParse(std::string("0x") + key, &title_id))
      result[title_id] = value;
  }
  return result;
}

// --- Synthetic TMD/Ticket/Content builders ---

// Build a fakesigned TMD blob for a forwarder channel.
static std::vector<u8> BuildForwarderTMD(u64 forwarder_title_id, u64 ios_id,
                                          const std::vector<u8>& content_data)
{
  // TMD = TMDHeader + 1 Content entry
  const size_t tmd_size = sizeof(IOS::ES::TMDHeader) + sizeof(IOS::ES::Content);
  std::vector<u8> tmd_bytes(tmd_size, 0);

  auto* header = reinterpret_cast<IOS::ES::TMDHeader*>(tmd_bytes.data());

  // Set signature type to RSA-2048 (0x00010001 big-endian)
  const u32 sig_type_be = Common::swap32(0x00010001u);
  std::memcpy(&header->signature.type, &sig_type_be, sizeof(sig_type_be));
  // Issuer
  std::strncpy(header->signature.issuer, "Root-CA00000001-CP00000004",
               sizeof(header->signature.issuer) - 1);

  // TMD fields
  header->tmd_version = 0;
  header->ios_id = Common::swap64(ios_id);
  header->title_id = Common::swap64(forwarder_title_id);
  header->title_flags = Common::swap32(IOS::ES::TITLE_TYPE_DEFAULT);
  header->group_id = 0;
  header->region = Common::swap16(static_cast<u16>(3));  // Region-free
  header->access_rights = 0;
  header->title_version = Common::swap16(static_cast<u16>(1));
  header->num_contents = Common::swap16(static_cast<u16>(1));
  header->boot_index = Common::swap16(static_cast<u16>(0));

  // Content entry
  auto* content = reinterpret_cast<IOS::ES::Content*>(
      tmd_bytes.data() + sizeof(IOS::ES::TMDHeader));
  content->id = 0;
  content->index = 0;
  content->type = Common::swap16(static_cast<u16>(0x0001));  // Normal content
  content->size = Common::swap64(static_cast<u64>(content_data.size()));

  // SHA1 hash of the content data
  const auto sha1 = Common::SHA1::CalculateDigest(content_data.data(), content_data.size());
  std::copy(sha1.begin(), sha1.end(), content->sha1.begin());

  return tmd_bytes;
}

// Build a fakesigned Ticket blob for a forwarder channel.
static std::vector<u8> BuildForwarderTicket(u64 forwarder_title_id)
{
  std::vector<u8> ticket_bytes(sizeof(IOS::ES::Ticket), 0);
  auto* ticket = reinterpret_cast<IOS::ES::Ticket*>(ticket_bytes.data());

  // Signature type RSA-2048 (0x00010001 big-endian)
  const u32 ticket_sig_type_be = Common::swap32(0x00010001u);
  std::memcpy(&ticket->signature.type, &ticket_sig_type_be, sizeof(ticket_sig_type_be));
  // Issuer
  std::strncpy(ticket->signature.issuer, "Root-CA00000001-XS00000003",
               sizeof(ticket->signature.issuer) - 1);

  ticket->version = 0;
  // Title key: all zeros (we'll encrypt content with zero key)
  std::memset(ticket->title_key, 0, sizeof(ticket->title_key));
  ticket->ticket_id = 0;
  ticket->device_id = 0;
  ticket->title_id = Common::swap64(forwarder_title_id);
  ticket->access_mask = Common::swap16(static_cast<u16>(0xFFFF));
  ticket->ticket_version = 0;
  ticket->common_key_index = 0;

  // Grant access to all contents
  std::memset(ticket->content_access_permissions, 0xFF,
              sizeof(ticket->content_access_permissions));

  return ticket_bytes;
}

// Extract the full opening.bnr from the disc image. This contains the IMET header
// plus the U8 archive with icon.bin, banner.bin, and sound.bin — giving us the
// animated banner, icon, and sound that the Wii Menu displays for channels.
// Falls back to a minimal text-only IMET if extraction fails.
static std::vector<u8> ExtractOpeningBnr(const DiscIO::Volume& volume,
                                          const DiscIO::Partition& partition)
{
  const DiscIO::FileSystem* fs = volume.GetFileSystem(partition);
  if (!fs)
    return {};

  std::unique_ptr<DiscIO::FileInfo> file_info = fs->FindFileInfo("opening.bnr");
  if (!file_info || file_info->IsDirectory())
    return {};

  const u64 file_size = file_info->GetSize();
  if (file_size < 0x600)  // Must be at least IMET header size
    return {};

  std::vector<u8> data(file_size);
  const u64 bytes_read = DiscIO::ReadFile(volume, partition, file_info.get(),
                                           data.data(), file_size);
  if (bytes_read != file_size)
    return {};

  // Verify IMET magic at offset 0x40
  if (data[0x40] != 'I' || data[0x41] != 'M' || data[0x42] != 'E' || data[0x43] != 'T')
    return {};

  return data;
}

// Build a minimal text-only IMET content as fallback when opening.bnr can't be extracted.
static std::vector<u8> BuildMinimalIMET(
    const std::map<DiscIO::Language, std::string>& long_names)
{
  constexpr size_t IMET_SIZE = 0x600;
  constexpr size_t IMET_NAMES_OFFSET = 0x60;
  constexpr size_t NAME_CHARS = 42;
  constexpr size_t NUM_LANGUAGES = 10;

  std::vector<u8> content(IMET_SIZE, 0);

  // IMET magic at 0x40
  content[0x40] = 'I';
  content[0x41] = 'M';
  content[0x42] = 'E';
  content[0x43] = 'T';

  // Fixed size field
  const u32 imet_size_be = Common::swap32(static_cast<u32>(IMET_SIZE));
  std::memcpy(&content[0x44], &imet_size_be, 4);

  // Unknown field = 3
  const u32 three_be = Common::swap32(static_cast<u32>(3));
  std::memcpy(&content[0x48], &three_be, 4);

  const DiscIO::Language lang_order[] = {
      DiscIO::Language::Japanese, DiscIO::Language::English,  DiscIO::Language::German,
      DiscIO::Language::French,   DiscIO::Language::Spanish,  DiscIO::Language::Italian,
      DiscIO::Language::Dutch,    DiscIO::Language::SimplifiedChinese,
      DiscIO::Language::TraditionalChinese, DiscIO::Language::Korean,
  };

  std::string fallback_name = "Disc Game";
  auto en_it = long_names.find(DiscIO::Language::English);
  if (en_it != long_names.end() && !en_it->second.empty())
    fallback_name = en_it->second;
  else if (!long_names.empty())
    fallback_name = long_names.begin()->second;

  for (size_t lang_idx = 0; lang_idx < NUM_LANGUAGES; lang_idx++)
  {
    std::string name = fallback_name;
    auto it = long_names.find(lang_order[lang_idx]);
    if (it != long_names.end() && !it->second.empty())
      name = it->second;

    const auto utf16 = UTF8ToUTF16(name);
    const size_t chars_to_write = std::min(utf16.size(), NAME_CHARS - 1);
    const size_t offset = IMET_NAMES_OFFSET + lang_idx * NAME_CHARS * 2;

    for (size_t i = 0; i < chars_to_write; i++)
    {
      const u16 ch = Common::swap16(static_cast<u16>(utf16[i]));
      std::memcpy(&content[offset + i * 2], &ch, 2);
    }
  }

  return content;
}

// Build the forwarder channel content. Tries to extract the real opening.bnr from
// the disc (with animated banner, icon, and sound). Falls back to a minimal IMET
// with just the game name if extraction fails. Appends the DFWD marker + disc path.
static std::vector<u8> BuildForwarderContent(
    const DiscIO::Volume& volume,
    const DiscIO::Partition& partition,
    const std::map<DiscIO::Language, std::string>& long_names,
    const std::string& disc_image_path)
{
  // Try to get the real opening.bnr with full animated banner
  std::vector<u8> content = ExtractOpeningBnr(volume, partition);

  if (content.empty())
  {
    WARN_LOG_FMT(CORE, "WiiForwarder: Could not extract opening.bnr, using minimal IMET");
    content = BuildMinimalIMET(long_names);
  }
  else
  {
    INFO_LOG_FMT(CORE, "WiiForwarder: Extracted opening.bnr ({} bytes) with full animated banner",
                 content.size());
  }

  // Append DFWD marker + disc image path after the banner content
  const size_t banner_size = content.size();
  const size_t path_section_size = 4 + disc_image_path.size() + 1;  // "DFWD" + path + null
  content.resize(banner_size + path_section_size, 0);

  content[banner_size + 0] = 'D';
  content[banner_size + 1] = 'F';
  content[banner_size + 2] = 'W';
  content[banner_size + 3] = 'D';
  std::memcpy(&content[banner_size + 4], disc_image_path.c_str(), disc_image_path.size() + 1);

  return content;
}

// --- Install/Uninstall ---

bool InstallForwarder(const std::string& disc_image_path, bool silent)
{
  // Open the disc image and extract metadata
  std::unique_ptr<DiscIO::Volume> volume = DiscIO::CreateVolume(disc_image_path);
  if (!volume)
  {
    ERROR_LOG_FMT(CORE, "WiiForwarder: Failed to open disc image: {}", disc_image_path);
    if (!silent)
      PanicAlertFmtT("Failed to open disc image: {0}", disc_image_path);
    return false;
  }

  if (volume->GetVolumeType() != DiscIO::Platform::WiiDisc)
  {
    if (!silent)
      PanicAlertFmtT("Only Wii disc images can be added to the Wii Menu.");
    return false;
  }

  const DiscIO::Partition game_partition = volume->GetGamePartition();
  const std::string game_id = volume->GetGameID(game_partition);
  if (game_id.empty())
  {
    ERROR_LOG_FMT(CORE, "WiiForwarder: Could not read game ID from disc image: {}", disc_image_path);
    if (!silent)
      PanicAlertFmtT("Could not read game ID from disc image.");
    return false;
  }

  // Check if already installed
  if (IsForwarderInstalled(disc_image_path))
  {
    if (!silent)
      PanicAlertFmtT("This game is already added to the Wii Menu.");
    return false;
  }

  // Generate forwarder title ID
  const u64 forwarder_title_id = GenerateForwarderTitleID(game_id);

  // Get game names for banner
  const auto long_names = volume->GetLongNames();

  // Get IOS version from the disc's TMD
  const IOS::ES::TMDReader& disc_tmd = volume->GetTMD(game_partition);
  u64 ios_id = 0x0000000100000038;  // Default to IOS56
  if (disc_tmd.IsValid())
    ios_id = disc_tmd.GetIOSId();

  // Build content with real animated banner from disc (or fallback to minimal IMET)
  const std::vector<u8> content_data =
      BuildForwarderContent(*volume, game_partition, long_names, disc_image_path);

  // Build synthetic TMD and Ticket
  const std::vector<u8> tmd_bytes = BuildForwarderTMD(forwarder_title_id, ios_id, content_data);
  const std::vector<u8> ticket_bytes = BuildForwarderTicket(forwarder_title_id);

  // Write directly to NAND filesystem instead of using ES import APIs,
  // because the ES pipeline expects encrypted content and would garble our
  // unencrypted forwarder data during the decryption step.

  const auto configured_root = Common::FromWhichRoot::Configured;

  // Create title directories
  const std::string title_path = Common::GetTitlePath(forwarder_title_id, configured_root);
  const std::string content_path = Common::GetTitleContentPath(forwarder_title_id, configured_root);
  const std::string data_path = Common::GetTitleDataPath(forwarder_title_id, configured_root);
  File::CreateFullPath(content_path + "/");
  File::CreateFullPath(data_path + "/");

  // Write ticket
  const std::string ticket_path = Common::GetTicketFileName(forwarder_title_id, configured_root);
  File::CreateFullPath(ticket_path);
  if (!File::IOFile(ticket_path, "wb").WriteBytes(ticket_bytes.data(), ticket_bytes.size()))
  {
    ERROR_LOG_FMT(CORE, "WiiForwarder: Failed to write ticket to {}", ticket_path);
    if (!silent)
      PanicAlertFmtT("Failed to install forwarder: could not write ticket.");
    return false;
  }

  // Write TMD
  const std::string tmd_path = Common::GetTMDFileName(forwarder_title_id, configured_root);
  if (!File::IOFile(tmd_path, "wb").WriteBytes(tmd_bytes.data(), tmd_bytes.size()))
  {
    ERROR_LOG_FMT(CORE, "WiiForwarder: Failed to write TMD to {}", tmd_path);
    File::Delete(ticket_path);
    if (!silent)
      PanicAlertFmtT("Failed to install forwarder: could not write TMD.");
    return false;
  }

  // Write content file (00000000.app)
  const std::string app_path = content_path + "/00000000.app";
  if (!File::IOFile(app_path, "wb").WriteBytes(content_data.data(), content_data.size()))
  {
    ERROR_LOG_FMT(CORE, "WiiForwarder: Failed to write content to {}", app_path);
    File::Delete(ticket_path);
    File::Delete(tmd_path);
    if (!silent)
      PanicAlertFmtT("Failed to install forwarder: could not write content.");
    return false;
  }

  // Save mapping to INI file
  Common::IniFile ini = LoadMappingFile();
  Common::IniFile::Section* section = ini.GetOrCreateSection("Forwarders");
  section->Set(fmt::format("{:016x}", forwarder_title_id), disc_image_path);
  SaveMappingFile(ini);

  INFO_LOG_FMT(CORE, "WiiForwarder: Installed forwarder {:016x} for '{}'", forwarder_title_id,
               disc_image_path);
  return true;
}

bool UninstallForwarder(u64 forwarder_title_id)
{
  if (!IsForwarderTitle(forwarder_title_id))
  {
    PanicAlertFmtT("Cannot uninstall: title is not a forwarder channel.");
    return false;
  }

  // Remove from NAND by deleting the title directory and ticket from the host filesystem

  // Delete title directory
  const std::string title_path = Common::GetTitlePath(forwarder_title_id, Common::FromWhichRoot::Configured);
  File::DeleteDirRecursively(title_path);

  // Delete ticket
  const std::string ticket_path = Common::GetTicketFileName(forwarder_title_id, Common::FromWhichRoot::Configured);
  File::Delete(ticket_path);

  // Remove from mapping file
  Common::IniFile ini = LoadMappingFile();
  Common::IniFile::Section* section = ini.GetOrCreateSection("Forwarders");
  section->Delete(fmt::format("{:016x}", forwarder_title_id));
  SaveMappingFile(ini);

  INFO_LOG_FMT(CORE, "WiiForwarder: Uninstalled forwarder {:016x}", forwarder_title_id);
  return true;
}

}  // namespace WiiForwarder
