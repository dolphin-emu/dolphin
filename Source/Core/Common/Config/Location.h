// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Enums.h"

namespace Config
{
struct Location
{
  System system;
  std::string section;
  std::string key;

  bool operator==(const Location& other) const;
  bool operator!=(const Location& other) const;
  bool operator<(const Location& other) const;
};
}  // namespace Config
