// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Triggers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Triggers::Triggers(const std::string& name_) : ControlGroup(name_, GroupType::Triggers)
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

Triggers::StateData Triggers::GetState()
{
  const size_t trigger_count = controls.size();
  const ControlState deadzone = numeric_settings[0]->GetValue();

  StateData result(trigger_count);
  for (size_t i = 0; i < trigger_count; ++i)
    result.data[i] = std::max(controls[i]->control_ref->State() - deadzone, 0.0) / (1 - deadzone);

  return result;
}
}  // namespace ControllerEmu
