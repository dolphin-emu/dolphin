// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Buttons.h"

#include <memory>
#include <string>

#include "Common/Common.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
Buttons::Buttons(const std::string& name_) : Buttons(name_, name_)
{
}

Buttons::Buttons(const std::string& ini_name, const std::string& group_name)
    : ControlGroup(ini_name, group_name, GroupType::Buttons)
{
}

}  // namespace ControllerEmu
