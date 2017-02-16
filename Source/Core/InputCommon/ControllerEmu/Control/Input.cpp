// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Control/Input.h"

#include <string>
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Input::Input(const std::string& name_) : Control(new InputReference, name_)
{
}
}  // namespace ControllerEmu
