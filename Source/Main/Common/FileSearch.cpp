// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"

#ifdef _MSC_VER
#include <Windows.h>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#define HAS_STD_FILESYSTEM
#else
#include <cstring>
#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#endif

namespace Common
{
#ifndef HAS_STD_FILESYSTEM

static std::vector<std::string>
FileSearchWithTest(const std::vector<std::string>& directories, bool recursive,
                   std::function<bool(const File::FSTEntry&)> callback)
{
  std::vector<std::string> result;
  for (const std::string& directory : directories)
  {
    File::FSTEntry top = File::ScanDirectoryTree(directory, recursive);

    std::function<void(File::FSTEntry&)> DoEntry;
    DoEntry = [&](File::FSTEntry& entry) {
      if (callback(entry))
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

std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  bool accept_all = exts.empty();
  return FileSearchWithTest(directories, recursive, [&](const File::FSTEntry& entry) {
    if (accept_all)
      return true;
    if (entry.isDirectory)
      return false;
    return std::any_of(exts.begin(), exts.end(), [&](const std::string& ext) {
      const std::string& name = entry.virtualName;
      return name.length() >= ext.length() &&
             strcasecmp(name.c_str() + name.length() - ext.length(), ext.c_str()) == 0;
    });
  });
}

#else

std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  bool accept_all = exts.empty();

  std::vector<fs::path> native_exts;
  for (const auto& ext : exts)
    native_exts.push_back(fs::u8path(ext));

  // N.B. This avoids doing any copies
  auto ext_matches = [&native_exts](const fs::path& path) {
    const auto& native_path = path.native();
    return std::any_of(native_exts.cbegin(), native_exts.cend(), [&native_path](const auto& ext) {
      // TODO provide cross-platform compat for the comparison function, once more platforms
      // support std::filesystem
      int compare_len = static_cast<int>(ext.native().length());
      return native_path.length() >= compare_len &&
             CompareStringOrdinal(&native_path.c_str()[native_path.length() - compare_len],
                                  compare_len, ext.c_str(), compare_len, TRUE) == CSTR_EQUAL;
    });
  };

  std::vector<std::string> result;
  auto add_filtered = [&](const fs::directory_entry& entry) {
    auto& path = entry.path();
    if (accept_all || (ext_matches(path) && !fs::is_directory(path)))
      result.emplace_back(path.u8string());
  };
  for (const auto& directory : directories)
  {
    if (recursive)
    {
      // TODO use fs::directory_options::follow_directory_symlink ?
      for (auto& entry : fs::recursive_directory_iterator(fs::u8path(directory)))
        add_filtered(entry);
    }
    else
    {
      for (auto& entry : fs::directory_iterator(fs::u8path(directory)))
        add_filtered(entry);
    }
  }

  // Remove duplicates (occurring because caller gave e.g. duplicate or overlapping directories -
  // not because std::filesystem returns duplicates). Also note that this pathname-based uniqueness
  // isn't as thorough as std::filesystem::equivalent.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());

  // Dolphin expects to be able to use "/" (DIR_SEP) everywhere.
  // std::filesystem uses the OS separator.
  constexpr fs::path::value_type os_separator = fs::path::preferred_separator;
  static_assert(os_separator == DIR_SEP_CHR || os_separator == '\\', "Unsupported path separator");
  if (os_separator != DIR_SEP_CHR)
    for (auto& path : result)
      std::replace(path.begin(), path.end(), '\\', DIR_SEP_CHR);

  return result;
}

#endif

}  // namespace Common
