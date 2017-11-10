// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/Win32/Win32.h"

#include "InputCommon/ControllerInterface/DInput/DInput.h"
#include "InputCommon/ControllerInterface/XInput/XInput.h"

void ciface::Win32::Init()
{
  // DInput::Init();
  XInput::Init();
}

void ciface::Win32::PopulateDevices(void* hwnd)
{
  DInput::PopulateDevices(static_cast<HWND>(hwnd));
  XInput::PopulateDevices();
}

void ciface::Win32::DeInit()
{
  // DInput::DeInit();
  XInput::DeInit();
}
