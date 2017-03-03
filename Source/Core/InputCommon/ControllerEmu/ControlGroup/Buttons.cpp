// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Threshold"), 0.5));
}
}  // namespace ControllerEmu
