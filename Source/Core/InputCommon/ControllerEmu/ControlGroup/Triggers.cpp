// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Triggers::Triggers(const std::string& name_) : ControlGroup(name_, GroupType::Triggers)
{
  AddDeadzoneSetting(&m_deadzone_setting, 50);
}

Triggers::StateData Triggers::GetState() const
{
  const size_t trigger_count = controls.size();
  const ControlState deadzone = m_deadzone_setting.GetValue() / 100;

  StateData result(trigger_count);
  for (size_t i = 0; i < trigger_count; ++i)
    result.data[i] = std::min(ApplyDeadzone(controls[i]->GetState(), deadzone), 1.0);

  return result;
}

Triggers::StateData Triggers::GetState(const InputOverrideFunction& override_func) const
{
  if (!override_func)
    return GetState();

  const size_t trigger_count = controls.size();
  const ControlState deadzone = m_deadzone_setting.GetValue() / 100;

  StateData result(trigger_count);
  for (size_t i = 0; i < trigger_count; ++i)
  {
    ControlState state = ApplyDeadzone(controls[i]->GetState(), deadzone);
    if (std::optional<ControlState> state_override = override_func(name, controls[i]->name, state))
      state = *state_override;
    result.data[i] = std::min(state, 1.0);
  }

  return result;
}

}  // namespace ControllerEmu
