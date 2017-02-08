// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/Setting/Setting.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class BooleanSetting
{
public:
  BooleanSetting(const std::string& setting_name, const std::string& ui_name,
                 const bool default_value, const SettingType setting_type = SettingType::NORMAL);
  BooleanSetting(const std::string& setting_name, const bool default_value,
                 const SettingType setting_type = SettingType::NORMAL);

  bool GetValue() const;
  void SetValue(bool value);
  const SettingType m_type;
  const std::string m_name;
  const std::string m_ui_name;
  const bool m_default_value;
  bool m_value;
};

}  // namespace ControllerEmu
