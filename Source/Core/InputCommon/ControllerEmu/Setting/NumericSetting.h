// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/Setting/Setting.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class NumericSetting
{
public:
  NumericSetting(const std::string& setting_name, const ControlState default_value,
                 const u32 low = 0, const u32 high = 100,
                 const SettingType setting_type = SettingType::NORMAL);

  ControlState GetValue() const;
  void SetValue(ControlState value);
  const SettingType m_type;
  const std::string m_name;
  const ControlState m_default_value;
  const u32 m_low;
  const u32 m_high;
  ControlState m_value;
};

}  // namespace ControllerEmu
