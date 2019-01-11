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
  using StateData = ReshapeData;

  explicit Tilt(const std::string& name);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  StateData GetState();

private:
  enum
  {
    SETTING_MAX_ANGLE = ReshapableInput::SETTING_COUNT,
  };

  static constexpr int MAX_DEG_PER_SEC = 360 * 6;

  StateData m_tilt;

  using Clock = std::chrono::steady_clock;
  Clock::time_point m_last_update;
};
}  // namespace ControllerEmu
