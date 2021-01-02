// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "InputCommon/ControllerEmu/Control/Control.h"

namespace ControllerEmu
{
class Input : public Control
{
public:
  Input(Translatability translate, std::string name, std::string ui_name,
        ControlState range = ControlReference::DEFAULT_RANGE);
  Input(Translatability translate, std::string name,
        ControlState range = ControlReference::DEFAULT_RANGE);
};
}  // namespace ControllerEmu
