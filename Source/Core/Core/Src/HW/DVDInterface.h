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

#ifndef _DVDINTERFACE_H
#define _DVDINTERFACE_H

#include "Common.h"
class PointerWrap;

namespace DVDInterface
{

void Init();
void Shutdown();
void DoState(PointerWrap &p);

void SetDiscInside(bool _DiscInside);
void SwapDisc(const char * fileName);

// Lid Functions
void SetLidOpen(bool open = true);
bool IsLidOpen();

// DVD Access Functions
bool DVDRead(u32 _iDVDOffset, u32 _iRamAddress, u32 _iLength);
bool DVDReadADPCM(u8* _pDestBuffer, u32 _iNumSamples);

// Read32
void Read32(u32& _uReturnValue, const u32 _iAddress);

// Write32
void Write32(const u32 _iValue, const u32 _iAddress);

} // end of namespace DVDInterface

#endif


