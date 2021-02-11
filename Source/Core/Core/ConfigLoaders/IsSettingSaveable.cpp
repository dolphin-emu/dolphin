// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/IsSettingSaveable.h"

#include <algorithm>
#include <vector>

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"

namespace ConfigLoaders
{
bool IsSettingSaveable(const Config::Location& config_location)
{
  for (Config::System system : {Config::System::SYSCONF, Config::System::GFX,
                                Config::System::DualShockUDPClient, Config::System::Logger})
  {
    if (config_location.system == system)
      return true;
  }

  if (config_location.system == Config::System::Main)
  {
    for (const std::string& section : {"NetPlay", "General", "Display", "Network", "Analytics",
                                       "AndroidOverlayButtons", "Android"})
    {
      if (config_location.section == section)
        return true;
    }
  }

  static constexpr std::array<const Config::Location*, 29> s_setting_saveable = {
      // Main.Core

      &Config::MAIN_DEFAULT_ISO.location,
      &Config::MAIN_MEMCARD_A_PATH.location,
      &Config::MAIN_MEMCARD_B_PATH.location,
      &Config::MAIN_AUTO_DISC_CHANGE.location,
      &Config::MAIN_ALLOW_SD_WRITES.location,
      &Config::MAIN_DPL2_DECODER.location,
      &Config::MAIN_DPL2_QUALITY.location,
      &Config::MAIN_RAM_OVERRIDE_ENABLE.location,
      &Config::MAIN_MEM1_SIZE.location,
      &Config::MAIN_MEM2_SIZE.location,
      &Config::MAIN_GFX_BACKEND.location,
      &Config::MAIN_ENABLE_SAVESTATES.location,
      &Config::MAIN_FALLBACK_REGION.location,

      // Main.Interface

      &Config::MAIN_USE_PANIC_HANDLERS.location,
      &Config::MAIN_OSD_MESSAGES.location,

      // Main.Interface

      &Config::MAIN_SKIP_NKIT_WARNING.location,

      // UI.General

      &Config::MAIN_USE_DISCORD_PRESENCE.location,

      // PrimeHack

      &Config::TOGGLE_ARM_REPOSITION.location,
      &Config::AUTO_EFB.location,
      &Config::ARMPOSITION_MODE.location,
      &Config::ARMPOSITION_LEFTRIGHT.location,
      &Config::ARMPOSITION_UPDOWN.location,
      &Config::ARMPOSITION_FORWARDBACK.location,
      &Config::TOGGLE_CULLING.location,
      &Config::DISABLE_BLOOM.location,
      &Config::FOV.location,
      &Config::ENABLE_SECONDARY_GUNFX.location,
      &Config::GC_SHOW_CROSSHAIR.location,
      &Config::GC_CROSSHAIR_COLOR_RGBA.location,
  };

  return std::any_of(s_setting_saveable.cbegin(), s_setting_saveable.cend(),
                     [&config_location](const Config::Location* location) {
                       return *location == config_location;
                     });
}
}  // namespace ConfigLoaders
