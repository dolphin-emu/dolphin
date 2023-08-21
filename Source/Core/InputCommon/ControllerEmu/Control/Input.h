// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/Control/Control.h"

namespace ControllerEmu
{
class Input : public Control
{
public:
  Input(Translatability translate, std::string name, std::string ui_name);
  Input(Translatability translate, std::string name);
};
}  // namespace ControllerEmu
