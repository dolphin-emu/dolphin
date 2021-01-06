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
// Instead of having two instances of this, we have one with two states,
// one for the UI and one for the game. Otherwise when bringing up the
// input settings, it would also affect the cursor state in the game.
// This is not a problem with other ReshapableInput as they don't
// cache a state
class Cursor : public ReshapableInput
{
public:
  struct StateData
  {
    ControlState x{};
    ControlState y{};

    bool IsVisible() const;
  };

  Cursor(std::string name, std::string ui_name);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  // Also updates the state
  StateData GetState(float absolute_time_elapsed, bool is_ui);

  void ResetState(bool is_ui);

  // Yaw movement in radians.
  ControlState GetTotalYaw() const;

  // Pitch movement in radians.
  ControlState GetTotalPitch() const;

  // Vertical offset in meters.
  ControlState GetVerticalOffset() const;

private:
  // This is used to reduce the cursor speed for relative input
  // to something that makes sense with the default range.
  static constexpr double STEP_PER_SEC = 0.01 * 200;

  static constexpr int AUTO_HIDE_MS = 2500;
  static constexpr double AUTO_HIDE_DEADZONE = 0.001;

  // Not adjusted by width/height/center.
  StateData m_state[2];
  StateData m_prev_result[2];

  int m_auto_hide_timer[2] = {AUTO_HIDE_MS, AUTO_HIDE_MS};

  using Clock = std::chrono::steady_clock;
  Clock::time_point m_last_update[2];

  SettingValue<double> m_yaw_setting;
  SettingValue<double> m_pitch_setting;
  SettingValue<double> m_vertical_offset_setting;

  SettingValue<bool> m_relative_setting;
  SettingValue<bool> m_relative_absolute_time_setting;
  SettingValue<bool> m_autohide_setting;
};
}  // namespace ControllerEmu
