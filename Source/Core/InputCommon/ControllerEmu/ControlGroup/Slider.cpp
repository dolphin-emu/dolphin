// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  AddInput(Translate, _trans("Left"));
  AddInput(Translate, _trans("Right"));

  AddDeadzoneSetting(&m_deadzone_setting, 50);
}

Slider::Slider(const std::string& name_) : Slider(name_, name_)
{
}

Slider::StateData Slider::GetState() const
{
  const ControlState deadzone = m_deadzone_setting.GetValue() / 100;
  const ControlState state = controls[1]->GetState() - controls[0]->GetState();

  return {std::clamp(ApplyDeadzone(state, deadzone), -1.0, 1.0)};
}

Slider::StateData Slider::GetState(const InputOverrideFunction& override_func) const
{
  if (!override_func)
    return GetState();

  const ControlState deadzone = m_deadzone_setting.GetValue() / 100;
  ControlState state = controls[1]->GetState() - controls[0]->GetState();

  state = ApplyDeadzone(state, deadzone);

  if (std::optional<ControlState> state_override = override_func(name, X_INPUT_OVERRIDE, state))
    state = *state_override;

  return {std::clamp(state, -1.0, 1.0)};
}

}  // namespace ControllerEmu
