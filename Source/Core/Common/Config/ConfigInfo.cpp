// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Config/ConfigInfo.h"

#include <cstring>

#include "Common/CommonFuncs.h"

namespace Config
{
bool Location::operator==(const Location& other) const
{
  return system == other.system && strcasecmp(section.c_str(), other.section.c_str()) == 0 &&
         strcasecmp(key.c_str(), other.key.c_str()) == 0;
}

bool Location::operator!=(const Location& other) const
{
  return !(*this == other);
}

bool Location::operator<(const Location& other) const
{
  if (system != other.system)
    return system < other.system;

  const int section_compare = strcasecmp(section.c_str(), other.section.c_str());
  if (section_compare != 0)
    return section_compare < 0;

  const int key_compare = strcasecmp(key.c_str(), other.key.c_str());
  return key_compare < 0;
}
}  // namespace Config
