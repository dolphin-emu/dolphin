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
enum class WriteBackChangedValues
{
  No,
  Yes,
};

enum class SkipIfControlledByGuest
{
  No,
  Yes,
};

void TransferSYSCONFControlToGuest();
void TransferSYSCONFControlFromGuest(WriteBackChangedValues write_back_changed_values);
void SaveToSYSCONF(Config::LayerType layer,
                   SkipIfControlledByGuest skip = SkipIfControlledByGuest::Yes,
                   std::function<bool(const Config::Location&)> predicate = {});
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader();
}  // namespace ConfigLoaders
