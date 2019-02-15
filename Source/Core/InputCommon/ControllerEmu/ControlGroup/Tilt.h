// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Tilt : public ControlGroup
{
public:
  struct StateData
  {
    ControlState x{};
    ControlState y{};
  };

  explicit Tilt(const std::string& name);

  StateData GetState(bool step = true);

private:
  StateData m_tilt;
};
}  // namespace ControllerEmu
