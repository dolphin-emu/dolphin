// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FileSearch.h"

#include <algorithm>
#include <functional>
#include <iterator>

#include "Common/CommonPaths.h"
#include "Common/StringUtil.h"

#ifdef _MSC_VER
#include <Windows.h>
#include <filesystem>
namespace fs = std::filesystem;
#define HAS_STD_FILESYSTEM
#else
#ifdef ANDROID
#include "jni/AndroidCommon/AndroidCommon.h"
#endif

#include <cstring>
#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#endif

namespace Common
{
#ifndef HAS_STD_FILESYSTEM

static void FileSearchWithTest(const std::string& directory, bool recursive,
                               std::vector<std::string>* result_out,
                               std::function<bool(const File::FSTEntry&)> callback)
{
  File::FSTEntry top = File::ScanDirectoryTree(directory, recursive);

  const std::function<void(File::FSTEntry&)> DoEntry = [&](File::FSTEntry& entry) {
    if (callback(entry))
      result_out->push_back(entry.physicalName);
    for (auto& child : entry.children)
      DoEntry(child);
  };

  for (auto& child : top.children)
    DoEntry(child);
}

std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  std::vector<std::string> result;

  bool accept_all = exts.empty();
  const auto callback = [&exts, accept_all](const File::FSTEntry& entry) {
    if (accept_all)
      return true;
    if (entry.isDirectory)
      return false;
    return std::any_of(exts.begin(), exts.end(), [&](const std::string& ext) {
      const std::string& name = entry.virtualName;
      return name.length() >= ext.length() &&
             strcasecmp(name.c_str() + name.length() - ext.length(), ext.c_str()) == 0;
    });
  };

  for (const std::string& directory : directories)
  {
#ifdef ANDROID
    // While File::ScanDirectoryTree (which is called in FileSearchWithTest) does handle Android
    // content correctly, having a specialized implementation of DoFileSearch for Android content
    // provides a much needed performance boost. Also, this specialized implementation will be
    // required if we in the future replace the use of File::ScanDirectoryTree with std::filesystem.
    if (IsPathAndroidContent(directory))
    {
      const std::vector<std::string> partial_result =
          DoFileSearchAndroidContent(directory, exts, recursive);

      result.insert(result.end(), std::make_move_iterator(partial_result.begin()),
                    std::make_move_iterator(partial_result.end()));
    }
    else
#endif
    {
      FileSearchWithTest(directory, recursive, &result, callback);
    }
  }

  // remove duplicates
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

#else

std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  bool accept_all = exts.empty();

  std::vector<fs::path> native_exts;
  for (const auto& ext : exts)
    native_exts.push_back(StringToPath(ext));

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
      result.emplace_back(PathToString(path));
  };
  for (const auto& directory : directories)
  {
    fs::path directory_path = StringToPath(directory);
    if (fs::is_directory(directory_path))  // Can't create iterators for non-existant directories
    {
      if (recursive)
      {
        // TODO use fs::directory_options::follow_directory_symlink ?
        for (auto& entry : fs::recursive_directory_iterator(std::move(directory_path)))
          add_filtered(entry);
      }
      else
      {
        for (auto& entry : fs::directory_iterator(std::move(directory_path)))
          add_filtered(entry);
      }
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
  if constexpr (os_separator != DIR_SEP_CHR)
  {
    for (auto& path : result)
      std::replace(path.begin(), path.end(), '\\', DIR_SEP_CHR);
  }

  return result;
}

#endif

}  // namespace Common
