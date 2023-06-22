// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cmath>
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
  void GetState(C* const buttons, const C* bitmasks) const
  {
    for (auto& control : controls)
      *buttons |= *(bitmasks++) * control->GetState<bool>();
  }

  template <typename C>
  void GetState(C* const buttons, const C* bitmasks,
                const InputOverrideFunction& override_func) const
  {
    if (!override_func)
      return GetState(buttons, bitmasks);

    for (auto& control : controls)
    {
      ControlState state = control->GetState();
      if (std::optional<ControlState> state_override = override_func(name, control->name, state))
        state = *state_override;
      *buttons |= *(bitmasks++) * (std::lround(state) > 0);
    }
  }
};
}  // namespace ControllerEmu
