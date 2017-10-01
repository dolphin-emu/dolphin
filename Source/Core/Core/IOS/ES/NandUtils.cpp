// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cctype>
#include <cinttypes>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
static IOS::ES::TMDReader FindTMD(u64 title_id, const std::string& tmd_path)
{
  File::IOFile file(tmd_path, "rb");
  if (!file)
    return {};

  std::vector<u8> tmd_bytes(file.GetSize());
  if (!file.ReadBytes(tmd_bytes.data(), tmd_bytes.size()))
    return {};

  return IOS::ES::TMDReader{std::move(tmd_bytes)};
}

IOS::ES::TMDReader ES::FindImportTMD(u64 title_id) const
{
  return FindTMD(title_id, Common::GetImportTitlePath(title_id) + "/content/title.tmd");
}

IOS::ES::TMDReader ES::FindInstalledTMD(u64 title_id) const
{
  return FindTMD(title_id, Common::GetTMDFileName(title_id, Common::FROM_SESSION_ROOT));
}

IOS::ES::TicketReader ES::FindSignedTicket(u64 title_id) const
{
  const std::string path = Common::GetTicketFileName(title_id, Common::FROM_SESSION_ROOT);
  File::IOFile ticket_file(path, "rb");
  if (!ticket_file)
    return {};

  std::vector<u8> signed_ticket(ticket_file.GetSize());
  if (!ticket_file.ReadBytes(signed_ticket.data(), signed_ticket.size()))
    return {};

  return IOS::ES::TicketReader{std::move(signed_ticket)};
}

static bool IsValidPartOfTitleID(const std::string& string)
{
  if (string.length() != 8)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto character) { return std::isxdigit(character) != 0; });
}

static std::vector<u64> GetTitlesInTitleOrImport(const std::string& titles_dir)
{
  if (!File::IsDirectory(titles_dir))
  {
    ERROR_LOG(IOS_ES, "%s is not a directory", titles_dir.c_str());
    return {};
  }

  std::vector<u64> title_ids;

  // The /title and /import directories contain one directory per title type, and each of them has
  // a directory per title (where the name is the low 32 bits of the title ID in %08x format).
  const auto entries = File::ScanDirectoryTree(titles_dir, true);
  for (const File::FSTEntry& title_type : entries.children)
  {
    if (!title_type.isDirectory || !IsValidPartOfTitleID(title_type.virtualName))
      continue;

    if (title_type.children.empty())
      continue;

    for (const File::FSTEntry& title_identifier : title_type.children)
    {
      if (!title_identifier.isDirectory || !IsValidPartOfTitleID(title_identifier.virtualName))
        continue;

      const u32 type = std::stoul(title_type.virtualName, nullptr, 16);
      const u32 identifier = std::stoul(title_identifier.virtualName, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  // On a real Wii, the title list is not in any particular order. However, because of how
  // the flash filesystem works, titles such as 1-2 are *never* in the first position.
  // We must keep this behaviour, or some versions of the System Menu may break.

  std::sort(title_ids.begin(), title_ids.end(), std::greater<>());

  return title_ids;
}

std::vector<u64> ES::GetInstalledTitles() const
{
  return GetTitlesInTitleOrImport(Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/title");
}

std::vector<u64> ES::GetTitleImports() const
{
  return GetTitlesInTitleOrImport(Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/import");
}

std::vector<u64> ES::GetTitlesWithTickets() const
{
  const std::string tickets_dir = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/ticket";
  if (!File::IsDirectory(tickets_dir))
  {
    ERROR_LOG(IOS_ES, "/ticket is not a directory");
    return {};
  }

  std::vector<u64> title_ids;

  // The /ticket directory contains one directory per title type, and each of them contains
  // one ticket per title (where the name is the low 32 bits of the title ID in %08x format).
  const auto entries = File::ScanDirectoryTree(tickets_dir, true);
  for (const File::FSTEntry& title_type : entries.children)
  {
    if (!title_type.isDirectory || !IsValidPartOfTitleID(title_type.virtualName))
      continue;

    if (title_type.children.empty())
      continue;

    for (const File::FSTEntry& ticket : title_type.children)
    {
      const std::string name_without_ext = ticket.virtualName.substr(0, 8);
      if (ticket.isDirectory || !IsValidPartOfTitleID(name_without_ext) ||
          name_without_ext + ".tik" != ticket.virtualName)
      {
        continue;
      }

      const u32 type = std::stoul(title_type.virtualName, nullptr, 16);
      const u32 identifier = std::stoul(name_without_ext, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  return title_ids;
}

std::vector<IOS::ES::Content> ES::GetStoredContentsFromTMD(const IOS::ES::TMDReader& tmd) const
{
  if (!tmd.IsValid())
    return {};

  const IOS::ES::SharedContentMap map{Common::FROM_SESSION_ROOT};
  const std::vector<IOS::ES::Content> contents = tmd.GetContents();

  std::vector<IOS::ES::Content> stored_contents;

  std::copy_if(contents.begin(), contents.end(), std::back_inserter(stored_contents),
               [this, &tmd, &map](const IOS::ES::Content& content) {
                 const std::string path = GetContentPath(tmd.GetTitleId(), content, map);
                 return !path.empty() && File::Exists(path);
               });

  return stored_contents;
}

u32 ES::GetSharedContentsCount() const
{
  const std::string shared1_path = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/shared1";
  const auto entries = File::ScanDirectoryTree(shared1_path, false);
  return static_cast<u32>(
      std::count_if(entries.children.begin(), entries.children.end(), [](const auto& entry) {
        return !entry.isDirectory && entry.virtualName.size() == 12 &&
               entry.virtualName.compare(8, 4, ".app") == 0;
      }));
}

std::vector<std::array<u8, 20>> ES::GetSharedContents() const
{
  const IOS::ES::SharedContentMap map{Common::FROM_SESSION_ROOT};
  return map.GetHashes();
}

bool ES::InitImport(u64 title_id)
{
  const std::string content_dir = Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT);
  const std::string data_dir = Common::GetTitleDataPath(title_id, Common::FROM_SESSION_ROOT);
  for (const auto& dir : {content_dir, data_dir})
  {
    if (!File::IsDirectory(dir) && !File::CreateFullPath(dir) && !File::CreateDir(dir))
    {
      ERROR_LOG(IOS_ES, "InitImport: Failed to create title dirs for %016" PRIx64, title_id);
      return false;
    }
  }

  IOS::ES::UIDSys uid_sys{Common::FROM_CONFIGURED_ROOT};
  uid_sys.GetOrInsertUIDForTitle(title_id);

  // IOS moves the title content directory to /import if the TMD exists during an import.
  if (File::Exists(Common::GetTMDFileName(title_id, Common::FROM_SESSION_ROOT)))
  {
    const std::string import_content_dir = Common::GetImportTitlePath(title_id) + "/content";
    File::CreateFullPath(import_content_dir);
    if (!File::Rename(content_dir, import_content_dir))
    {
      ERROR_LOG(IOS_ES, "InitImport: Failed to move content dir for %016" PRIx64, title_id);
      return false;
    }
  }

  return true;
}

bool ES::FinishImport(const IOS::ES::TMDReader& tmd)
{
  const u64 title_id = tmd.GetTitleId();
  const std::string import_content_dir = Common::GetImportTitlePath(title_id) + "/content";

  // Remove everything not listed in the TMD.
  std::unordered_set<std::string> expected_entries = {"title.tmd"};
  for (const auto& content_info : tmd.GetContents())
    expected_entries.insert(StringFromFormat("%08x.app", content_info.id));
  const auto entries = File::ScanDirectoryTree(import_content_dir, false);
  for (const File::FSTEntry& entry : entries.children)
  {
    // There should not be any directory in there. Remove it.
    if (entry.isDirectory)
      File::DeleteDirRecursively(entry.physicalName);
    else if (expected_entries.find(entry.virtualName) == expected_entries.end())
      File::Delete(entry.physicalName);
  }

  const std::string content_dir = Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT);
  if (File::IsDirectory(content_dir))
  {
    WARN_LOG(IOS_ES, "FinishImport: %s already exists -- removing", content_dir.c_str());
    File::DeleteDirRecursively(content_dir);
  }
  if (!File::Rename(import_content_dir, content_dir))
  {
    ERROR_LOG(IOS_ES, "FinishImport: Failed to rename import directory to %s", content_dir.c_str());
    return false;
  }
  return true;
}

bool ES::WriteImportTMD(const IOS::ES::TMDReader& tmd)
{
  const std::string tmd_path = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/tmp/title.tmd";
  File::CreateFullPath(tmd_path);

  {
    File::IOFile file(tmd_path, "wb");
    if (!file.WriteBytes(tmd.GetBytes().data(), tmd.GetBytes().size()))
      return false;
  }

  const std::string dest = Common::GetImportTitlePath(tmd.GetTitleId()) + "/content/title.tmd";
  return File::Rename(tmd_path, dest);
}

void ES::FinishStaleImport(u64 title_id)
{
  const auto import_tmd = FindImportTMD(title_id);
  if (!import_tmd.IsValid())
    File::DeleteDirRecursively(Common::GetImportTitlePath(title_id) + "/content");
  else
    FinishImport(import_tmd);
}

void ES::FinishAllStaleImports()
{
  const std::vector<u64> titles = GetTitleImports();
  for (const u64& title_id : titles)
    FinishStaleImport(title_id);

  const std::string import_dir = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/import";
  File::DeleteDirRecursively(import_dir);
  File::CreateDir(import_dir);
}

std::string ES::GetContentPath(const u64 title_id, const IOS::ES::Content& content,
                               const IOS::ES::SharedContentMap& content_map) const
{
  if (content.IsShared())
    return content_map.GetFilenameFromSHA1(content.sha1).value_or("");

  return Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT) +
         StringFromFormat("%08x.app", content.id);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
