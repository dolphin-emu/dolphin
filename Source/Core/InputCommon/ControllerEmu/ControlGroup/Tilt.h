// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
class Tilt : public ControlGroup
{
public:
  explicit Tilt(const std::string& name);

  void GetState(ControlState* x, ControlState* y, bool step = true);

private:
  ControlState m_tilt[2];
};
}  // namespace ControllerEmu
