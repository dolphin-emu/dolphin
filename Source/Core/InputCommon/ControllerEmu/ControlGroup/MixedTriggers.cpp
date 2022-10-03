// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"

#include <algorithm>
#include <cmath>
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
    const ControlState button_value = ApplyDeadzone(controls[i]->GetState(), deadzone);
    ControlState analog_value =
        std::min(ApplyDeadzone(controls[trigger_count + i]->GetState(), deadzone), 1.0);

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

void MixedTriggers::GetState(u16* digital, const u16* bitmasks, ControlState* analog,
                             const InputOverrideFunction& override_func, bool adjusted) const
{
  if (!override_func)
    return GetState(digital, bitmasks, analog, adjusted);

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
    bool button_bool = false;
    const ControlState button_value = ApplyDeadzone(controls[i]->GetState(), deadzone);
    ControlState analog_value = ApplyDeadzone(controls[trigger_count + i]->GetState(), deadzone);

    // Apply threshold:
    if (button_value > threshold)
    {
      analog_value = 1.0;
      button_bool = true;
    }

    if (const std::optional<ControlState> button_override =
            override_func(name, controls[i]->name, static_cast<ControlState>(button_bool)))
    {
      button_bool = std::lround(*button_override) > 0;
    }

    if (const std::optional<ControlState> analog_override =
            override_func(name, controls[trigger_count + i]->name, analog_value))
    {
      analog_value = *analog_override;
    }

    if (button_bool)
      *digital |= bitmasks[i];
    analog[i] = std::min(analog_value, 1.0);
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

size_t MixedTriggers::GetTriggerCount() const
{
  return controls.size() / 2;
}

}  // namespace ControllerEmu
