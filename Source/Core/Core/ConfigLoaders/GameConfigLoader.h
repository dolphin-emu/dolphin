// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace Config
{
class ConfigLayerLoader;
}

namespace ConfigLoaders
{
std::vector<std::string> GetGameIniFilenames(std::string_view id, std::optional<u16> revision);

std::unique_ptr<Config::ConfigLayerLoader> GenerateGlobalGameConfigLoader(const std::string& id,
                                                                          u16 revision);
std::unique_ptr<Config::ConfigLayerLoader> GenerateLocalGameConfigLoader(const std::string& id,
                                                                         u16 revision);
}  // namespace ConfigLoaders
