// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

namespace DSP::Host
{
u8 ReadHostMemory(u32 addr);
void WriteHostMemory(u8 value, u32 addr);
void DMAToDSP(u16* dst, u32 addr, u32 size);
void DMAFromDSP(const u16* src, u32 addr, u32 size);
void OSD_AddMessage(std::string str, u32 ms);
bool OnThread();
bool IsWiiHost();
void InterruptRequest();
void CodeLoaded(u32 addr, size_t size);
void CodeLoaded(const u8* ptr, size_t size);
void UpdateDebugger();
}  // namespace DSP::Host
