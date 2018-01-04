// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
NumericSetting::NumericSetting(const std::string& setting_name, const ControlState default_value,
                               const u32 low, const u32 high, const SettingType setting_type)
    : m_type(setting_type), m_name(setting_name), m_default_value(default_value), m_low(low),
      m_high(high), m_value(default_value)
{
}

ControlState NumericSetting::GetValue() const
{
  return m_value;
}
void NumericSetting::SetValue(ControlState value)
{
  m_value = value;
}

}  // namespace ControllerEmu
