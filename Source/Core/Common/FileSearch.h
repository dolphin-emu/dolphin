// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

std::vector<std::string> DoFileSearch(const std::vector<std::string>& exts, const std::vector<std::string>& directories, bool recursive = false);
std::vector<std::string> FindSubdirectories(const std::vector<std::string>& directories, bool recursive);
