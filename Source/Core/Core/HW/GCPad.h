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

InputPlugin *GetPlugin();

void GetStatus(u8 _numPAD, GCPadStatus* _pPADStatus);
void Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);
void Motor(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);

bool GetMicButton(u8 pad);
}
