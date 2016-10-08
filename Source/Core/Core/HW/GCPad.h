// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/Device.h"

class InputConfig;
struct GCPadStatus;

namespace Pad
{
void Shutdown();
void Initialize(void* const hwnd);
void LoadConfig();

InputConfig* GetConfig();

GCPadStatus GetStatus(int pad_num);
void Rumble(int pad_num, ControlState strength);

bool GetMicButton(int pad_num);
}
