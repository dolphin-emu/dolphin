// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
MixedTriggers::MixedTriggers(const std::string& name_)
    : ControlGroup(name_, GroupType::MixedTriggers)
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Threshold"), 0.9));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0.0, 0, 25));
}

void MixedTriggers::GetState(u16* const digital, const u16* bitmasks, ControlState* analog,
                             bool adjusted) const
{
  const ControlState threshold = numeric_settings[SETTING_THRESHOLD]->GetValue();
  ControlState deadzone = numeric_settings[SETTING_DEADZONE]->GetValue();

  // Return raw values. (used in UI)
  if (!adjusted)
  {
    deadzone = 0.0;
  }

  const int trigger_count = int(controls.size() / 2);
  for (int i = 0; i != trigger_count; ++i)
  {
    ControlState button_value = controls[i]->control_ref->State();
    ControlState analog_value = controls[trigger_count + i]->control_ref->State();

    // Apply deadzone:
    analog_value = std::max(0.0, analog_value - deadzone) / (1.0 - deadzone);
    button_value = std::max(0.0, button_value - deadzone) / (1.0 - deadzone);

    // Apply threshold:
    if (button_value > threshold)
    {
      // Fully activate analog:
      analog_value = 1.0;

      // Activate button:
      *digital |= bitmasks[i];
    }

    analog[i] = analog_value;
  }
}

ControlState MixedTriggers::GetDeadzone() const
{
  return numeric_settings[SETTING_DEADZONE]->GetValue();
}

ControlState MixedTriggers::GetThreshold() const
{
  return numeric_settings[SETTING_THRESHOLD]->GetValue();
}

}  // namespace ControllerEmu
