// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Triggers : public ControlGroup
{
public:
  explicit Triggers(const std::string& name);

  void GetState(ControlState* analog);
};
}  // namespace ControllerEmu
