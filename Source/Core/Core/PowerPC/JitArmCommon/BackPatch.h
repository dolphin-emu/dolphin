// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include "Common/CommonTypes.h"

struct BackPatchInfo
{
	enum
	{
		FLAG_STORE       = (1 << 0),
		FLAG_LOAD        = (1 << 1),
		FLAG_SIZE_8      = (1 << 2),
		FLAG_SIZE_16     = (1 << 3),
		FLAG_SIZE_32     = (1 << 4),
		FLAG_SIZE_F32    = (1 << 5),
		FLAG_SIZE_F32X2  = (1 << 6),
		FLAG_SIZE_F32X2I = (1 << 7),
		FLAG_SIZE_F64    = (1 << 8),
		FLAG_REVERSE     = (1 << 9),
		FLAG_EXTEND      = (1 << 10),
		FLAG_SIZE_F32I   = (1 << 11),
		FLAG_ZERO_256    = (1 << 12),
		FLAG_MASK_FLOAT  = FLAG_SIZE_F32 |
		                   FLAG_SIZE_F32X2 |
		                   FLAG_SIZE_F32X2I |
		                   FLAG_SIZE_F64 |
		                   FLAG_SIZE_F32I,
	};

	static u32 GetFlagSize(u32 flags)
	{
		if (flags & FLAG_SIZE_8)
			return 8;
		if (flags & FLAG_SIZE_16)
			return 16;
		if (flags & FLAG_SIZE_32)
			return 32;
		if (flags & FLAG_SIZE_F32 || flags & FLAG_SIZE_F32I)
			return 32;
		if (flags & FLAG_SIZE_F64)
			return 64;
		if (flags & FLAG_ZERO_256)
			return 256;
		return 0;
	}
};
