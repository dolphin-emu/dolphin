// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Force : public ControlGroup
{
public:
  explicit Force(const std::string& name);

  void GetState(ControlState* axis);

private:
  std::array<ControlState, 3> m_swing{};
};
}  // namespace ControllerEmu
