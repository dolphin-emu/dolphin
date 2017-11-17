// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonFuncs.h"
#include "Common/Config/ConfigInfo.h"

namespace Config
{
bool ConfigLocation::operator==(const ConfigLocation& other) const
{
  return system == other.system && strcasecmp(section.c_str(), other.section.c_str()) == 0 &&
         strcasecmp(key.c_str(), other.key.c_str()) == 0;
}

bool ConfigLocation::operator!=(const ConfigLocation& other) const
{
  return !(*this == other);
}

bool ConfigLocation::operator<(const ConfigLocation& other) const
{
  if (system != other.system)
    return system < other.system;

  const int section_compare = strcasecmp(section.c_str(), other.section.c_str());
  if (section_compare != 0)
    return section_compare < 0;

  const int key_compare = strcasecmp(key.c_str(), other.key.c_str());
  return key_compare < 0;
}
}
