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
class Cursor : public ReshapableInput
{
public:
  struct StateData
  {
    ControlState x{};
    ControlState y{};
    ControlState z{};
  };

  enum
  {
    SETTING_CENTER = ReshapableInput::SETTING_COUNT,
    SETTING_WIDTH,
    SETTING_HEIGHT,
  };

  explicit Cursor(const std::string& name);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  StateData GetState(bool adjusted);

private:
  // This is used to reduce the cursor speed for relative input
  // to something that makes sense with the default range.
  static constexpr double STEP_PER_SEC = 0.04 * 200;

  // Smooth out forward/backward movements:
  static constexpr double STEP_Z_PER_SEC = 0.05 * 200;

  static constexpr int AUTO_HIDE_MS = 2500;
  static constexpr double AUTO_HIDE_DEADZONE = 0.001;

  static constexpr double BUTTON_THRESHOLD = 0.5;

  // Not adjusted by width/height/center:
  StateData m_state;

  // Adjusted:
  StateData m_prev_result;

  int m_auto_hide_timer = AUTO_HIDE_MS;

  using Clock = std::chrono::steady_clock;
  Clock::time_point m_last_update;
};
}  // namespace ControllerEmu
