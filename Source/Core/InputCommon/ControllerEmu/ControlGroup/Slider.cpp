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

namespace ControllerEmu
{
Slider::Slider(const std::string& name_, const std::string& ui_name_)
    : ControlGroup(name_, ui_name_, GroupType::Slider)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));

  AddDeadzoneSetting(&m_deadzone_setting, 50);
}

Slider::Slider(const std::string& name_) : Slider(name_, name_)
{
}

Slider::StateData Slider::GetState()
{
  const ControlState deadzone = m_deadzone_setting.GetValue() / 100;
  const ControlState state = controls[1]->control_ref->State() - controls[0]->control_ref->State();

  return {ApplyDeadzone(state, deadzone)};
}
}  // namespace ControllerEmu
