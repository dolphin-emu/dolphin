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

#ifndef _JITBACKPATCH_H
#define _JITBACKPATCH_H

#include "Common.h"

#ifdef _WIN32

#include <windows.h>

#else

// A bit of a hack to get things building under linux. We manually fill in this structure as needed
// from the real context.
struct CONTEXT
{
#ifdef _M_X64
	u64 Rip;
	u64 Rax;
#else
	u32 Eip;
	u32 Eax;
#endif 
};

#endif

namespace Jit64 {
// Returns the new RIP value
u8 *BackPatch(u8 *codePtr, int accessType, u32 emAddress, CONTEXT *ctx);

}  // namespace

#endif
