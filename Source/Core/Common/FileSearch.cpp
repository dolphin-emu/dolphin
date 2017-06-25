// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"

#ifdef _MSC_VER
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define HAS_STD_FILESYSTEM
#else
#include "Common/FileUtil.h"
#endif

namespace Common
{
#ifndef HAS_STD_FILESYSTEM

static std::vector<std::string>
DoFileSearch(const std::vector<std::string>& directories, bool recursive,
             std::function<bool(const std::string& path, bool is_directory)> filter)
{
  std::vector<std::string> result;
  for (const std::string& directory : directories)
  {
    File::FSTEntry top = File::ScanDirectoryTree(directory, recursive);

    std::function<void(File::FSTEntry&)> DoEntry;
    DoEntry = [&](File::FSTEntry& entry) {
      if (filter(entry.physicalName, entry.isDirectory))
        result.push_back(entry.physicalName);
      for (auto& child : entry.children)
        DoEntry(child);
    };
    for (auto& child : top.children)
      DoEntry(child);
  }
  // remove duplicates
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

#else

static std::vector<std::string>
DoFileSearch(const std::vector<std::string>& directories, bool recursive,
             std::function<bool(const std::string& path, bool is_directory)> filter)
{
  std::vector<std::string> result;
  auto add_filtered = [&](const fs::directory_entry& entry) {
    const std::string u8_path = entry.path().u8string();
    if (filter(u8_path, fs::is_directory(entry.path())))
      result.emplace_back(u8_path);
  };
  for (const auto& directory : directories)
  {
    if (recursive)
    {
      // TODO use fs::directory_options::follow_directory_symlink ?
      for (auto& entry : fs::recursive_directory_iterator(fs::path(directory.c_str())))
        add_filtered(entry);
    }
    else
    {
      for (auto& entry : fs::directory_iterator(fs::path(directory.c_str())))
        add_filtered(entry);
    }
  }

  // Remove duplicates (occurring because caller gave e.g. duplicate or overlapping directories -
  // not because std::filesystem returns duplicates). Also note that this pathname-based uniqueness
  // isn't as thorough as std::filesystem::equivalent.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());

  // Dolphin expects to be able to use "/" (DIR_SEP) everywhere. std::filesystem uses the OS
  // separator.
  if (fs::path::preferred_separator != DIR_SEP_CHR)
    for (auto& path : result)
      std::replace(path.begin(), path.end(), '\\', DIR_SEP_CHR);

  return result;
}

#endif

std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories, bool recursive,
                                      const std::vector<std::string>& exts)
{
  const bool accept_all = exts.empty();
  return DoFileSearch(directories, recursive, [&](const std::string& path, bool is_directory) {
    if (accept_all)
      return true;
    if (is_directory)
      return false;
    std::string path_copy = path;
    std::transform(path_copy.begin(), path_copy.end(), path_copy.begin(), ::tolower);
    return std::any_of(exts.begin(), exts.end(), [&](const std::string& ext) {
      return path_copy.length() >= ext.length() &&
             path_copy.compare(path_copy.length() - ext.length(), ext.length(), ext) == 0;
    });
  });
}

}  // namespace Common
