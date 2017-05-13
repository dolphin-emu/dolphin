// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "Core/Config/Config.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"

namespace ConfigLoaders
{
const static std::vector<Config::ConfigLocation> s_setting_saveable{};

bool IsSettingSaveable(const Config::ConfigLocation& config_location)
{
  return std::find(s_setting_saveable.begin(), s_setting_saveable.end(), config_location) !=
         s_setting_saveable.end();
}
}  // namespace ConfigLoader
