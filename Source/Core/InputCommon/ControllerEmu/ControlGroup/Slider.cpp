// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Slider.h"

#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Slider::Slider(const std::string& name, const std::string& ui_name)
    : ControlGroup(name, ui_name, GroupType::Slider)
{
  controls.emplace_back(std::make_unique<Input>("Left"));
  controls.emplace_back(std::make_unique<Input>("Right"));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

Slider::Slider(const std::string& name) : Slider(name, name)
{
}

void Slider::GetState(ControlState* const slider)
{
  const ControlState deadzone = numeric_settings[0]->GetValue();
  const ControlState state = controls[1]->control_ref->State() - controls[0]->control_ref->State();

  if (fabs(state) > deadzone)
    *slider = (state - (deadzone * sign(state))) / (1 - deadzone);
  else
    *slider = 0;
}
}  // namespace ControllerEmu
