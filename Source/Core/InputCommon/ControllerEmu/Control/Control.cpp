// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/Control/Control.h"

#include <utility>
#include "InputCommon/ControlReference/ControlReference.h"

namespace ControllerEmu
{
Control::Control(std::unique_ptr<ControlReference> ref, Translatability translate_,
                 std::string name_, std::string ui_name_)
    : control_ref(std::move(ref)), translate(translate_), name(std::move(name_)),
      ui_name(std::move(ui_name_))
{
}

Control::Control(std::unique_ptr<ControlReference> ref, Translatability translate_,
                 std::string name_)
    : control_ref(std::move(ref)), translate(translate_), name(name_), ui_name(std::move(name_))
{
}

Control::~Control() = default;
}  // namespace ControllerEmu
