// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace VideoCommon::Assets
{
using AssetMap = std::map<std::string, std::filesystem::path>;
}
