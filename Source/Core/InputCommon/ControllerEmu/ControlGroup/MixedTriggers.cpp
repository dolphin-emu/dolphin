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

size_t MixedTriggers::GetTriggerCount() const
{
  return controls.size() / 2;
}

void MixedTriggers::GetState(u16* const digital, const u16* bitmasks, ControlState* analog) const
{
  const ControlState threshold = GetThreshold();
  const ControlState deadzone = GetDeadzone();

  for (size_t i = 0, trigger_count = GetTriggerCount(); i != trigger_count; ++i)
  {
    // Apply threshold:
    const bool is_button_pressed = GetRawDigitalState(i) > threshold;

    // Activate button:
    *digital |= bitmasks[i] * is_button_pressed;

    // Apply deadzone and digital button override.
    analog[i] = std::max(std::min(ApplyDeadzone(GetRawAnalogState(i), deadzone), 1.0),
                         is_button_pressed * 1.0);
  }
}

ControlState MixedTriggers::GetRawDigitalState(size_t index) const
{
  return controls[index]->control_ref->State();
}

ControlState MixedTriggers::GetRawAnalogState(size_t index) const
{
  return controls[GetTriggerCount() + index]->control_ref->State();
}

ControlState MixedTriggers::GetDeadzone() const
{
  return m_deadzone_setting.GetValue() / 100;
}

ControlState MixedTriggers::GetThreshold() const
{
  return m_threshold_setting.GetValue() / 100;
}

bool MixedTriggers::IsAnalogEnabled() const
{
  return true;
}

}  // namespace ControllerEmu
