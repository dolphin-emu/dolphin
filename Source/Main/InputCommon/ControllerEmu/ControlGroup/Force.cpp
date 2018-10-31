// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Force.h"

#include <cmath>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Force::Force(const std::string& name_) : ControlGroup(name_, GroupType::Force)
{
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Up")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Down")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Left")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Right")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(Translate, _trans("Backward")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

Force::StateData Force::GetState()
{
  StateData state_data;
  const ControlState deadzone = numeric_settings[0]->GetValue();

  for (u32 i = 0; i < 6; i += 2)
  {
    const ControlState state =
        controls[i + 1]->control_ref->State() - controls[i]->control_ref->State();

    ControlState tmpf = 0;
    if (fabs(state) > deadzone)
      tmpf = ((state - (deadzone * sign(state))) / (1 - deadzone));

    state_data[i / 2] = tmpf;
  }

  return state_data;
}
}  // namespace ControllerEmu
