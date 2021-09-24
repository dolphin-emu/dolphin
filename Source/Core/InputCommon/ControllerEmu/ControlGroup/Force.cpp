// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  AddInput(Translate, _trans("Up"));
  AddInput(Translate, _trans("Down"));
  AddInput(Translate, _trans("Left"));
  AddInput(Translate, _trans("Right"));
  AddInput(Translate, _trans("Forward"));
  AddInput(Translate, _trans("Backward"));

  AddSetting(&m_distance_setting,
             {_trans("Distance"),
              // i18n: The symbol/abbreviation for centimeters.
              _trans("cm"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Distance of travel from neutral position.")},
             50, 1, 100);

  // These speed settings are used to calculate a maximum jerk (change in acceleration).
  // The calculation uses a travel distance of 1 meter.
  // The maximum value of 40 m/s is the approximate speed of the head of a golf club.
  // Games seem to not even properly detect motions at this speed.
  // Values result in an exponentially increasing jerk.

  AddSetting(&m_speed_setting,
             {_trans("Speed"),
              // i18n: The symbol/abbreviation for meters per second.
              _trans("m/s"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Peak velocity of outward swing movements.")},
             16, 1, 40);

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
             90, 1, 180);
}

Force::ReshapeData Force::GetReshapableState(bool adjusted) const
{
  const ControlState y = controls[0]->GetState() - controls[1]->GetState();
  const ControlState x = controls[3]->GetState() - controls[2]->GetState();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  return Reshape(x, y);
}

Force::StateData Force::GetState(bool adjusted) const
{
  const auto state = GetReshapableState(adjusted);
  ControlState z = controls[4]->GetState() - controls[5]->GetState();

  if (adjusted)
  {
    // Apply deadzone to z and scale.
    z = ApplyDeadzone(z, GetDeadzonePercentage()) * GetMaxDistance();
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

Shake::Shake(const std::string& name_, ControlState default_intensity_scale)
    : ControlGroup(name_, name_, GroupType::Shake)
{
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  AddInput(ControllerEmu::Translate, _trans("X"));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  AddInput(ControllerEmu::Translate, _trans("Y"));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  AddInput(ControllerEmu::Translate, _trans("Z"));

  AddDeadzoneSetting(&m_deadzone_setting, 50);

  // Total travel distance in centimeters.
  // Negative values can be used to reverse the initial direction of movement.
  AddSetting(&m_intensity_setting,
             // i18n: Refers to the intensity of shaking an emulated wiimote.
             {_trans("Intensity"),
              // i18n: The symbol/abbreviation for centimeters.
              _trans("cm"),
              // i18n: Refering to emulated wii remote movement.
              _trans("Total travel distance.")},
             10 * default_intensity_scale, -50, 50);

  // Approximate number of up/down movements in one second.
  AddSetting(&m_frequency_setting,
             // i18n: Refers to a number of actions per second in Hz.
             {_trans("Frequency"),
              // i18n: The symbol/abbreviation for hertz (cycles per second).
              _trans("Hz"),
              // i18n: Refering to emulated wii remote movement.
              _trans("Number of shakes per second.")},
             6, 1, 20);
}

Shake::StateData Shake::GetState(bool adjusted) const
{
  const float x = controls[0]->GetState();
  const float y = controls[1]->GetState();
  const float z = controls[2]->GetState();

  StateData result = {x, y, z};

  // FYI: Unadjusted values are used in UI.
  if (adjusted)
    for (auto& c : result.data)
      c = ApplyDeadzone(c, GetDeadzone());

  return result;
}

ControlState Shake::GetDeadzone() const
{
  return m_deadzone_setting.GetValue() / 100;
}

ControlState Shake::GetIntensity() const
{
  return m_intensity_setting.GetValue() / 100;
}

ControlState Shake::GetFrequency() const
{
  return m_frequency_setting.GetValue();
}

}  // namespace ControllerEmu
