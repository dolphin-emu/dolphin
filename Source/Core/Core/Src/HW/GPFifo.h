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

#ifndef _GPFIFO_H
#define _GPFIFO_H

#include "Common.h"
class PointerWrap;

namespace GPFifo
{

enum
{
	GATHER_PIPE_SIZE = 32
};

extern u8 m_gatherPipe[GATHER_PIPE_SIZE*16]; //more room, for the fastmodes

// pipe counter
extern u32 m_gatherPipeCount;

// Init
void Init();
void DoState(PointerWrap &p);

// ResetGatherPipe
void ResetGatherPipe();
void CheckGatherPipe();

bool IsEmpty();

// Write
void HWCALL Write8(const u8 _iValue, const u32 _iAddress);
void HWCALL Write16(const u16 _iValue, const u32 _iAddress);
void HWCALL Write32(const u32 _iValue, const u32 _iAddress);

// These expect pre-byteswapped values
// Also there's an upper limit of about 512 per batch
// Most likely these should be inlined into JIT instead
void HWCALL FastWrite8(const u8 _iValue);
void HWCALL FastWrite16(const u16 _iValue);
void HWCALL FastWrite32(const u32 _iValue);

};

#endif
