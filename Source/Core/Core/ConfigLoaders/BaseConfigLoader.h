// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <memory>

namespace Config
{
class ConfigLayerLoader;
enum class LayerType;
struct Location;
}  // namespace Config

namespace ConfigLoaders
{
void SaveToSYSCONF(Config::LayerType layer,
                   std::function<bool(const Config::Location&)> predicate = {});
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader();
}  // namespace ConfigLoaders
