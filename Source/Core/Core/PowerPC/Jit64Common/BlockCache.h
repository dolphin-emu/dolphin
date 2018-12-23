// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/JitCache.h"

class Jit64;

class JitBlockCache : public JitBaseBlockCache
{
public:
  explicit JitBlockCache(Jit64& jit);

private:
  Jit64& m_jit;
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;
};
