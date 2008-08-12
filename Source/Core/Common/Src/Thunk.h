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

#ifndef _THUNK_H
#define _THUNK_H

// This simple class creates a wrapper around a C/C++ function that saves all fp state
// before entering it, and restores it upon exit. This is required to be able to selectively
// call functions from generated code, without inflicting the performance hit and increase
// of complexity that it means to protect the generated code from this problem.

// This process is called thunking.

// There will only ever be one level of thunking on the stack, plus,
// we don't want to pollute the stack, so we store away regs somewhere global.
// NOT THREAD SAFE. This may only be used from the CPU thread.
// Any other thread using this stuff will be FATAL.

void Thunk_Init();
void Thunk_Reset();
void Thunk_Shutdown();

void *ProtectFunction(void *function, int num_params);

#endif
