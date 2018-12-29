// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <string>

#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Tilt : public ReshapableInput
{
public:
  enum
  {
    SETTING_MAX_ANGLE = ReshapableInput::SETTING_COUNT,
  };

  explicit Tilt(const std::string& name);

  StateData GetState(bool adjusted = true);

  ControlState GetGateRadiusAtAngle(double ang) const override;

private:
  typedef std::chrono::steady_clock Clock;

  StateData m_tilt;
  Clock::time_point m_last_update;
};
}  // namespace ControllerEmu
