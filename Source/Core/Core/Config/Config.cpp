// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <tuple>

#include "Core/Config/Config.h"

namespace Config
{
bool ConfigLocation::operator==(const ConfigLocation& other) const
{
  return std::tie(system, section, key) == std::tie(other.system, other.section, other.key);
}

bool ConfigLocation::operator!=(const ConfigLocation& other) const
{
  return !(*this == other);
}

bool ConfigLocation::operator<(const ConfigLocation& other) const
{
  return std::tie(system, section, key) < std::tie(other.system, other.section, other.key);
}

LayerType GetActiveLayerForConfig(const ConfigLocation& config)
{
  for (auto layer : SEARCH_ORDER)
  {
    if (!LayerExists(layer))
      continue;

    if (GetLayer(layer)->Exists(config.system, config.section, config.key))
      return layer;
  }

  // If config is not present in any layer, base layer is considered active.
  return LayerType::Base;
}

}  // namespace Config
