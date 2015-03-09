// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/KeyboardStatus.h"

#pragma once

namespace Keyboard
{

void Shutdown();
void Initialize(void* const hwnd);
void LoadConfig();

InputConfig* GetConfig();

void GetStatus(u8 _port, KeyboardStatus* _pKeyboardStatus);

}
