// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/Control/Control.h"

#include <utility>
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Control::Control(std::unique_ptr<ControlReference> ref, const std::string& name_,
                 const std::string& ui_name_)
    : control_ref(std::move(ref)), name(name_), ui_name(ui_name_)
{
}

Control::Control(std::unique_ptr<ControlReference> ref, const std::string& name_)
    : Control(std::move(ref), name_, name_)
{
}

Control::~Control() = default;
}  // namespace ControllerEmu
