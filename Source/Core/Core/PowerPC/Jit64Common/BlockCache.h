// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Core/PowerPC/JitCommon/JitCache.h"

class JitBase;

class JitBlockCache : public JitBaseBlockCache
{
public:
  explicit JitBlockCache(JitBase& jit);

  void Init() override;

  void DestroyBlock(JitBlock& block) override;

  const std::vector<std::pair<u8*, u8*>>& GetRangesToFreeNear() const;
  const std::vector<std::pair<u8*, u8*>>& GetRangesToFreeFar() const;

  void ClearRangesToFree();

private:
  void WriteLinkBlock(const JitBlock::LinkData& source, const JitBlock* dest) override;
  void WriteDestroyBlock(const JitBlock& block) override;

  std::vector<std::pair<u8*, u8*>> m_ranges_to_free_on_next_codegen_near;
  std::vector<std::pair<u8*, u8*>> m_ranges_to_free_on_next_codegen_far;
};
