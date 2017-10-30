// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <strings.h>

#include "Common/Config/ConfigInfo.h"

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
}
