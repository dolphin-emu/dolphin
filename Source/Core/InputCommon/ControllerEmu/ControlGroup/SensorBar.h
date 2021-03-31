// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

namespace ControllerEmu
{
class SensorBar : public ControlGroup
{
public:
  struct StateData
  {
    ControlState x1{};
    ControlState y1{};
    float size1{};
    ControlState x2{};
    ControlState y2{};
    float size2{};
  };

  SensorBar(std::string name, std::string ui_name);

  StateData GetState();
};
}  // namespace ControllerEmu
