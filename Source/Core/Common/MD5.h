// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>

namespace MD5
{
std::string MD5Sum(const std::string& file_name, std::function<bool(int)> progress);
}