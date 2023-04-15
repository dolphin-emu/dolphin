// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigLoaders/IsSettingSaveable.h"

#include <algorithm>
#include <vector>

#include "Common/Config/Config.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/UISettings.h"
#include "Core/Config/WiimoteSettings.h"

namespace ConfigLoaders
{
bool IsSettingSaveable(const Config::Location& config_location)
{
  for (Config::System system :
       {Config::System::SYSCONF, Config::System::GFX, Config::System::DualShockUDPClient,
        Config::System::Logger, Config::System::FreeLook, Config::System::Main,
        Config::System::GameSettingsOnly})
  {
    if (config_location.system == system)
      return true;
  }

  static const auto s_setting_saveable = {
      // UI.General

      &Config::MAIN_USE_DISCORD_PRESENCE.GetLocation(),

      // Wiimote

      &Config::WIIMOTE_1_SOURCE.GetLocation(),
      &Config::WIIMOTE_2_SOURCE.GetLocation(),
      &Config::WIIMOTE_3_SOURCE.GetLocation(),
      &Config::WIIMOTE_4_SOURCE.GetLocation(),
      &Config::WIIMOTE_BB_SOURCE.GetLocation(),

      // Achievements

      &Config::RA_ENABLED.GetLocation(),
      &Config::RA_USERNAME.GetLocation(),
      &Config::RA_API_TOKEN.GetLocation(),
  };

  return std::any_of(begin(s_setting_saveable), end(s_setting_saveable),
                     [&config_location](const Config::Location* location) {
                       return *location == config_location;
                     });
}
}  // namespace ConfigLoaders
