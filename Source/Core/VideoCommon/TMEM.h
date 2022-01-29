// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "VideoCommon/BPMemory.h"

namespace TMEM
{
void InvalidateAll();
void Invalidate(u32 param);
void ConfigurationChanged(TexUnitAddress bp_addr, u32 config);
void Bind(u32 unit, int num_blocks_width, int num_blocks_height, bool is_mipmapped, bool is_32_bit);
void FinalizeBinds(BitSet32 used_textures);
bool IsCached(u32 unit);
bool IsValid(u32 unit);

void Init();
void DoState(PointerWrap& p);

}  // namespace TMEM
