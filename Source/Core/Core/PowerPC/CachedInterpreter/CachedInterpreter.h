// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CachedInterpreter/InterpreterBlockCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PPCAnalyst.h"

class CachedInterpreter : public JitBase
{
public:
  CachedInterpreter();
  ~CachedInterpreter();

  void Init() override;
  void Shutdown() override;

  bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }
  void ClearCache() override;

  void Run() override;
  void SingleStep() override;

  void Jit(u32 address) override;

  JitBaseBlockCache* GetBlockCache() override { return &m_block_cache; }
  const char* GetName() override { return "Cached Interpreter"; }
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }
private:
  struct Instruction;

  const u8* GetCodePtr() const;
  void ExecuteOneBlock();

  BlockCache m_block_cache{*this};
  std::vector<Instruction> m_code;
  PPCAnalyst::CodeBuffer code_buffer;
};
