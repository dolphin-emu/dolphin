// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GBAPad.h"

#include "Core/HW/GBAPadEmu.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

namespace Pad
{
static InputConfig s_config("GBA", _trans("Pad"), "GBA", "GBA");
InputConfig* GetGBAConfig()
{
  return &s_config;
}

void ShutdownGBA()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();
}

void InitializeGBA()
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = 0; i < 4; ++i)
      s_config.CreateController<GBAPad>(i);
  }

  s_config.RegisterHotplugCallback();

  // Load the saved controller config
  s_config.LoadConfig();
}

void LoadGBAConfig()
{
  s_config.LoadConfig();
}

bool IsGBAInitialized()
{
  return !s_config.ControllersNeedToBeCreated();
}

GCPadStatus GetGBAStatus(int pad_num)
{
  return static_cast<GBAPad*>(s_config.GetController(pad_num))->GetInput();
}

void SetGBAReset(int pad_num, bool reset)
{
  static_cast<GBAPad*>(s_config.GetController(pad_num))->SetReset(reset);
}

ControllerEmu::ControlGroup* GetGBAGroup(int pad_num, GBAPadGroup group)
{
  return static_cast<GBAPad*>(s_config.GetController(pad_num))->GetGroup(group);
}
}  // namespace Pad
