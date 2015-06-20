// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/Device.h"
#include "GCPadEmu.h"

class InputConfig;
struct GCPadStatus;

namespace Pad
{

void Shutdown();
void Initialize(void* const hwnd);
void LoadConfig();

InputConfig* GetConfig();

GCPad* InitLibDolphinVirt();
void GetStatus(u8 _numPAD, GCPadStatus* _pPADStatus);
void Rumble(u8 _numPAD, const ControlState strength);

bool GetMicButton(u8 pad);
}
