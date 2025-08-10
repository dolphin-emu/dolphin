// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/FreeLookConfig.h"

#include <optional>

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
static std::optional<CPUThreadConfigCallback::ConfigChangedCallbackID>
    s_config_changed_callback_id = std::nullopt;

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
  if (!s_config_changed_callback_id.has_value())
  {
    s_config_changed_callback_id =
        CPUThreadConfigCallback::AddConfigChangedCallback([] { s_config.Refresh(); });
  }

  camera_config.control_type = ::Config::Get(::Config::FL1_CONTROL_TYPE);
  enabled = ::Config::Get(::Config::FREE_LOOK_ENABLED) &&
            !AchievementManager::GetInstance().IsHardcoreModeActive();
}

void Config::Shutdown()
{
  if (!s_config_changed_callback_id.has_value())
    return;

  CPUThreadConfigCallback::RemoveConfigChangedCallback(*s_config_changed_callback_id);
  s_config_changed_callback_id.reset();
}
}  // namespace FreeLook
