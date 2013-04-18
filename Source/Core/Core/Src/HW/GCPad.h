// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "CommonTypes.h"
#include "GCPadStatus.h"
#include "../../InputCommon/Src/InputConfig.h"

#ifndef _GCPAD_H_
#define _GCPAD_H_

namespace Pad
{

void Shutdown();
void Initialize(void* const hwnd);

InputPlugin *GetPlugin();

void GetStatus(u8 _numPAD, SPADStatus* _pPADStatus);
void Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);
void Motor(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);

bool GetMicButton(u8 pad);
}

#endif
