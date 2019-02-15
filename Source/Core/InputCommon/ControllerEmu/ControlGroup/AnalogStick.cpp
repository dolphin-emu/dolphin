// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/AnalogStick.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "Common/Common.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
AnalogStick::AnalogStick(const char* const name_, ControlState default_radius)
    : AnalogStick(name_, name_, default_radius)
{
}

AnalogStick::AnalogStick(const char* const name_, const char* const ui_name_,
                         ControlState default_radius)
    : ControlGroup(name_, ui_name_, GroupType::Stick)
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(Translate, named_direction));

  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Modifier")));
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Radius"), default_radius, 0, 100));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

AnalogStick::StateData AnalogStick::GetState()
{
  ControlState y = controls[0]->control_ref->State() - controls[1]->control_ref->State();
  ControlState x = controls[3]->control_ref->State() - controls[2]->control_ref->State();

  const ControlState radius = numeric_settings[SETTING_RADIUS]->GetValue();
  const ControlState deadzone = numeric_settings[SETTING_DEADZONE]->GetValue();
  const ControlState m = controls[4]->control_ref->State();

  const ControlState ang = atan2(y, x);
  const ControlState ang_sin = sin(ang);
  const ControlState ang_cos = cos(ang);

  ControlState dist = sqrt(x * x + y * y);

  // dead zone code
  dist = std::max(0.0, dist - deadzone);
  dist /= (1 - deadzone);

  // radius
  dist *= radius;

  // The modifier halves the distance by 50%, which is useful
  // for keyboard controls.
  if (m)
    dist *= 0.5;

  y = std::max(-1.0, std::min(1.0, ang_sin * dist));
  x = std::max(-1.0, std::min(1.0, ang_cos * dist));

  return {x, y};
}
}  // namespace ControllerEmu
