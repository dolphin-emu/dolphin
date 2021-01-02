// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Control/Output.h"

#include <memory>
#include <string>
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Output::Output(Translatability translate_, std::string name_, ControlState range)
    : Control(std::make_unique<OutputReference>(range), translate_, std::move(name_))
{
}
}  // namespace ControllerEmu
