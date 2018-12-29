// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Tilt::Tilt(const std::string& name_)
    : ReshapableInput(name_, name_, GroupType::Tilt), m_last_update(Clock::now())
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Modifier")));

  // Set default input radius to the full 1.0 (no resizing)
  // Set default input shape to a square (no reshaping)
  // Max deadzone to 50%
  AddReshapingSettings(1.0, 0.5, 50);

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Angle"), 0.9, 0, 180));
}

Tilt::StateData Tilt::GetState(bool adjusted)
{
  ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  const ControlState modifier = controls[4]->control_ref->State();

  // Compute desired tilt:
  StateData target = Reshape(x, y, modifier);

  // Step the simulation. This is pretty ugly being here.
  const auto now = Clock::now();
  const auto ms_since_update =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_update).count();
  m_last_update = now;

  constexpr int MAX_DEG_PER_SEC = 360 * 2;
  const double MAX_STEP = MAX_DEG_PER_SEC / 180.0 * ms_since_update / 1000;

  // TODO: Allow wrap around from 1.0 to -1.0
  // (take the fastest route to target)

  const double diff_x = (target.x - m_tilt.x);
  m_tilt.x += std::min(MAX_STEP, std::abs(diff_x)) * ((diff_x < 0) ? -1 : 1);
  const double diff_y = (target.y - m_tilt.y);
  m_tilt.y += std::min(MAX_STEP, std::abs(diff_y)) * ((diff_y < 0) ? -1 : 1);

  return m_tilt;
}

ControlState Tilt::GetGateRadiusAtAngle(double ang) const
{
  const ControlState max_tilt_angle = numeric_settings[SETTING_MAX_ANGLE]->GetValue() / 1.8;
  return SquareStickGate(max_tilt_angle).GetRadiusAtAngle(ang);
}

}  // namespace ControllerEmu
