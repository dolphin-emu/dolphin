// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/MixedTriggers.h"

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
}

void MixedTriggers::GetState(u16* const digital, const u16* bitmasks, ControlState* analog)
{
  const size_t trigger_count = controls.size() / 2;

  for (size_t i = 0; i < trigger_count; ++i, ++bitmasks, ++analog)
  {
    if (controls[i]->control_ref->State() > numeric_settings[0]->GetValue())  // threshold
    {
      *analog = 1.0;
      *digital |= *bitmasks;
    }
    else
    {
      *analog = controls[i + trigger_count]->control_ref->State();
    }
  }
}
}  // namespace ControllerEmu
