// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Common
{
std::vector<std::string>
DoFileSearch(const std::vector<std::string>& directories, bool recursive = false,
             std::function<bool(const std::string& path, bool is_directory)> predicate = {});
std::vector<std::string>
DoFileSearch(const std::vector<std::string>& directories, bool recursive,
             const std::vector<std::string>& exts,
             std::function<bool(const std::string& path, bool is_directory)> predicate = {});
}  // namespace Common
