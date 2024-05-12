// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class Control;

class NumericSettingBase;
struct NumericSettingDetails;

template <typename T>
class NumericSetting;

template <typename T>
class SettingValue;

using InputOverrideFunction = std::function<std::optional<ControlState>(
    const std::string_view group_name, const std::string_view control_name, ControlState state)>;

enum class GroupType
{
  Other,
  Stick,
  MixedTriggers,
  Buttons,
  Force,
  Attachments,
  Tilt,
  Cursor,
  Triggers,
  Slider,
  Shake,
  IMUAccelerometer,
  IMUGyroscope,
  IMUCursor,
  IRPassthrough,
};

class ControlGroup
{
public:
  enum class DefaultValue
  {
    AlwaysEnabled,
    Enabled,
    Disabled,
  };

  explicit ControlGroup(std::string name, GroupType type = GroupType::Other,
                        DefaultValue default_value = DefaultValue::AlwaysEnabled);
  ControlGroup(std::string name, std::string ui_name, GroupType type = GroupType::Other,
               DefaultValue default_value = DefaultValue::AlwaysEnabled);
  virtual ~ControlGroup();

  virtual void LoadConfig(Common::IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");
  virtual void SaveConfig(Common::IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");

  void SetControlExpression(int index, const std::string& expression);

  void AddInput(Translatability translate, std::string name);
  void AddInput(Translatability translate, std::string name, std::string ui_name);
  void AddOutput(Translatability translate, std::string name);

  template <typename T>
  void AddSetting(SettingValue<T>* value, const NumericSettingDetails& details,
                  std::common_type_t<T> default_value_, std::common_type_t<T> min_value = {},
                  std::common_type_t<T> max_value = T(100))
  {
    numeric_settings.emplace_back(
        std::make_unique<NumericSetting<T>>(value, details, default_value_, min_value, max_value));
  }

  void AddVirtualNotchSetting(SettingValue<double>* value, double max_virtual_notch_deg);

  void AddDeadzoneSetting(SettingValue<double>* value, double maximum_deadzone);

  template <typename T>
  static T ApplyDeadzone(T input, std::common_type_t<T> deadzone)
  {
    return std::copysign(std::max(T{0}, std::abs(input) - deadzone) / (T{1} - deadzone), input);
  }

  const std::string name;
  const std::string ui_name;
  const GroupType type;
  const DefaultValue default_value;

  bool enabled = true;
  std::vector<std::unique_ptr<Control>> controls;
  std::vector<std::unique_ptr<NumericSettingBase>> numeric_settings;
};
}  // namespace ControllerEmu
