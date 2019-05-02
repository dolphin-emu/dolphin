// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Tilt : public ReshapableInput
{
public:
  using StateData = ReshapeData;

  explicit Tilt(const std::string& name);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double angle) const final override;

  // Tilt is using the gate radius to adjust the tilt angle so we must provide an unadjusted value
  // for the default input radius.
  ControlState GetDefaultInputRadiusAtAngle(double angle) const final override;

  StateData GetState();

private:
  SettingValue<double> m_max_angle_setting;
};
}  // namespace ControllerEmu
