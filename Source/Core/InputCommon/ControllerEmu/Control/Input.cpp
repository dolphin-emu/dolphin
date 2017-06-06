// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Control/Input.h"

#include <memory>
#include <string>
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Input::Input(const std::string& name_, const std::string& ui_name_)
    : Control(std::make_unique<InputReference>(), name_, ui_name_)
{
}

Input::Input(const std::string& name_) : Control(std::make_unique<InputReference>(), name_)
{
}
}  // namespace ControllerEmu
