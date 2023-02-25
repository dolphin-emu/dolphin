// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/Arm64Emitter.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class JitBase;

typedef void (*CompiledCode)();

class JitArm64BlockCache : public JitBaseBlockCache
{
public:
  explicit JitArm64BlockCache(JitBase& jit);

  void Init() override;

  void DestroyBlock(JitBlock& block) override;

  const std::vector<std::pair<u8*, u8*>>& GetRangesToFreeNear() const;
  const std::vector<std::pair<u8*, u8*>>& GetRangesToFreeFar() const;

  void ClearRangesToFree();

  void WriteLinkBlock(Arm64Gen::ARM64XEmitter& emit, const JitBlock::LinkData& source,
                      const JitBlock* dest = nullptr);

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;

  std::vector<std::pair<u8*, u8*>> m_ranges_to_free_on_next_codegen_near;
  std::vector<std::pair<u8*, u8*>> m_ranges_to_free_on_next_codegen_far;
};
