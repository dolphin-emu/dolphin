// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSPHOST_H
#define _DSPHOST_H

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

u8 DSPHost_ReadHostMemory(u32 addr);
void DSPHost_WriteHostMemory(u8 value, u32 addr);
void DSPHost_OSD_AddMessage(const std::string& str, u32 ms);
bool DSPHost_OnThread();
bool DSPHost_Wii();
void DSPHost_InterruptRequest();
void DSPHost_CodeLoaded(const u8 *ptr, int size);
void DSPHost_UpdateDebugger();

#endif
