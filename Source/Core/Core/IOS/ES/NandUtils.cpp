// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/ES/NandUtils.h"

namespace IOS
{
namespace ES
{
static TMDReader FindTMD(u64 title_id, const std::string& tmd_path)
{
  File::IOFile file(tmd_path, "rb");
  if (!file)
    return {};

  std::vector<u8> tmd_bytes(file.GetSize());
  if (!file.ReadBytes(tmd_bytes.data(), tmd_bytes.size()))
    return {};

  return TMDReader{std::move(tmd_bytes)};
}

TMDReader FindImportTMD(u64 title_id)
{
  return FindTMD(title_id, Common::GetImportTitlePath(title_id) + "/content/title.tmd");
}

TMDReader FindInstalledTMD(u64 title_id)
{
  return FindTMD(title_id, Common::GetTMDFileName(title_id, Common::FROM_SESSION_ROOT));
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

  return title_ids;
}

std::vector<u64> GetInstalledTitles()
{
  return GetTitlesInTitleOrImport(Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/title");
}

std::vector<u64> GetTitleImports()
{
  return GetTitlesInTitleOrImport(Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/import");
}

std::vector<u64> GetTitlesWithTickets()
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

std::vector<Content> GetStoredContentsFromTMD(const TMDReader& tmd)
{
  if (!tmd.IsValid())
    return {};

  const IOS::ES::SharedContentMap shared{Common::FROM_SESSION_ROOT};
  const std::vector<Content> contents = tmd.GetContents();

  std::vector<Content> stored_contents;

  std::copy_if(contents.begin(), contents.end(), std::back_inserter(stored_contents),
               [&tmd, &shared](const auto& content) {
                 if (content.IsShared())
                 {
                   const std::string path = shared.GetFilenameFromSHA1(content.sha1);
                   return path != "unk" && File::Exists(path);
                 }
                 return File::Exists(
                     Common::GetTitleContentPath(tmd.GetTitleId(), Common::FROM_SESSION_ROOT) +
                     StringFromFormat("%08x.app", content.id));
               });

  return stored_contents;
}
}  // namespace ES
}  // namespace IOS
