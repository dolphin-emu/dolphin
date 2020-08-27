// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Config
{
class ConfigLayerLoader;
enum class LayerType;
}  // namespace Config

namespace ConfigLoaders
{
void SaveToSYSCONF(Config::LayerType layer);
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader();
}  // namespace ConfigLoaders
