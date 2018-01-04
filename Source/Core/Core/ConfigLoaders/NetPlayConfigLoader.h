// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

struct NetSettings;

namespace Config
{
class ConfigLayerLoader;
}

namespace ConfigLoaders
{
std::unique_ptr<Config::ConfigLayerLoader> GenerateNetPlayConfigLoader(const NetSettings& settings);
}
