// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Cursor.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Cursor::Cursor(const std::string& name_)
    : ReshapableInput(name_, name_, GroupType::Cursor), m_last_update(Clock::now())
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(Translate, named_direction));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Hide")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Recenter")));

  // Default shape is a 1.0 square (no resizing/reshaping):
  AddReshapingSettings(1.0, 0.5, 50);

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Center"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Width"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Height"), 0.5));

  boolean_settings.emplace_back(std::make_unique<BooleanSetting>(_trans("Relative Input"), false));
  boolean_settings.emplace_back(std::make_unique<BooleanSetting>(_trans("Auto-Hide"), false));
}

Cursor::ReshapeData Cursor::GetReshapableState(bool adjusted)
{
  const ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  const ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  return Reshape(x, y, 0.0);
}

ControlState Cursor::GetGateRadiusAtAngle(double ang) const
{
  // TODO: Change this to 0.5 and adjust the math,
  // so pointer doesn't have to be clamped to the configured width/height?
  return SquareStickGate(1.0).GetRadiusAtAngle(ang);
}

Cursor::StateData Cursor::GetState(const bool adjusted)
{
  ControlState z = controls[4]->control_ref->State() - controls[5]->control_ref->State();

  if (!adjusted)
  {
    const auto raw_input = GetReshapableState(false);

    return {raw_input.x, raw_input.y, z};
  }

  const auto input = GetReshapableState(true);

  // TODO: Using system time is ugly.
  // Kill this after state is moved into wiimote rather than this class.
  const auto now = Clock::now();
  const auto ms_since_update =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_update).count();
  m_last_update = now;

  const double max_step = STEP_PER_SEC / 1000.0 * ms_since_update;
  const double max_z_step = STEP_Z_PER_SEC / 1000.0 * ms_since_update;

  // Apply deadzone to z:
  const ControlState deadzone = numeric_settings[SETTING_DEADZONE]->GetValue();
  z = std::copysign(std::max(0.0, std::abs(z) - deadzone) / (1.0 - deadzone), z);

  // Smooth out z movement:
  // FYI: Not using relative input for Z.
  m_state.z += MathUtil::Clamp(z - m_state.z, -max_z_step, max_z_step);

  // Relative input:
  if (boolean_settings[0]->GetValue())
  {
    // Recenter:
    if (controls[7]->control_ref->State() > BUTTON_THRESHOLD)
    {
      m_state.x = 0.0;
      m_state.y = 0.0;
    }
    else
    {
      m_state.x = MathUtil::Clamp(m_state.x + input.x * max_step, -1.0, 1.0);
      m_state.y = MathUtil::Clamp(m_state.y + input.y * max_step, -1.0, 1.0);
    }
  }
  // Absolute input:
  else
  {
    m_state.x = input.x;
    m_state.y = input.y;
  }

  StateData result = m_state;

  // Adjust cursor according to settings:
  result.x *= (numeric_settings[SETTING_WIDTH]->GetValue() * 2);
  result.y *= (numeric_settings[SETTING_HEIGHT]->GetValue() * 2);
  result.y += (numeric_settings[SETTING_CENTER]->GetValue() - 0.5);

  const bool autohide = boolean_settings[1]->GetValue();

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

  // If auto-hide time is up or hide button is held:
  if (!m_auto_hide_timer || controls[6]->control_ref->State() > BUTTON_THRESHOLD)
  {
    // TODO: Use NaN or something:
    result.x = 10000;
    result.y = 0;
  }

  return result;
}

}  // namespace ControllerEmu
