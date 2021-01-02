// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/StickGate.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

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

  ReshapeData GetReshapableState(bool adjusted) const final override;
  ControlState GetGateRadiusAtAngle(double ang) const override;

  // Also updates the state. We need a separate state for the UI otherwise when the render widget
  // loses focus, we might not be able to preview values from the mapping widgets, and we'd also
  // pollute the game state from the UI update. time_elapsed is in seconds.
  StateData GetState(bool is_ui, float time_elapsed);

  void ResetState(bool is_ui);

  // Yaw movement in radians.
  ControlState GetTotalYaw() const;

  // Pitch movement in radians.
  ControlState GetTotalPitch() const;

  // Vertical offset in meters.
  ControlState GetVerticalOffset() const;

private:
  // This is used to reduce the cursor speed for relative input to something that
  // makes sense with the default range. 200 is the wiimote update frequency.
  static constexpr double STEP_PER_SEC = 0.01 * 200;

  static constexpr int AUTO_HIDE_MS = 2500;
  static constexpr double AUTO_HIDE_DEADZONE = 0.001;

  // Not adjusted by width/height/center.
  StateData m_state[2];
  StateData m_prev_state[2];

  int m_auto_hide_timer[2] = {AUTO_HIDE_MS, AUTO_HIDE_MS};

  SettingValue<double> m_yaw_setting;
  SettingValue<double> m_pitch_setting;
  SettingValue<double> m_vertical_offset_setting;

  SettingValue<bool> m_relative_setting;
  SettingValue<bool> m_autohide_setting;
};
}  // namespace ControllerEmu
