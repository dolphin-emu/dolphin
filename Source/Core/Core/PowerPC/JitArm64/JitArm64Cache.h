// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Arm64Emitter.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class JitBase;

typedef void (*CompiledCode)();

class JitArm64BlockCache : public JitBaseBlockCache
{
public:
  explicit JitArm64BlockCache(JitBase& jit);

  void WriteLinkBlock(Arm64Gen::ARM64XEmitter& emit, const JitBlock::LinkData& source,
                      const JitBlock* dest = nullptr);

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;
};
