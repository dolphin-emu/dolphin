// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class IRPassthrough : public ControlGroup
{
public:
  IRPassthrough(std::string name, std::string ui_name);

  ControlState GetObjectPositionX(size_t object_index) const;
  ControlState GetObjectPositionY(size_t object_index) const;
  ControlState GetObjectSize(size_t object_index) const;
};
}  // namespace ControllerEmu
