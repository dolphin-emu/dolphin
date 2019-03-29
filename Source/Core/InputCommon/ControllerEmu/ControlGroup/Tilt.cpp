// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Tilt.h"

#include <string>

#include "Common/Common.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Tilt::Tilt(const std::string& name_) : ReshapableInput(name_, name_, GroupType::Tilt)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Modifier")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Angle"), 0.9, 0, 180));
}

Tilt::ReshapeData Tilt::GetReshapableState(bool adjusted)
{
  const ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  const ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  // Return raw values. (used in UI)
  if (!adjusted)
    return {x, y};

  const ControlState modifier = controls[4]->control_ref->State();

  return Reshape(x, y, modifier);
}

Tilt::StateData Tilt::GetState()
{
  return GetReshapableState(true);
}

ControlState Tilt::GetGateRadiusAtAngle(double ang) const
{
  const ControlState max_tilt_angle = numeric_settings[SETTING_MAX_ANGLE]->GetValue() / 1.8;
  return SquareStickGate(max_tilt_angle).GetRadiusAtAngle(ang);
}

ControlState Tilt::GetDefaultInputRadiusAtAngle(double ang) const
{
  return SquareStickGate(1.0).GetRadiusAtAngle(ang);
}

}  // namespace ControllerEmu
