// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

#include <fmt/format.h>

namespace ControllerEmu
{
NumericSettingBase::NumericSettingBase(const NumericSettingDetails& details) : m_details(details)
{
}

// Explicit instantiations so generic definitions can exist outside of the header.
template class NumericSetting<int>;
template class NumericSetting<double>;
template class NumericSetting<bool>;

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

template <typename T>
void NumericSetting<T>::SetExpressionFromValue()
{
  // Always include -/+ sign to prevent CoalesceExpression binding.
  // e.g. 1 is a valid input name for keyboard devices, +1 is not.
  m_value.m_input.SetExpression(fmt::format("{:+g}", ControlState(GetValue())));
}

template <typename T>
void NumericSetting<T>::SimplifyIfPossible()
{
  ValueType value;
  if (TryParse(std::string(StripWhitespace(m_value.m_input.GetExpression())), &value))
    m_value.SetValue(value);
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
