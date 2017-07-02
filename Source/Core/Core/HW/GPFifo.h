// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

class PointerWrap;

namespace GPFifo
{
enum
{
  GATHER_PIPE_SIZE = 32
};

// pipe pointer for JIT access
extern u8* g_gather_pipe_ptr;

// Init
void Init();
void DoState(PointerWrap& p);

// ResetGatherPipe
void ResetGatherPipe();
void CheckGatherPipe();
void FastCheckGatherPipe();

bool IsEmpty();

// Write
void Write8(u8 value);
void Write16(u16 value);
void Write32(u32 value);
void Write64(u64 value);

// These expect pre-byteswapped values
// Also there's an upper limit of about 512 per batch
// Most likely these should be inlined into JIT instead
void FastWrite8(u8 value);
void FastWrite16(u16 value);
void FastWrite32(u32 value);
void FastWrite64(u64 value);
}
