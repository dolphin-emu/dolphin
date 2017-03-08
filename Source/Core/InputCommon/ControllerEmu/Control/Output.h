// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/Control/Control.h"

namespace ControllerEmu
{
class Output : public Control
{
public:
  explicit Output(const std::string& name);
};
}  // namespace ControllerEmu
