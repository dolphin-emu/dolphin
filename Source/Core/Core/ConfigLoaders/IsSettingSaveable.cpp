// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/IsSettingSaveable.h"

#include <algorithm>
#include <vector>

#include "Common/Config/Enums.h"
#include "Common/Config/Location.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

namespace ConfigLoaders
{
bool IsSettingSaveable(const Config::Location& config_location)
{
  for (Config::System system :
       {Config::System::GFX, Config::System::DualShockUDPClient, Config::System::Logger})
  {
    if (config_location.system == system)
      return true;
  }

  if (config_location.system == Config::System::Main)
  {
    for (const std::string& section :
         {"NetPlay", "General", "Display", "Network", "Analytics", "Android"})
    {
      if (config_location.section == section)
        return true;
    }
  }

  static std::array<const Config::Location*, 16> s_setting_saveable = {
      // Main.Core

      &Config::MAIN_DEFAULT_ISO.GetLocation(),
      &Config::MAIN_MEMCARD_A_PATH.GetLocation(),
      &Config::MAIN_MEMCARD_B_PATH.GetLocation(),
      &Config::MAIN_AUTO_DISC_CHANGE.GetLocation(),
      &Config::MAIN_ALLOW_SD_WRITES.GetLocation(),
      &Config::MAIN_DPL2_DECODER.GetLocation(),
      &Config::MAIN_DPL2_QUALITY.GetLocation(),
      &Config::MAIN_RAM_OVERRIDE_ENABLE.GetLocation(),
      &Config::MAIN_MEM1_SIZE.GetLocation(),
      &Config::MAIN_MEM2_SIZE.GetLocation(),
      &Config::MAIN_GFX_BACKEND.GetLocation(),
      &Config::MAIN_ENABLE_SAVESTATES.GetLocation(),

      // Main.Interface

      &Config::MAIN_USE_PANIC_HANDLERS.GetLocation(),
      &Config::MAIN_OSD_MESSAGES.GetLocation(),

      // Main.Interface

      &Config::MAIN_SKIP_NKIT_WARNING.GetLocation(),

      // UI.General

      &Config::MAIN_USE_DISCORD_PRESENCE.GetLocation(),
  };

  return std::any_of(s_setting_saveable.cbegin(), s_setting_saveable.cend(),
                     [&config_location](const Config::Location* location) {
                       return *location == config_location;
                     });
}
}  // namespace ConfigLoaders
