// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class InputConfig;
struct KeyboardStatus;

namespace Keyboard
{

void Shutdown();
void Initialize(void* const hwnd);
void LoadConfig();

InputConfig* GetConfig();

void GetStatus(u8 port, KeyboardStatus* keyboard_status);

}
