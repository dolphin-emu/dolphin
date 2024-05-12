// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
enum class SettingType
{
  Int,
  Double,
  Bool,
};

enum class SettingVisibility
{
  Normal,
  Advanced,
};

struct NumericSettingDetails
{
  NumericSettingDetails(const char* const _ini_name, const char* const _ui_suffix = nullptr,
                        const char* const _ui_description = nullptr,
                        const char* const _ui_name = nullptr,
                        SettingVisibility _visibility = SettingVisibility::Normal)
      : ini_name(_ini_name), ui_suffix(_ui_suffix), ui_description(_ui_description),
        ui_name(_ui_name ? _ui_name : _ini_name), visibility(_visibility)
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

  // Advanced settings should be harder to change in the UI. They might confuse users.
  const SettingVisibility visibility;
};

class NumericSettingBase
{
public:
  NumericSettingBase(const NumericSettingDetails& details);

  virtual ~NumericSettingBase() = default;

  virtual void LoadFromIni(const Common::IniFile::Section& section,
                           const std::string& group_name) = 0;
  virtual void SaveToIni(Common::IniFile::Section& section,
                         const std::string& group_name) const = 0;

  virtual InputReference& GetInputReference() = 0;
  virtual const InputReference& GetInputReference() const = 0;

  virtual bool IsSimpleValue() const = 0;

  // Convert a literal expression e.g. "7.0" to a regular value. (disables expression parsing)
  virtual void SimplifyIfPossible() = 0;

  // Convert a regular value to an expression. (used before expression editing)
  virtual void SetExpressionFromValue() = 0;

  virtual SettingType GetType() const = 0;

  virtual void SetToDefault() = 0;

  const char* GetININame() const;
  const char* GetUIName() const;
  const char* GetUISuffix() const;
  const char* GetUIDescription() const;
  SettingVisibility GetVisibility() const;

protected:
  NumericSettingDetails m_details;
};

template <typename T>
class SettingValue;

template <typename T>
class NumericSetting final : public NumericSettingBase
{
public:
  using ValueType = T;

  static_assert(std::is_same<ValueType, int>() || std::is_same<ValueType, double>() ||
                    std::is_same<ValueType, bool>(),
                "NumericSetting is only implemented for int, double, and bool.");

  NumericSetting(SettingValue<ValueType>* value, const NumericSettingDetails& details,
                 ValueType default_value, ValueType min_value, ValueType max_value)
      : NumericSettingBase(details), m_value(*value), m_default_value(default_value),
        m_min_value(min_value), m_max_value(max_value)
  {
    SetToDefault();
  }

  void SetToDefault() override { m_value.SetValue(m_default_value); }

  void LoadFromIni(const Common::IniFile::Section& section, const std::string& group_name) override
  {
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

  void SaveToIni(Common::IniFile::Section& section, const std::string& group_name) const override
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

  bool IsSimpleValue() const override { return m_value.IsSimpleValue(); }
  void SimplifyIfPossible() override;
  void SetExpressionFromValue() override;
  InputReference& GetInputReference() override { return m_value.m_input; }
  const InputReference& GetInputReference() const override { return m_value.m_input; }

  ValueType GetValue() const { return m_value.GetValue(); }
  void SetValue(ValueType value) { m_value.SetValue(value); }

  ValueType GetDefaultValue() const { return m_default_value; }
  ValueType GetMinValue() const { return m_min_value; }
  ValueType GetMaxValue() const { return m_max_value; }

  SettingType GetType() const override;

private:
  SettingValue<ValueType>& m_value;

  const ValueType m_default_value;
  const ValueType m_min_value;
  const ValueType m_max_value;
};

template <typename T>
class SettingValue
{
  using ValueType = T;

  friend class NumericSetting<T>;

public:
  ValueType GetValue() const
  {
    // Only update dynamic values when the input gate is enabled.
    // Otherwise settings will all change to 0 when window focus is lost.
    // This is very undesirable for things like battery level or attached extension.
    if (!IsSimpleValue() && ControlReference::GetInputGate())
      m_value = m_input.GetState<ValueType>();

    return m_value;
  }

  bool IsSimpleValue() const { return m_input.GetExpression().empty(); }

  void SetValue(ValueType value)
  {
    m_value = value;

    // Clear the expression to use our new "simple" value.
    m_input.SetExpression("");
  }

private:
  // Values are R/W by both UI and CPU threads.
  mutable std::atomic<ValueType> m_value = {};

  // Unfortunately InputReference's state grabbing is non-const requiring mutable here.
  mutable InputReference m_input;
};

}  // namespace ControllerEmu
