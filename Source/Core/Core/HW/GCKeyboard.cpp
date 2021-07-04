// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCKeyboard.h"

#include <cstring>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

#include "Core/HW/GCKeyboardEmu.h"

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/KeyboardStatus.h"

namespace Keyboard
{
static InputConfig s_config("GCKeyNew", _trans("Keyboard"), "GCKey");
InputConfig* GetConfig()
{
  return &s_config;
}

void Shutdown()
{
  s_config.UnregisterHotplugCallback();

  s_config.ClearControllers();
}

void Initialize()
{
  if (s_config.ControllersNeedToBeCreated())
  {
    for (unsigned int i = 0; i < 4; ++i)
      s_config.CreateController<GCKeyboard>(i);
  }

  s_config.RegisterHotplugCallback();

  // Load the saved controller config
  s_config.LoadConfig(InputConfig::InputClass::GC);
}

void LoadConfig()
{
  s_config.LoadConfig(InputConfig::InputClass::GC);
}

ControllerEmu::ControlGroup* GetGroup(int port, KeyboardGroup group)
{
  return static_cast<GCKeyboard*>(s_config.GetController(port))->GetGroup(group);
}

KeyboardStatus GetStatus(int port)
{
  return static_cast<GCKeyboard*>(s_config.GetController(port))->GetInput();
}
}  // namespace Keyboard
