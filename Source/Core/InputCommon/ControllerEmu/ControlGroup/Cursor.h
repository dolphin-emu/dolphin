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

    bool IsVisible() const;
  };

  Cursor(std::string name, std::string ui_name);

  ReshapeData GetReshapableState(bool adjusted) final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  StateData GetState(bool adjusted);

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

  static constexpr double BUTTON_THRESHOLD = 0.5;

  // Not adjusted by width/height/center:
  StateData m_state;

  // Adjusted:
  StateData m_prev_result;

  int m_auto_hide_timer = AUTO_HIDE_MS;

  using Clock = std::chrono::steady_clock;
  Clock::time_point m_last_update;

  SettingValue<double> m_yaw_setting;
  SettingValue<double> m_pitch_setting;
  SettingValue<double> m_vertical_offset_setting;

  SettingValue<bool> m_relative_setting;
  SettingValue<bool> m_autohide_setting;
};
}  // namespace ControllerEmu
