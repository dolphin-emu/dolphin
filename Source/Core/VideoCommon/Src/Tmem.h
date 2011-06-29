// Copyright (C) 2003 Dolphin Project.

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

#ifndef _TMEM_H
#define _TMEM_H

#include "VideoCommon.h"

enum 
{
    TMEM_SIZE = 1024*1024,
	TMEM_HALF = TMEM_SIZE/2
};

// This does not hold the real cache. It only holds TLUTs and preloaded
// textures. Cached areas are handled externally.
extern u8 g_texMem[];

void LoadTmem(u32 tmemAddr, const u8* src, u32 size);

#endif
