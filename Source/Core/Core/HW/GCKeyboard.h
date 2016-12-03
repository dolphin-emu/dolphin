// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu.h"

class InputConfig;
enum class KeyboardGroup;
struct KeyboardStatus;

namespace Keyboard
{
void Shutdown();
void Initialize();
void LoadConfig();

InputConfig* GetConfig();
ControllerEmu::ControlGroup* GetGroup(int port, KeyboardGroup group);

KeyboardStatus GetStatus(int port);
}
