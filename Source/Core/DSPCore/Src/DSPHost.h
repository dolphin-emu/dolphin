// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _DSPHOST_H
#define _DSPHOST_H

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.
#define DEBUG_EXP 1

u8 DSPHost_ReadHostMemory(u32 addr);
void DSPHost_WriteHostMemory(u8 value, u32 addr);
bool DSPHost_OnThread();
bool DSPHost_Running();
void DSPHost_InterruptRequest();
u32 DSPHost_CodeLoaded(const u8 *ptr, int size);
void DSPHost_UpdateDebugger();

#endif
