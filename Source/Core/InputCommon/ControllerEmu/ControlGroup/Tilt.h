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
class Tilt : public ControlGroup
{
public:
  explicit Tilt(const std::string& name);

  void GetState(ControlState* x, ControlState* y, bool step = true);

private:
  std::array<ControlState, 2> m_tilt{};
};
}  // namespace ControllerEmu
