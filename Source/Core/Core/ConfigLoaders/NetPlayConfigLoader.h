// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Config
{
class ConfigLayerLoader;
}

namespace NetPlay
{
struct NetSettings;
}

namespace ConfigLoaders
{
std::unique_ptr<Config::ConfigLayerLoader>
GenerateNetPlayConfigLoader(const NetPlay::NetSettings& settings);
}
