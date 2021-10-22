// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

namespace MD5
{
std::string MD5Sum(const std::string& file_name, std::function<bool(int)> progress);
}
