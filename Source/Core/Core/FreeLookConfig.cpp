// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FreeLookConfig.h"

#include "Core/AchievementManager.h"
#include "Core/CPUThreadConfigCallback.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

namespace FreeLook
{
static Config s_config;
static Config s_active_config;
static bool s_has_registered_callback = false;

Config& GetConfig()
{
  return s_config;
}

const Config& GetActiveConfig()
{
  return s_active_config;
}

void UpdateActiveConfig()
{
  s_active_config = s_config;
}

Config::Config()
{
  camera_config.control_type = ControlType::SixAxis;
  enabled = false;
}

void Config::Refresh()
{
  if (!s_has_registered_callback)
  {
    CPUThreadConfigCallback::AddConfigChangedCallback([] { s_config.Refresh(); });
    s_has_registered_callback = true;
  }

  camera_config.control_type = ::Config::Get(::Config::FL1_CONTROL_TYPE);
#ifdef USE_RETRO_ACHIEVEMENTS
  enabled = ::Config::Get(::Config::FREE_LOOK_ENABLED) &&
            !AchievementManager::GetInstance()->IsHardcoreModeActive();
#else   // USE_RETRO_ACHIEVEMENTS
  enabled = ::Config::Get(::Config::FREE_LOOK_ENABLED);
#endif  // USE_RETRO_ACHIEVEMENTS
}
}  // namespace FreeLook
