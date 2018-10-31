// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Cursor : public ControlGroup
{
public:
  struct StateData
  {
    ControlState x{};
    ControlState y{};
    ControlState z{};
  };

  explicit Cursor(const std::string& name);

  StateData GetState(bool adjusted = false);

private:
  // This is used to reduce the cursor speed for relative input
  // to something that makes sense with the default range.
  static constexpr double SPEED_MULTIPLIER = 0.04;

  // Sets the length for the auto-hide timer
  static constexpr int TIMER_VALUE = 500;

  StateData m_state;

  int m_autohide_timer = TIMER_VALUE;
  ControlState m_prev_xx;
  ControlState m_prev_yy;
};
}  // namespace ControllerEmu
