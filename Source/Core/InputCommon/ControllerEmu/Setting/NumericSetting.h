// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
enum class SettingType
{
  Int,
  Double,
  Bool,
};

struct NumericSettingDetails
{
  NumericSettingDetails(const char* const _ini_name, const char* const _ui_suffix = nullptr,
                        const char* const _ui_description = nullptr,
                        const char* const _ui_name = nullptr)
      : ini_name(_ini_name), ui_suffix(_ui_suffix), ui_description(_ui_description),
        ui_name(_ui_name ? _ui_name : _ini_name)
  {
  }

  // The name used in ini files.
  const char* const ini_name;

  // A string applied to the number in the UI (unit of measure).
  const char* const ui_suffix;

  // Detailed description of the setting.
  const char* const ui_description;

  // The name used in the UI (if different from ini file).
  const char* const ui_name;
};

class NumericSettingBase
{
public:
  NumericSettingBase(const NumericSettingDetails& details);

  virtual ~NumericSettingBase() = default;

  virtual void LoadFromIni(const IniFile::Section& section, const std::string& group_name) = 0;
  virtual void SaveToIni(IniFile::Section& section, const std::string& group_name) const = 0;

  virtual InputReference& GetInputReference() = 0;
  virtual const InputReference& GetInputReference() const = 0;

  // Can be edited in UI? This setting should not used if is false
  virtual bool IsEnabled() const = 0;
  virtual bool HasEditCondition() const = 0;

  virtual ControlState GetGenericValue() const = 0;

  virtual ControlState GetGenericMinValue() const = 0;
  virtual ControlState GetGenericMaxValue() const = 0;

  virtual bool IsSimpleValue() const = 0;

  // Convert a literal expression e.g. "7.0" to a regular value. (disables expression parsing)
  virtual void SimplifyIfPossible() = 0;

  virtual void Update() = 0;

  // Convert a regular value to an expression. (used before expression editing)
  virtual std::string GetExpressionFromValue() const = 0;
  virtual void SetExpressionFromValue() = 0;

  virtual SettingType GetType() const = 0;

  const char* GetUIName() const;
  const char* GetUISuffix() const;
  const char* GetUIDescription() const;

protected:
  NumericSettingDetails m_details;
};

template <typename T>
class SettingValue;

// Needs EmulatedController::GetStateLock() on anything that mutates it (not GetValue())
template <typename T>
class NumericSetting final : public NumericSettingBase
{
public:
  using ValueType = T;

  static_assert(std::is_same<ValueType, int>() || std::is_same<ValueType, double>() ||
                    std::is_same<ValueType, bool>(),
                "NumericSetting is only implemented for int, double, and bool.");

  NumericSetting(SettingValue<ValueType>* value, const NumericSettingDetails& details,
                 ValueType default_value, ValueType min_value, ValueType max_value,
                 const NumericSetting<bool>* edit_condition = nullptr)
      : NumericSettingBase(details), m_value(*value), m_default_value(default_value),
        m_min_value(min_value), m_max_value(max_value), m_edit_condition(edit_condition)
  {
    ASSERT(m_max_value >= m_min_value);  // Let them be equal
    // Theoretically not needed but avoids log errors
    const auto lock = EmulatedController::GetStateLock();
    m_value.SetValue(m_default_value);
  }

  void LoadFromIni(const IniFile::Section& section, const std::string& group_name) override
  {
    // Note that even if we have a range value, we completely ignore it because it would
    // be useless and it's not exposed to the user
    std::string str_value;
    if (section.Get(group_name + m_details.ini_name, &str_value))
    {
      m_value.m_input.SetExpression(std::move(str_value));
      SimplifyIfPossible();
    }
    else
    {
      SetValue(m_default_value);
    }
  }

  void SaveToIni(IniFile::Section& section, const std::string& group_name) const override
  {
    if (IsSimpleValue())
    {
      section.Set(group_name + m_details.ini_name, GetValue(), m_default_value);
    }
    else
    {
      // We can't save line breaks in a single line config. Restoring them is too complicated.
      std::string expression = m_value.m_input.GetExpression();
      ReplaceBreaksWithSpaces(expression);
      section.Set(group_name + m_details.ini_name, expression, "");
    }
  }

  // If the edit condition isn't simplified, its value might change at runtime, but this
  // is for UI only so it's fine.
  bool IsEnabled() const override { return !m_edit_condition || m_edit_condition->GetValue(); }
  bool HasEditCondition() const override { return m_edit_condition != nullptr; }

  bool IsSimpleValue() const override { return m_value.IsSimpleValue(); }

  // Simplification has its limit, but is also not really necessary,
  // so if we have an expression with only fixed values, like 2+3, it won't be simplified.
  // Only call this if the expression has changed or you will lose your value.
  void SimplifyIfPossible() override
  {
    if (IsSimpleValue())  // Expression is empty, simplify to 0
    {
      m_value.SetValue(ValueType());
      return;
    }

    // TODO: Theoretically we should ask the expression if its value is fixed or not, and if it is,
    // just copy its value into ours, with something similar to ControlReference::BoundCount().
    // Otherwise if we have an expression like "(7*9/3)" it won't be simplified.

    // Strip spaces and line breaks around the edges because the expression keeps them.
    const std::string stripped_expression(StripSpaces(m_value.m_input.GetExpression()));
    ValueType value;
    bool value_found = false;
    if constexpr (std::is_same<ValueType, bool>())
    {
      ControlState temp_value = ControlState(GetValue());
      // Any expression that isn't exactly 0 or 1 would fail to parse to bool
      if (TryParse(stripped_expression, &temp_value))
      {
        value = std::lround(temp_value) > 0;
        value_found = true;
      }
      // Fallback to bool parsing if number parsing failed, this will also work with "true/false"
      else if (TryParse(stripped_expression, &value))
      {
        value_found = true;
      }
    }
    else if (TryParse(stripped_expression, &value))
    {
      value_found = true;
    }
    // Only simplify if within the accepted ranges, to not force users to write numbers around "()".
    // It's a shame to not have it simplified but otherwise we'd need to rewrite a lot of UI code to
    // unlock the ranges over the allowed min/max, to avoid the UI resetting the values.
    if (value_found && value >= GetMinValue() && value <= GetMaxValue())
      m_value.SetValue(value);
  }

  void Update() override
  {
    if (!IsSimpleValue())  // Only update the expression if it's not been simplified
      m_value.m_input.UpdateState();
  }

  std::string GetExpressionFromValue() const override;
  void SetExpressionFromValue() override
  {
    m_value.m_input.SetExpression(std::move(GetExpressionFromValue()));
  }
  InputReference& GetInputReference() override { return m_value.m_input; }
  const InputReference& GetInputReference() const override { return m_value.m_input; }

  ControlState GetGenericValue() const override { return ControlState(GetValue()); }
  ValueType GetValue() const { return m_value.GetValue(); }
  void SetValue(ValueType value) { m_value.SetValue(value); }

  ValueType GetDefaultValue() const { return m_default_value; }
  // These are only for UI and they are not applied if IsSimpleValue() is false
  ValueType GetMinValue() const { return m_min_value; }
  ValueType GetMaxValue() const { return m_max_value; }
  ControlState GetGenericMinValue() const override { return ControlState(GetMinValue()); }
  ControlState GetGenericMaxValue() const override { return ControlState(GetMaxValue()); }

  SettingType GetType() const override;

private:
  SettingValue<ValueType>& m_value;

  const ValueType m_default_value;
  const ValueType m_min_value;
  const ValueType m_max_value;

  // Assuming settings are never destroyed if not all together
  const NumericSetting<bool>* const m_edit_condition;
};

// Needs EmulatedController::GetStateLock() on SetValue()
template <typename T>
class SettingValue
{
  using ValueType = T;

  friend class NumericSetting<T>;

public:
  ValueType GetValue() const
  {
    // IsSimpleValue() could theoretically change while we are within this call but
    // at worse it will return the previous m_value for one frame
    if (!IsSimpleValue())
      return m_input.GetState<ValueType>();
    return m_value;
  }

  bool IsSimpleValue() const
  {
    // An empty or all spaces expression string won't set parsing as succeeded.
    // An invalid expression will return 0 so we could just simplify it anyway, but then
    // if we wanted to go back to its widget to fix the expression the expression would be lost.
    return m_input.GetParseStatus() == ciface::ExpressionParser::ParseStatus::EmptyExpression;
  }

private:
  void SetValue(ValueType value)
  {
    m_value = value;
    // Clear the expression to use our new "simple" value.
    m_input.SetExpression("");
  }

  // This won't be reset if we aren't simple anymore, just ignore it
  std::atomic<ValueType> m_value = {};

  IgnoreGateInputReference m_input;
};

}  // namespace ControllerEmu
