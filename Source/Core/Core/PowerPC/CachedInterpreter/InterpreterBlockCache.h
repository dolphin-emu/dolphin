// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"

class BlockCache final : public JitBaseBlockCache
{
public:
  BlockCache();

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
};
