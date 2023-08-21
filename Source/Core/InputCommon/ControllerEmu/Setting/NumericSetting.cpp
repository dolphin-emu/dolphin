// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

#include <sstream>

namespace ControllerEmu
{
NumericSettingBase::NumericSettingBase(const NumericSettingDetails& details) : m_details(details)
{
}

const char* NumericSettingBase::GetININame() const
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

SettingVisibility NumericSettingBase::GetVisibility() const
{
  return m_details.visibility;
}

template <>
void NumericSetting<int>::SetExpressionFromValue()
{
  m_value.m_input.SetExpression(ValueToString(GetValue()));
}

template <>
void NumericSetting<double>::SetExpressionFromValue()
{
  // We must use a dot decimal separator for expression parser.
  std::ostringstream ss;
  ss.imbue(std::locale::classic());
  ss << GetValue();

  m_value.m_input.SetExpression(ss.str());
}

template <>
void NumericSetting<bool>::SetExpressionFromValue()
{
  // Cast bool to prevent "true"/"false" strings.
  m_value.m_input.SetExpression(ValueToString(int(GetValue())));
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
