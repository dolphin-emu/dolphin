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
Force::Force(std::string name, std::string ui_name)
    : ReshapableInput(std::move(name), std::move(ui_name), GroupType::Force)
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

  AddSetting(&m_jerk_setting,
             // i18n: "Jerk" as it relates to physics. The time derivative of acceleration.
             {_trans("Jerk"),
              // i18n: The symbol/abbreviation for meters per second to the 3rd power.
              _trans("m/s³"),
              // i18n: Refering to emulated wii remote swing movement.
              _trans("Maximum change in acceleration.")},
             500, 1, 1000);

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
    // Apply deadzone to z.
    z = ApplyDeadzone(z, GetDeadzonePercentage());
  }

  return {float(state.x), float(state.y), float(z)};
}

ControlState Force::GetGateRadiusAtAngle(double) const
{
  // Just a circle of the configured distance:
  return GetMaxDistance();
}

ControlState Force::GetMaxJerk() const
{
  return m_jerk_setting.GetValue();
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
  controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("X")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Y")));
  // i18n: Refers to a 3D axis (used when mapping motion controls)
  controls.emplace_back(new ControllerEmu::Input(ControllerEmu::Translate, _trans("Z")));

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
  const float x = controls[0]->control_ref->State();
  const float y = controls[1]->control_ref->State();
  const float z = controls[2]->control_ref->State();

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
