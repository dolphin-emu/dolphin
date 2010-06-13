// Copyright (C) 2010 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "CommonTypes.h"
#include "GCPadStatus.h"
#include "../../InputCommon/Src/InputConfig.h"

void GCPad_Init( void* const hwnd );
void GCPad_Deinit();

void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus);
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength);

InputPlugin *PAD_GetPlugin();