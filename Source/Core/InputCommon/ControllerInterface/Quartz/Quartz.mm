// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

namespace ciface::Quartz
{
void PopulateDevices(void* window)
{
  if (!window)
    return;

  g_controller_interface.AddDevice(std::make_shared<KeyboardAndMouse>(window));
}

void DeInit()
{
}
}  // namespace ciface::Quartz
