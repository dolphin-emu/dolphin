// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
class Force : public ControlGroup
{
public:
  explicit Force(const std::string& name);

  void GetState(ControlState* axis);

private:
  ControlState m_swing[3];
};
}  // namespace ControllerEmu
