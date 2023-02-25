// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/Control/Control.h"

namespace ControllerEmu
{
class Output : public Control
{
public:
  Output(Translatability translate, std::string name);
};
}  // namespace ControllerEmu
