// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

namespace ciface
{
namespace Quartz
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
}  // namespace Quartz
}  // namespace ciface
