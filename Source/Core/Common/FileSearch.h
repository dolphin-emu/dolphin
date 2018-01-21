// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

namespace Common
{
// Callers can pass empty "exts" to indicate they want all files + directories in results
// Otherwise, only files matching the extensions are returned
std::vector<std::string> DoFileSearch(const std::vector<std::string>& directories,
                                      const std::vector<std::string>& exts = {},
                                      bool recursive = false);
}  // namespace Common
