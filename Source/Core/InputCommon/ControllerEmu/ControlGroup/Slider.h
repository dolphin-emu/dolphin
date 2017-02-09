// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

namespace ControllerEmu
{
class Slider : public ControlGroup
{
public:
  explicit Slider(const std::string& name);

  void GetState(ControlState* slider);
};
}  // namespace ControllerEmu
