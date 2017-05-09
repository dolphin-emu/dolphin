// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Config/Enums.h"

namespace Config
{

template <typename T>
struct SettingInfo
{
  System system;
  const char* section_name;
  const char* key;
  T default_value;
};

template <>
struct SettingInfo<std::string>
{
  System system;
  const char* section_name;
  const char* key;
  const char* default_value;
};
}
