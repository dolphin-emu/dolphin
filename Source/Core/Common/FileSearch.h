// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

namespace Common
{
// Callers can pass empty "exts" to indicate they want all files + directories in results.
// Otherwise, only files matching the extensions are returned.
// T must be std::string or File::Path.
template <class T>
std::vector<T> DoFileSearch(const std::vector<T>& directories,
                            const std::vector<std::string>& exts = {}, bool recursive = false);
}  // namespace Common
