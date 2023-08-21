// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
