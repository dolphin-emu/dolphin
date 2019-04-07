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

  AddSetting(&m_distance_setting,
             {_trans("Distance"),
              // i18n: The symbol/abbreviation for centimeters.
              _trans("cm"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Distance of travel from neutral position.")},
             25, 0, 100);

  // These speed settings are used to calculate a maximum jerk (change in acceleration).
  // The calculation uses a travel distance of 1 meter.
  // The maximum value of 40 m/s is the approximate speed of the head of a golf club.
  // Games seem to not even properly detect motions at this speed.
  // Values of 1 to 40 m/s result in a jerk from 4 to 256,000.

  AddSetting(&m_speed_setting,
             {_trans("Speed"),
              // i18n: The symbol/abbreviation for meters per second.
              _trans("m/s"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Peak velocity of outward swing movements.")},
             8, 1, 40);

  // "Return Speed" allows for a "slow return" that won't trigger additional actions.

  AddSetting(&m_return_speed_setting,
             {_trans("Return Speed"),
              // i18n: The symbol/abbreviation for meters per second.
              _trans("m/s"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Peak velocity of movements to neutral position.")},
             2, 1, 40);

  AddSetting(&m_angle_setting,
             {_trans("Angle"),
              // i18n: The symbol/abbreviation for degrees (unit of angular measure).
              _trans("°"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Rotation applied at extremities of swing.")},
             45, 0, 180);
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
    // Apply deadzone to z and scale.
    const ControlState deadzone = GetDeadzonePercentage();

    z = std::copysign(std::max(0.0, std::abs(z) - deadzone) / (1.0 - deadzone), z) *
        GetMaxDistance();
  }

  return {float(state.x), float(state.y), float(z)};
}

ControlState Force::GetGateRadiusAtAngle(double) const
{
  // Just a circle of the configured distance:
  return GetMaxDistance();
}

ControlState Force::GetSpeed() const
{
  return m_speed_setting.GetValue();
}

ControlState Force::GetReturnSpeed() const
{
  return m_return_speed_setting.GetValue();
}

ControlState Force::GetTwistAngle() const
{
  return m_angle_setting.GetValue() * MathUtil::TAU / 360;
}

ControlState Force::GetMaxDistance() const
{
  return m_distance_setting.GetValue() / 100;
}

ControlState Force::GetDefaultInputRadiusAtAngle(double) const
{
  // Just a circle of radius 1.0.
  return 1.0;
}

}  // namespace ControllerEmu
