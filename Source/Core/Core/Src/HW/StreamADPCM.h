// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

