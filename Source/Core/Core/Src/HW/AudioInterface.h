// Copyright (C) 2003-2008 Dolphin Project.

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
#ifndef _AUDIOINTERFACE_H
#define _AUDIOINTERFACE_H

namespace AudioInterface
{

// Init
void Init();

// Shutdown
void Shutdown();	

// Update
void Update();

// Calls by DSP plugin
unsigned __int32 Callback_GetStreaming(short* _pDestBuffer, unsigned __int32 _numSamples);

// Read32
void HWCALL Read32(u32& _uReturnValue, const u32 _iAddress);

// Write32
void HWCALL Write32(const u32 _iValue, const u32 _iAddress);

// Get the Audio Rate (48000 or 32000)
u32 GetRate();

} // end of namespace AudioInterface

#endif

