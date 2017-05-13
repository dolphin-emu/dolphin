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
}  // namespace Config
