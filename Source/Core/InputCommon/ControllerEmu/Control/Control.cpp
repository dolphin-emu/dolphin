// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Control::Control(ControlReference* ref, const std::string& name_) : control_ref(ref), name(name_)
{
}

Control::~Control() = default;
}  // namespace ControllerEmu
