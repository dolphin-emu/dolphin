// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"


namespace TmemBase
{
void Invalidate(u32 param);
void ConfigurationChanged(int bp_addr, int config);
void Bind(u8 unit, int num_blocks_width, int num_blocks_height, bool is_mipmapped, bool is_32_bit);
bool IsCached(u8 unit);
}
