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

namespace ControllerEmu
{
MixedTriggers::MixedTriggers(const std::string& name_)
    : ControlGroup(name_, GroupType::MixedTriggers)
{
  AddDeadzoneSetting(&m_deadzone_setting, 25);

  AddSetting(&m_threshold_setting,
             {_trans("Threshold"),
              // i18n: The percent symbol.
              _trans("%"),
              // i18n: Refers to the "threshold" setting for pressure sensitive gamepad inputs.
              _trans("Input strength required for activation.")},
             90, 0, 100);
}

void MixedTriggers::GetState(u16* const digital, const u16* bitmasks, ControlState* analog,
                             bool adjusted) const
{
  const ControlState threshold = GetThreshold();
  ControlState deadzone = GetDeadzone();

  // Return raw values. (used in UI)
  if (!adjusted)
  {
    deadzone = 0.0;
  }

  const int trigger_count = int(controls.size() / 2);
  for (int i = 0; i != trigger_count; ++i)
  {
    const ControlState button_value = ApplyDeadzone(controls[i]->control_ref->State(), deadzone);
    ControlState analog_value =
        ApplyDeadzone(controls[trigger_count + i]->control_ref->State(), deadzone);

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
  return m_deadzone_setting.GetValue() / 100;
}

ControlState MixedTriggers::GetThreshold() const
{
  return m_threshold_setting.GetValue() / 100;
}

}  // namespace ControllerEmu
