// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class Triggers : public ControlGroup
{
public:
  struct StateData
  {
    StateData() = default;
    explicit StateData(std::size_t trigger_count) : data(trigger_count) {}

    std::vector<ControlState> data;
  };

  explicit Triggers(const std::string& name);

  StateData GetState() const;
  StateData GetState(const InputOverrideFunction& override_func) const;

private:
  SettingValue<double> m_deadzone_setting;
};
}  // namespace ControllerEmu
