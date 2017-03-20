// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"

namespace ControllerEmu
{
BooleanSetting::BooleanSetting(const std::string& setting_name, const std::string& ui_name,
                               const bool default_value, const SettingType setting_type)
    : m_type(setting_type), m_name(setting_name), m_ui_name(ui_name),
      m_default_value(default_value), m_value(default_value)
{
}

BooleanSetting::BooleanSetting(const std::string& setting_name, const bool default_value,
                               const SettingType setting_type)
    : BooleanSetting(setting_name, setting_name, default_value, setting_type)
{
}

bool BooleanSetting::GetValue() const
{
  return m_value;
}
void BooleanSetting::SetValue(bool value)
{
  m_value = value;
}

}  // namespace ControllerEmu
