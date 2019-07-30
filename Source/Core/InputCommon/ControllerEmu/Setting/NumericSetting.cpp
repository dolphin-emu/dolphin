// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
NumericSettingBase::NumericSettingBase(const NumericSettingDetails& details) : m_details(details)
{
}

const char* NumericSettingBase::GetIniName() const
{
  return m_details.ini_name;
}

const char* NumericSettingBase::GetUIName() const
{
  return m_details.ui_name;
}

const char* NumericSettingBase::GetUISuffix() const
{
  return m_details.ui_suffix;
}

const char* NumericSettingBase::GetUIDescription() const
{
  return m_details.ui_description;
}

template <>
SettingType NumericSetting<double>::GetType() const
{
  return SettingType::Double;
}

template <>
SettingType NumericSetting<bool>::GetType() const
{
  return SettingType::Bool;
}

}  // namespace ControllerEmu
