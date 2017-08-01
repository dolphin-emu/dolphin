// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Config
{
class ConfigLayerLoader;
class Layer;
}

namespace ConfigLoaders
{
void SaveToSYSCONF(Config::Layer* layer);
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader();
}
