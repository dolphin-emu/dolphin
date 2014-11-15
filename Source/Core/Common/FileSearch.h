// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

std::vector<std::string> DoFileSearch(const std::vector<std::string>& _rSearchStrings, const std::vector<std::string>& _rDirectories, bool recursive = false);

