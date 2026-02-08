// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>

#include "Common/Matrix.h"
#include "InputCommon/ControllerEmu/StickGate.h"

namespace ControllerEmu
{
class Force : public ReshapableInput
{
public:
  using StateData = Common::Vec3;

  explicit Force(const std::string& name);

  ReshapeData GetReshapableState(bool adjusted) const final override;
  ControlState GetGateRadiusAtAngle(double ang) const final override;

  ControlState GetDefaultInputRadiusAtAngle(double angle) const final override;

  StateData GetState(bool adjusted = true) const;

  // Velocities returned in m/s.
  ControlState GetSpeed() const;
  ControlState GetReturnSpeed() const;

  // Return twist angle in radians.
  ControlState GetTwistAngle() const;

  // Return swing distance in meters.
  ControlState GetMaxDistance() const;

private:
  SettingValue<double> m_distance_setting;
  SettingValue<double> m_speed_setting;
  SettingValue<double> m_return_speed_setting;
  SettingValue<double> m_angle_setting;
};

class Shake : public ControlGroup
{
public:
  using StateData = Common::Vec3;

  explicit Shake(const std::string& name, ControlState default_intensity_scale = 1);

  StateData GetState(bool adjusted = true) const;

  ControlState GetDeadzone() const;

  // Return total travel distance in meters.
  ControlState GetIntensity() const;

  // Return frequency in Hz.
  ControlState GetFrequency() const;

private:
  SettingValue<double> m_deadzone_setting;
  SettingValue<double> m_intensity_setting;
  SettingValue<double> m_frequency_setting;
};

}  // namespace ControllerEmu
