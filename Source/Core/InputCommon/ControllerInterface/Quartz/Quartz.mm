// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDLib.h>

#include "Common/StringUtil.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

#include <map>

namespace ciface
{
namespace Quartz
{
void Init(void* window)
{
  g_controller_interface.AddDevice(std::make_shared<KeyboardAndMouse>(window));
}

void DeInit()
{
}
}
}
