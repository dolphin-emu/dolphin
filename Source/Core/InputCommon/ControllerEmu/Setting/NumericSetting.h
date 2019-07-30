// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
enum class SettingType
{
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

  virtual SettingType GetType() const = 0;

  const char* GetIniName() const;
  const char* GetUIName() const;
  const char* GetUISuffix() const;
  const char* GetUIDescription() const;

protected:
  NumericSettingDetails m_details;
};

template <typename T>
class SettingValue;

template <typename T>
class NumericSetting : public NumericSettingBase
{
public:
  using ValueType = T;

  static_assert(std::is_same<ValueType, double>() || std::is_same<ValueType, bool>(),
                "NumericSetting is only implemented for double and bool.");

  NumericSetting(SettingValue<ValueType>* value, const NumericSettingDetails& details,
                 ValueType default_value, ValueType min_value, ValueType max_value)
      : NumericSettingBase(details), m_value(*value), m_default_value(default_value),
        m_min_value(min_value), m_max_value(max_value)
  {
    m_value.SetValue(m_default_value);
  }

  void LoadFromIni(const IniFile::Section& section, const std::string& group_name) override
  {
    ValueType value;
    section.Get(group_name + m_details.ini_name, &value, m_default_value);
    SetValue(value);
  }

  void SaveToIni(IniFile::Section& section, const std::string& group_name) const override
  {
    section.Set(group_name + m_details.ini_name, GetValue(), m_default_value);
  }

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
  ValueType GetValue() const { return m_value; }

private:
  void SetValue(ValueType value) { m_value = value; }

  // Values are R/W by both UI and CPU threads.
  std::atomic<ValueType> m_value = {};
};

}  // namespace ControllerEmu
