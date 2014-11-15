// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/InputConfig.h"

#pragma once

namespace Pad
{

void Shutdown();
void Initialize(void* const hwnd);

InputConfig* GetConfig();

void GetStatus(u8 _numPAD, GCPadStatus* _pPADStatus);
void Rumble(u8 _numPAD, const ControlState strength);

bool GetMicButton(u8 pad);
}
