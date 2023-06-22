// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

class InputConfig;
enum class KeyboardGroup;
struct KeyboardStatus;

namespace ControllerEmu
{
class ControlGroup;
}

namespace Keyboard
{
void Shutdown();
void Initialize();
void LoadConfig();

InputConfig* GetConfig();
ControllerEmu::ControlGroup* GetGroup(int port, KeyboardGroup group);

KeyboardStatus GetStatus(int port);
}  // namespace Keyboard
