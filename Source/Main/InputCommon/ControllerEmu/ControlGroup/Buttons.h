// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
class Buttons : public ControlGroup
{
public:
  explicit Buttons(const std::string& name_);
  Buttons(const std::string& ini_name, const std::string& group_name);

  template <typename C>
  void GetState(C* const buttons, const C* bitmasks)
  {
    for (auto& control : controls)
    {
      if (control->control_ref->State() > numeric_settings[0]->GetValue())  // threshold
        *buttons |= *bitmasks;

      bitmasks++;
    }
  }
};
}  // namespace ControllerEmu
