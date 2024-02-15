// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/CachedInterpreter/InterpreterBlockCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PPCAnalyst.h"

class CachedInterpreter : public JitBase
{
public:
  explicit CachedInterpreter(Core::System& system);
  CachedInterpreter(const CachedInterpreter&) = delete;
  CachedInterpreter(CachedInterpreter&&) = delete;
  CachedInterpreter& operator=(const CachedInterpreter&) = delete;
  CachedInterpreter& operator=(CachedInterpreter&&) = delete;
  ~CachedInterpreter();

  void Init() override;
  void Shutdown() override;

  bool HandleFault(uintptr_t access_address, SContext* ctx) override { return false; }
  void ClearCache() override;

  void Run() override;
  void SingleStep() override;

  void Jit(u32 address) override;

  JitBaseBlockCache* GetBlockCache() override { return &m_block_cache; }
  const char* GetName() const override { return "Cached Interpreter"; }
  const CommonAsmRoutinesBase* GetAsmRoutines() override { return nullptr; }

private:
  struct Instruction;

  u8* GetCodePtr();
  void ExecuteOneBlock();

  bool HandleFunctionHooking(u32 address);

  static void EndBlock(CachedInterpreter& cached_interpreter, UGeckoInstruction data);
  static void UpdateNumLoadStoreInstructions(CachedInterpreter& cached_interpreter,
                                             UGeckoInstruction data);
  static void UpdateNumFloatingPointInstructions(CachedInterpreter& cached_interpreter,
                                                 UGeckoInstruction data);
  static void WritePC(CachedInterpreter& cached_interpreter, UGeckoInstruction data);
  static void WriteBrokenBlockNPC(CachedInterpreter& cached_interpreter, UGeckoInstruction data);
  static bool CheckFPU(CachedInterpreter& cached_interpreter, u32 data);
  static bool CheckDSI(CachedInterpreter& cached_interpreter, u32 data);
  static bool CheckProgramException(CachedInterpreter& cached_interpreter, u32 data);
  static bool CheckBreakpoint(CachedInterpreter& cached_interpreter, u32 data);
  static bool CheckIdle(CachedInterpreter& cached_interpreter, u32 idle_pc);

  BlockCache m_block_cache{*this};
  std::vector<Instruction> m_code;
};
