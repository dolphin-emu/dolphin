// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FileSearch.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iterator>
#include <system_error>

#include "Common/CommonPaths.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#ifdef _MSC_VER
#include <Windows.h>
#else
#ifdef ANDROID
#include "jni/AndroidCommon/AndroidCommon.h"
#endif

#include <cstring>
#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#endif

namespace fs = std::filesystem;

namespace Common
{
std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts, bool recursive)
{
  const bool accept_all = exts.empty();

  std::vector<fs::path> native_exts;
  for (const auto& ext : exts)
    native_exts.push_back(StringToPath(ext));

  // N.B. This avoids doing any copies
  auto ext_matches = [&native_exts](const fs::path& path) {
    const std::basic_string_view<fs::path::value_type> native_path = path.native();
    return std::any_of(native_exts.cbegin(), native_exts.cend(), [&native_path](const auto& ext) {
      const auto compare_len = ext.native().length();
      if (native_path.length() < compare_len)
        return false;
      const auto substr_to_compare = native_path.substr(native_path.length() - compare_len);
#ifdef _WIN32
      return CompareStringOrdinal(substr_to_compare.data(), static_cast<int>(compare_len),
                                  ext.c_str(), static_cast<int>(compare_len), TRUE) == CSTR_EQUAL;
#else
      return strncasecmp(substr_to_compare.data(), ext.c_str(), compare_len) == 0;
#endif
    });
  };

  std::vector<std::string> result;
  auto add_filtered = [&](const fs::directory_entry& entry) {
    auto& path = entry.path();
    if (accept_all || (!entry.is_directory() && ext_matches(path)))
      result.emplace_back(PathToString(path));
  };
  for (const auto& directory : directories)
  {
#ifdef ANDROID
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
      fs::path directory_path = StringToPath(directory);
      std::error_code error;
      if (recursive)
      {
        for (auto it = fs::recursive_directory_iterator(std::move(directory_path), error);
             it != fs::recursive_directory_iterator(); it.increment(error))
          add_filtered(*it);
      }
      else
      {
        for (auto it = fs::directory_iterator(std::move(directory_path), error);
             it != fs::directory_iterator(); it.increment(error))
          add_filtered(*it);
      }
      if (error)
        ERROR_LOG_FMT(COMMON, "{} error on {}: {}", __func__, directory, error.message());
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

}  // namespace Common
