// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Config/Enums.h"

namespace Config
{
struct ConfigLocation
{
  System system;
  std::string section;
  std::string key;

  bool operator==(const ConfigLocation& other) const;
  bool operator!=(const ConfigLocation& other) const;
  bool operator<(const ConfigLocation& other) const;
};
}  // namespace Config
