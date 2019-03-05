// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const final override;

  ControlState GetDefaultInputRadiusAtAngle(double angle) const final override;

  StateData GetState(bool adjusted = true);

  // Return jerk in m/s^3.
  ControlState GetMaxJerk() const;

  // Return twist angle in radians.
  ControlState GetTwistAngle() const;

  // Return swing distance in meters.
  ControlState GetMaxDistance() const;

private:
  enum
  {
    SETTING_DISTANCE = ReshapableInput::SETTING_COUNT,
    SETTING_JERK,
    SETTING_ANGLE,
  };
};
}  // namespace ControllerEmu
