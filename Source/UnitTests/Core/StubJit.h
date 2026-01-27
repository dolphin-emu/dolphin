// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

class StubJit : public JitBase
{
public:
  explicit StubJit(Core::System& system) : JitBase(system) {}

  // CPUCoreBase methods
  void Init() override {}
  void Shutdown() override {}
  void ClearCache() override {}
  void Run() override {}
  void SingleStep() override {}
  const char* GetName() const override { return nullptr; }
  // JitBase methods
  JitBaseBlockCache* GetBlockCache() override { return nullptr; }
  void Jit(u32) override {}
  void EraseSingleBlock(const JitBlock&) override {}
  std::vector<MemoryStats> GetMemoryStats() const override { return {}; }
  std::size_t DisassembleNearCode(const JitBlock&, std::ostream&) const override { return 0; }
  std::size_t DisassembleFarCode(const JitBlock&, std::ostream&) const override { return 0; }
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }
  bool HandleFault(uintptr_t, SContext*) override { return false; }
};

class StubBlockCache : public JitBaseBlockCache
{
public:
  explicit StubBlockCache(JitBase& jit) : JitBaseBlockCache(jit) {}

  void WriteLinkBlock(const JitBlock::LinkData&, const JitBlock*) override {}
};
