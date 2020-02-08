// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class MixedTriggers : public ControlGroup
{
public:
  explicit MixedTriggers(const std::string& name);

  size_t GetTriggerCount() const;

  void GetState(u16* digital, const u16* bitmasks, ControlState* analog) const;

  ControlState GetRawDigitalState(size_t index) const;
  ControlState GetRawAnalogState(size_t index) const;

  ControlState GetDeadzone() const;
  ControlState GetThreshold() const;

  virtual bool IsAnalogEnabled() const;

private:
  SettingValue<double> m_threshold_setting;
  SettingValue<double> m_deadzone_setting;
};
}  // namespace ControllerEmu
