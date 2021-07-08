// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <ratio>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/Core.h"

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace ControllerEmu
{
using milli_with_remainder = std::chrono::duration<double, std::milli>;

Cursor::Cursor(std::string name_, std::string ui_name_)
    : ReshapableInput(std::move(name_), std::move(ui_name_), GroupType::Cursor)
{
  for (auto& named_direction : named_directions)
    AddInput(Translate, named_direction);

  AddInput(Translate, _trans("Hide"));
  AddInput(Translate, _trans("Recenter"));

  AddInput(Translate, _trans("Relative Input Hold"));

  // Default values chosen to reach screen edges in most games including the Wii Menu.

  AddSetting(&m_vertical_offset_setting,
             // i18n: Refers to a positional offset applied to an emulated wiimote.
             {_trans("Vertical Offset"),
              // i18n: The symbol/abbreviation for centimeters.
              _trans("cm")},
             10, -100, 100);

  AddSetting(&m_yaw_setting,
             // i18n: Refers to an amount of rotational movement about the "yaw" axis.
             {_trans("Total Yaw"),
              // i18n: The symbol/abbreviation for degrees (unit of angular measure).
              _trans("°"),
              // i18n: Refers to emulated wii remote movements.
              _trans("Total rotation about the yaw axis.")},
             25, 0, 360);

  AddSetting(&m_pitch_setting,
             // i18n: Refers to an amount of rotational movement about the "pitch" axis.
             {_trans("Total Pitch"),
              // i18n: The symbol/abbreviation for degrees (unit of angular measure).
              _trans("°"),
              // i18n: Refers to emulated wii remote movements.
              _trans("Total rotation about the pitch axis.")},
             20, 0, 360);

  AddSetting(&m_relative_setting, {_trans("Relative Input")}, false);
  AddSetting(&m_autohide_setting, {_trans("Auto-Hide")}, false);
}

Cursor::ReshapeData Cursor::GetReshapableState(bool adjusted) const
{
  const ControlState y = controls[0]->GetState() - controls[1]->GetState();
  const ControlState x = controls[3]->GetState() - controls[2]->GetState();

  // Return raw values
  if (!adjusted)
    return {x, y};

  // Values are clamped later on, the maximum movement between two frames
  // should not be clamped in relative mode
  return Reshape(x, y, 0.0, std::numeric_limits<ControlState>::infinity());
}

ControlState Cursor::GetGateRadiusAtAngle(double ang) const
{
  return SquareStickGate(1.0).GetRadiusAtAngle(ang);
}

// TODO: pass in the state as reference and let wiimote and UI keep their own state.
Cursor::StateData Cursor::GetState(bool is_ui, float time_elapsed)
{
  const int i = is_ui ? 1 : 0;

  const auto input = GetReshapableState(true);

  // Relative input (the second check is for Hold):
  if (m_relative_setting.GetValue() ^ controls[6]->GetState<bool>())
  {
    // Recenter:
    if (controls[5]->GetState<bool>())
    {
      m_state[i].x = 0.0;
      m_state[i].y = 0.0;
    }
    else
    {
      // Here we have two choices: to divide by the emulation speed or not.
      // The first one would be good if the relative input is mapped to buttons
      // or an analog stick, the second one would be good for a mouse or touch pad.
      // In general, this is more likely to be mapped to a mouse, but if not, users
      // can always use input expressions to pre-divide their input by the emu speed.
      const double step = STEP_PER_SEC * time_elapsed;

      m_state[i].x += input.x * step;
      m_state[i].y += input.y * step;
    }
  }
  // Absolute input:
  else
  {
    m_state[i].x = input.x;
    m_state[i].y = input.y;
  }

  // Clamp between -1 and 1, before auto hide. Clamping after auto hide could make it easier to find
  // when you've lost the cursor but it could also make it more annoying.
  // If we don't do this, we could go over the user specified angles (which can just be increased
  // instead), or lose the cursor over the borders.
  m_state[i].x = std::clamp(m_state[i].x, -1.0, 1.0);
  m_state[i].y = std::clamp(m_state[i].y, -1.0, 1.0);

  StateData result = m_state[i];

  const bool autohide = m_autohide_setting.GetValue();
  const bool has_moved = std::abs(m_prev_state[i].x - result.x) > AUTO_HIDE_DEADZONE ||
                         std::abs(m_prev_state[i].y - result.y) > AUTO_HIDE_DEADZONE;

  // Auto-hide timer (ignores Z):
  if (!autohide || has_moved)
  {
    m_auto_hide_timer[i] = AUTO_HIDE_MS;
  }
  if (autohide && !has_moved && m_auto_hide_timer[i] > 0)
  {
    // Auto should be based on real world time, independent of emulation speed
    const float emulation_speed = is_ui ? 1.f : static_cast<float>(Core::GetActualEmulationSpeed());
    m_auto_hide_timer[i] -= static_cast<int>((time_elapsed * 1000.f) / emulation_speed);
    m_auto_hide_timer[i] = std::max(m_auto_hide_timer[i], 0);
  }

  m_prev_state[i] = result;

  // If auto-hide time is up or hide button is held:
  if (m_auto_hide_timer[i] <= 0 || controls[4]->GetState<bool>())
  {
    result.x = std::numeric_limits<ControlState>::quiet_NaN();
    result.y = 0;
  }

  return result;
}

void Cursor::ResetState(bool is_ui)
{
  const int i = is_ui ? 1 : 0;

  m_state[i] = {};
  m_prev_state[i] = {};

  m_auto_hide_timer[i] = AUTO_HIDE_MS;
}

ControlState Cursor::GetTotalYaw() const
{
  return m_yaw_setting.GetValue() * MathUtil::TAU / 360;
}

ControlState Cursor::GetTotalPitch() const
{
  return m_pitch_setting.GetValue() * MathUtil::TAU / 360;
}

ControlState Cursor::GetVerticalOffset() const
{
  return m_vertical_offset_setting.GetValue() / 100;
}

bool Cursor::StateData::IsVisible() const
{
  return !std::isnan(x);
}

}  // namespace ControllerEmu
