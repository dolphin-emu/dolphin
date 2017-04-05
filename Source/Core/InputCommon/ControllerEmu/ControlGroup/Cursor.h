// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Cursor : public ControlGroup
{
public:
  explicit Cursor(const std::string& name);

  void GetState(ControlState* x, ControlState* y, ControlState* z, bool adjusted = false);

private:
  // This is used to reduce the cursor speed for relative input
  // to something that makes sense with the default range.
  static constexpr double SPEED_MULTIPLIER = 0.04;

  ControlState m_x = 0.0;
  ControlState m_y = 0.0;
  ControlState m_z = 0.0;
};
}  // namespace ControllerEmu
