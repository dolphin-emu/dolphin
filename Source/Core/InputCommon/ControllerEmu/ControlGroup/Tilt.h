// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class Tilt : public ReshapableInput
{
public:
  using StateData = ReshapeData;

  explicit Tilt(const std::string& name);

  ReshapeData GetReshapableState(bool adjusted) const final override;
  ControlState GetGateRadiusAtAngle(double angle) const final override;

  // Tilt is using the gate radius to adjust the tilt angle so we must provide an unadjusted value
  // for the default input radius.
  ControlState GetDefaultInputRadiusAtAngle(double angle) const final override;

  StateData GetState() const;

  // Return peak rotational velocity (for a complete turn) in radians/sec
  ControlState GetMaxRotationalVelocity() const;

private:
  Control* GetModifierInput() const override;

  SettingValue<double> m_max_angle_setting;
  SettingValue<double> m_max_rotational_velocity;
};
}  // namespace ControllerEmu
