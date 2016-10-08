// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/GCPadEmu.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Pad
{
static InputConfig s_config("GCPadNew", _trans("Pad"), "GCPad");
InputConfig* GetConfig()
{
  return &s_config;
}

void Shutdown()
{
  s_config.ClearControllers();

  g_controller_interface.Shutdown();
}

void Initialize(void* const hwnd)
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = 0; i < 4; ++i)
      s_config.CreateController<GCPad>(i);
  }

  g_controller_interface.Initialize(hwnd);
  g_controller_interface.RegisterHotplugCallback(LoadConfig);

  // Load the saved controller config
  s_config.LoadConfig(true);
}

void LoadConfig()
{
  s_config.LoadConfig(true);
}

GCPadStatus GetStatus(int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetInput();
}

void Rumble(const int pad_num, const ControlState strength)
{
  static_cast<GCPad*>(s_config.GetController(pad_num))->SetOutput(strength);
}

bool GetMicButton(const int pad_num)
{
  return static_cast<GCPad*>(s_config.GetController(pad_num))->GetMicButton();
}
}
