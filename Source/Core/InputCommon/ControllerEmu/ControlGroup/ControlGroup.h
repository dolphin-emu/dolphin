// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

namespace ControllerEmu
{
class Control;

class NumericSettingBase;
struct NumericSettingDetails;

template <typename T>
class NumericSetting;

template <typename T>
class SettingValue;

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
};

class ControlGroup
{
public:
  explicit ControlGroup(std::string name, GroupType type = GroupType::Other);
  ControlGroup(std::string name, std::string ui_name, GroupType type = GroupType::Other);
  virtual ~ControlGroup();

  virtual void LoadConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");

  void SetControlExpression(int index, const std::string& expression);

  template <typename T>
  void AddSetting(SettingValue<T>* value, const NumericSettingDetails& details,
                  std::common_type_t<T> default_value, std::common_type_t<T> min_value = {},
                  std::common_type_t<T> max_value = T(100))
  {
    numeric_settings.emplace_back(
        std::make_unique<NumericSetting<T>>(value, details, default_value, min_value, max_value));
  }

  void AddDeadzoneSetting(SettingValue<double>* value, double maximum_deadzone);

  template <typename T>
  static T ApplyDeadzone(T input, std::common_type_t<T> deadzone)
  {
    return std::copysign(std::max(T{0}, std::abs(input) - deadzone) / (T{1} - deadzone), input);
  }

  const std::string name;
  const std::string ui_name;
  const GroupType type;

  std::vector<std::unique_ptr<Control>> controls;
  std::vector<std::unique_ptr<NumericSettingBase>> numeric_settings;
};
}  // namespace ControllerEmu
