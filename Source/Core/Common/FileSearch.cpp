// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/FileSearch.h"

#include <algorithm>
#include <cstring>
#include <functional>

#include "Common/CommonPaths.h"
#include "Common/DirectoryIterator.h"

#ifdef _WIN32
#include <Windows.h>
#include "Common/StringUtil.h"
#endif

namespace Common
{
std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  bool accept_all = exts.empty();

#ifdef _WIN32
  std::vector<std::wstring> native_exts;
  for (const std::string& ext : exts)
    native_exts.push_back(UTF8ToTStr(ext));

  // N.B. This avoids doing any copies
  auto ext_matches = [&native_exts](const wchar_t* name) {
    return std::any_of(native_exts.cbegin(), native_exts.cend(), [&name](const std::wstring& ext) {
      const int compare_length = static_cast<int>(ext.length());
      const size_t name_length = wcslen(name);
      return name_length >= compare_length &&
             CompareStringOrdinal(&name[name_length - compare_length], compare_length, ext.c_str(),
                                  compare_length, TRUE) == CSTR_EQUAL;
    });
  };
#else
  // N.B. This avoids doing any copies
  auto ext_matches = [&exts](const char* name) {
    return std::any_of(exts.cbegin(), exts.cend(), [&name](const std::string& ext) {
      const size_t name_length = strlen(name);
      return name_length >= ext.length() &&
             strcasecmp(name + name_length - ext.length(), ext.c_str()) == 0;
    });
  };
#endif

  std::vector<std::string> result;

#ifdef _WIN32
  constexpr wchar_t directory_separator = L'/';
#else
  constexpr char directory_separator = '/';
#endif

  std::function<void(const DirectoryEntry::StringType& path)> do_directory;
  do_directory = [&](const DirectoryEntry::StringType& path) {
    for (const DirectoryEntry& entry : Directory<DirectoryEntry::StringType>(path))
    {
      // For performance, skip calling IsDirectory if we won't be using the value
      const bool is_directory = (!accept_all || recursive) ? entry.IsDirectory() : false;
      const DirectoryEntry::CharType* const child_name = entry.GetName();

      if (accept_all || (!is_directory && ext_matches(child_name)))
        result.emplace_back(ToUTF8(path + directory_separator + child_name));

      if (recursive && is_directory)
        do_directory(path + directory_separator + child_name);

      // TODO: We could get better performance in the (currently unused?) case
      // (accept_all && recursive) by not computing (path + directory_separator + child_name)
      // twice. We shouldn't compute it if the result won't be used, though.
    }
  };

  for (const auto& directory : directories)
    do_directory(ToOSEncoding(directory));

  // Remove duplicates (occurring because caller gave e.g. duplicate or overlapping directories -
  // not because the scanning code returns duplicates). Also note that this pathname-based
  // uniqueness isn't as thorough as std::filesystem::equivalent.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());

#ifdef _WIN32
  // Dolphin expects to be able to use / (DIR_SEP_CHR) everywhere, but Windows uses \.
  for (std::string& path : result)
    std::replace(path.begin(), path.end(), '\\', DIR_SEP_CHR);
#endif

  return result;
}

}  // namespace Common
