// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Setting/BackgroundInputSetting.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerEmu/Setting/Setting.h"

namespace ControllerEmu
{
BackgroundInputSetting::BackgroundInputSetting(const std::string& setting_name)
    : BooleanSetting(setting_name, false, SettingType::VIRTUAL)
{
}

bool BackgroundInputSetting::GetValue() const
{
  return SConfig::GetInstance().m_BackgroundInput;
}
void BackgroundInputSetting::SetValue(bool value)
{
  m_value = value;
  SConfig::GetInstance().m_BackgroundInput = value;
}

}  // namespace ControllerEmu
