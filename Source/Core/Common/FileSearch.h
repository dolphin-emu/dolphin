// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Common
{
// Callers can pass empty "exts" to indicate they want all files + directories in results
// Otherwise, only files matching the extensions are returned
std::vector<std::string> DoFileSearch(std::span<const std::string_view> directories,
                                      std::span<const std::string_view> exts = {},
                                      bool recursive = false);

inline std::vector<std::string> DoFileSearch(std::span<const std::string_view> directories,
                                             std::string_view ext, bool recursive = false)
{
  return DoFileSearch(directories, std::span(&ext, 1), recursive);
}

inline std::vector<std::string> DoFileSearch(std::string_view directory,
                                             std::span<const std::string_view> exts = {},
                                             bool recursive = false)
{
  return DoFileSearch(std::span(&directory, 1), exts, recursive);
}

inline std::vector<std::string> DoFileSearch(std::string_view directory, std::string_view ext,
                                             bool recursive = false)
{
  return DoFileSearch(std::span(&directory, 1), std::span(&ext, 1), recursive);
}

}  // namespace Common
