// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Cursor::Cursor(std::string name_, std::string ui_name_)
    : ReshapableInput(std::move(name_), std::move(ui_name_), GroupType::Cursor),
      m_last_update(Clock::now())
{
  for (auto& named_direction : named_directions)
    AddInput(Translatability::Translate, named_direction);

  AddInput(Translatability::Translate, _trans("Hide"));
  AddInput(Translatability::Translate, _trans("Recenter"));

  AddInput(Translatability::Translate, _trans("Relative Input Hold"));

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

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  return Reshape(x, y, 0.0);
}

ControlState Cursor::GetGateRadiusAtAngle(double ang) const
{
  return SquareStickGate(1.0).GetRadiusAtAngle(ang);
}

Cursor::StateData Cursor::GetState(const bool adjusted)
{
  const ReshapeData input = GetReshapableState(adjusted);
  const StateData state = adjusted ? UpdateState(input) : StateData{input.x, input.y};
  return state;
}

Cursor::StateData Cursor::GetState(const bool adjusted,
                                   const ControllerEmu::InputOverrideFunction& override_func)
{
  StateData state = GetState(adjusted);
  if (!override_func)
    return state;

  if (const std::optional<ControlState> x_override = override_func(name, X_INPUT_OVERRIDE, state.x))
    state.x = *x_override;
  if (const std::optional<ControlState> y_override = override_func(name, Y_INPUT_OVERRIDE, state.y))
    state.y = *y_override;

  return state;
}

Cursor::StateData Cursor::UpdateState(Cursor::ReshapeData input)
{
  // TODO: Using system time is ugly.
  // Kill this after state is moved into wiimote rather than this class.
  const auto now = Clock::now();
  const auto ms_since_update =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_update).count();
  m_last_update = now;

  const double max_step = STEP_PER_SEC / 1000.0 * ms_since_update;

  // Relative input:
  if (m_relative_setting.GetValue() ^ (controls[6]->GetState<bool>()))
  {
    // Recenter:
    if (controls[5]->GetState<bool>())
    {
      m_state.x = 0.0;
      m_state.y = 0.0;
    }
    else
    {
      m_state.x = std::clamp(m_state.x + input.x * max_step, -1.0, 1.0);
      m_state.y = std::clamp(m_state.y + input.y * max_step, -1.0, 1.0);
    }
  }
  // Absolute input:
  else
  {
    m_state.x = input.x;
    m_state.y = input.y;
  }

  StateData result = m_state;

  const bool autohide = m_autohide_setting.GetValue();

  // Auto-hide timer:
  // TODO: should Z movement reset this?
  if (!autohide || std::abs(m_prev_result.x - result.x) > AUTO_HIDE_DEADZONE ||
      std::abs(m_prev_result.y - result.y) > AUTO_HIDE_DEADZONE)
  {
    m_auto_hide_timer = AUTO_HIDE_MS;
  }
  else if (m_auto_hide_timer)
  {
    m_auto_hide_timer -= std::min<int>(ms_since_update, m_auto_hide_timer);
  }

  m_prev_result = result;

  // If auto-hide time is up, the hide button is held, or the input gate is disabled, hide the
  // cursor.  We need to check the input gate explicitly as the hide button check always returns
  // false if the input gate is disabled (e.g. the window is not focused with background input
  // disabled)
  if (!m_auto_hide_timer || !ControlReference::GetInputGate() || controls[4]->GetState<bool>())
  {
    result.x = std::numeric_limits<ControlState>::quiet_NaN();
    result.y = 0;
  }

  return result;
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

void Cursor::SetRelativeInput(bool value)
{
  m_relative_setting.SetValue(value);
}

bool Cursor::StateData::IsVisible() const
{
  return !std::isnan(x);
}

}  // namespace ControllerEmu
