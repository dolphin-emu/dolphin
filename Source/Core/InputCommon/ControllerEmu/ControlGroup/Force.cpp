// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Force.h"

#include <string>

#include "Common/Common.h"
#include "Common/MathUtil.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Force::Force(const std::string& name_) : ReshapableInput(name_, name_, GroupType::Force)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Up")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Down")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));

  // Maximum swing movement (centimeters).
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Distance"), 0.25, 1, 100));

  // Maximum jerk (m/s^3).
  // i18n: "Jerk" as it relates to physics. The time derivative of acceleration.
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Jerk"), 5.0, 1, 1000));

  // Angle of twist applied at the extremities of the swing (degrees).
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Angle"), 0.45, 0, 180));
}

Force::ReshapeData Force::GetReshapableState(bool adjusted)
{
  const ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  const ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  return Reshape(x, y);
}

Force::StateData Force::GetState(bool adjusted)
{
  const auto state = GetReshapableState(adjusted);
  ControlState z = controls[4]->control_ref->State() - controls[5]->control_ref->State();

  if (adjusted)
  {
    // Apply deadzone to z.
    const ControlState deadzone = numeric_settings[SETTING_DEADZONE]->GetValue();
    z = std::copysign(std::max(0.0, std::abs(z) - deadzone) / (1.0 - deadzone), z);
  }

  return {float(state.x), float(state.y), float(z)};
}

ControlState Force::GetGateRadiusAtAngle(double) const
{
  // Just a circle of the configured distance:
  return numeric_settings[SETTING_DISTANCE]->GetValue();
}

ControlState Force::GetMaxJerk() const
{
  return numeric_settings[SETTING_JERK]->GetValue() * 100;
}

ControlState Force::GetTwistAngle() const
{
  return numeric_settings[SETTING_ANGLE]->GetValue() * MathUtil::TAU / 3.60;
}

ControlState Force::GetMaxDistance() const
{
  return numeric_settings[SETTING_DISTANCE]->GetValue();
}

ControlState Force::GetDefaultInputRadiusAtAngle(double) const
{
  // Just a circle of radius 1.0.
  return 1.0;
}

}  // namespace ControllerEmu
