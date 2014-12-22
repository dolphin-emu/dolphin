// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#include "Common/CommonTypes.h"

struct BackPatchInfo
{
	enum
	{
		FLAG_STORE    = (1 << 0),
		FLAG_LOAD     = (1 << 1),
		FLAG_SIZE_8   = (1 << 2),
		FLAG_SIZE_16  = (1 << 3),
		FLAG_SIZE_32  = (1 << 4),
		FLAG_SIZE_F32 = (1 << 5),
		FLAG_SIZE_F64 = (1 << 6),
		FLAG_REVERSE  = (1 << 7),
		FLAG_EXTEND   = (1 << 8),
	};

	u32 m_fastmem_size;
	u32 m_fastmem_trouble_inst_offset;
	u32 m_slowmem_size;
};
