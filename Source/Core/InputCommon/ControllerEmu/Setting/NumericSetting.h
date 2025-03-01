// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Common/IniFile.h"
#include "InputCommon/ControlReference/ControlReference.h"

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
  virtual ~SettingValue() = default;

  virtual ValueType GetValue() const
  {
    // Only update dynamic values when the input gate is enabled.
    // Otherwise settings will all change to 0 when window focus is lost.
    // This is very undesirable for things like battery level or attached extension.
    if (!IsSimpleValue() && ControlReference::GetInputGate())
      m_value = m_input.GetState<ValueType>();

    return m_value;
  }

  ValueType GetCachedValue() const { return m_value; }

  bool IsSimpleValue() const { return m_input.GetExpression().empty(); }

  virtual void SetValue(const ValueType value)
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

template <typename ValueType>
class SubscribableSettingValue final : public SettingValue<ValueType>
{
public:
  using Base = SettingValue<ValueType>;

  ValueType GetValue() const override
  {
    const ValueType cached_value = GetCachedValue();
    if (IsSimpleValue())
      return cached_value;

    const ValueType updated_value = Base::GetValue();
    if (updated_value != cached_value)
      TriggerCallbacks();

    return updated_value;
  }

  void SetValue(const ValueType value) override
  {
    if (value != GetCachedValue())
    {
      Base::SetValue(value);
      TriggerCallbacks();
    }
    else if (!IsSimpleValue())
    {
      // The setting has an expression with a cached value equal to the one currently being set.
      // Don't trigger the callbacks (since the value didn't change), but clear the expression and
      // make the setting a simple value instead.
      Base::SetValue(value);
    }
  }

  ValueType GetCachedValue() const { return Base::GetCachedValue(); }
  bool IsSimpleValue() const { return Base::IsSimpleValue(); }

  using SettingChangedCallback = std::function<void(ValueType)>;

  int AddCallback(const SettingChangedCallback& callback)
  {
    std::lock_guard lock(m_mutex);
    const int callback_id = m_next_callback_id;
    ++m_next_callback_id;
    m_callback_pairs.emplace_back(callback_id, callback);

    return callback_id;
  }

  void RemoveCallback(const int id)
  {
    std::lock_guard lock(m_mutex);
    const auto iter = std::ranges::find(m_callback_pairs, id, &IDCallbackPair::first);
    if (iter != m_callback_pairs.end())
      m_callback_pairs.erase(iter);
  }

private:
  void TriggerCallbacks() const
  {
    std::lock_guard lock(m_mutex);
    const ValueType value = Base::GetValue();
    for (const auto& pair : m_callback_pairs)
      pair.second(value);
  }

  using IDCallbackPair = std::pair<int, SettingChangedCallback>;
  std::vector<IDCallbackPair> m_callback_pairs;
  int m_next_callback_id = 0;

  mutable std::mutex m_mutex;
};

}  // namespace ControllerEmu
