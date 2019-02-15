// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"

#include <string>

#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace ControllerEmu
{
Extension::Extension(const std::string& name_) : ControlGroup(name_, GroupType::Extension)
{
}
}  // namespace ControllerEmu
