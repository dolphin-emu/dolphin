// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class AnalogStick : public ControlGroup
{
public:
  enum
  {
    SETTING_RADIUS,
    SETTING_DEADZONE,
  };

  // The GameCube controller and Wiimote attachments have a different default radius
  AnalogStick(const char* name, ControlState default_radius);
  AnalogStick(const char* name, const char* ui_name, ControlState default_radius);

  void GetState(ControlState* x, ControlState* y);
};
}  // namespace ControllerEmu
