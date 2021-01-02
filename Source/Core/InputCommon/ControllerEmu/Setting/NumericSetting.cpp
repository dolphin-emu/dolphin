// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

#include <sstream>

namespace ControllerEmu
{
NumericSettingBase::NumericSettingBase(const NumericSettingDetails& details) : m_details(details)
{
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
std::string NumericSetting<int>::GetExpressionFromValue() const
{
  return ValueToString(int(GetValue()));
}

template <>
std::string NumericSetting<double>::GetExpressionFromValue() const
{
  // We must use a dot decimal separator for expression parser.
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << GetValue();

  return ss.str();
}

template <>
std::string NumericSetting<bool>::GetExpressionFromValue() const
{
  // Cast bool to prevent "true"/"false" strings.
  return ValueToString(int(GetValue()));
}

template <>
SettingType NumericSetting<int>::GetType() const
{
  return SettingType::Int;
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
