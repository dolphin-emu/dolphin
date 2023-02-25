// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"

class JitBase;

class BlockCache final : public JitBaseBlockCache
{
public:
  explicit BlockCache(JitBase& jit);

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
};
