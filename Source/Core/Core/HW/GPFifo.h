// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
class PointerWrap;

namespace GPFifo
{

enum
{
	GATHER_PIPE_SIZE = 32
};

extern u8 GC_ALIGNED32(m_gatherPipe[GATHER_PIPE_SIZE*16]); //more room, for the fastmodes

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
void Write8(const u8 _iValue, const u32 _iAddress);
void Write16(const u16 _iValue, const u32 _iAddress);
void Write32(const u32 _iValue, const u32 _iAddress);
void Write64(const u64 _iValue, const u32 _iAddress);

// These expect pre-byteswapped values
// Also there's an upper limit of about 512 per batch
// Most likely these should be inlined into JIT instead
void FastWrite8(const u8 _iValue);
void FastWrite16(const u16 _iValue);
void FastWrite32(const u32 _iValue);
void FastWrite64(const u64 _iValue);
}
