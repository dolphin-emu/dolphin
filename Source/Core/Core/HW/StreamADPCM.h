// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Adapted from in_cube by hcs & destop

#pragma once

#include "Common/CommonTypes.h"

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
