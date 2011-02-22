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

// Adapted from in_cube by hcs & destop

#ifndef _STREAMADPCM_H
#define _STREAMADPCM_H

#include "Common.h"

class NGCADPCM
{
public:
	enum
	{
		ONE_BLOCK_SIZE = 32,
		SAMPLES_PER_BLOCK = 28
	};

	static void InitFilter();
	static void DecodeBlock(s16 *pcm, const u8 *adpcm);
};

#endif

