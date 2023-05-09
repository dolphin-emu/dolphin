// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <map>
#include <unordered_set>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/PPCAnalyst.h"

namespace Core
{
class System;
}
namespace PowerPC
{
class MMU;
struct PowerPCState;
}  // namespace PowerPC

//#define JIT_LOG_GENERATED_CODE  // Enables logging of generated code
//#define JIT_LOG_GPR             // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR             // Enables logging of the PPC floating point regs

// Use these to control the instruction selection
// #define INSTRUCTION_START FallBackToInterpreter(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst, m_ppc_state.pc);
#define INSTRUCTION_START

#define FALLBACK_IF(cond)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if (cond)                                                                                      \
    {                                                                                              \
      FallBackToInterpreter(inst);                                                                 \
      return;                                                                                      \
    }                                                                                              \
  } while (0)

#define JITDISABLE(setting) FALLBACK_IF(bJITOff || setting)

class JitBase : public CPUCoreBase
{
protected:
  enum class CarryFlag
  {
    InPPCState,
    InHostCarry,
#ifdef _M_X86_64
    InHostCarryInverted,
#endif
#ifdef _M_ARM_64
    ConstantTrue,
    ConstantFalse,
#endif
  };

  static constexpr size_t SAFE_STACK_SIZE = 256 * 1024;
  static constexpr size_t MIN_UNSAFE_STACK_SIZE = 192 * 1024;
  static constexpr size_t MIN_STACK_SIZE = SAFE_STACK_SIZE + MIN_UNSAFE_STACK_SIZE;
  static constexpr size_t GUARD_SIZE = 64 * 1024;
  static constexpr size_t GUARD_OFFSET = SAFE_STACK_SIZE - GUARD_SIZE;

  struct JitOptions
  {
    bool enableBlocklink;
    bool optimizeGatherPipe;
    bool accurateSinglePrecision;
    bool fastmem;
    bool fastmem_arena;
    bool memcheck;
    bool fp_exceptions;
    bool div_by_zero_exceptions;
    bool profile_blocks;
  };
  struct JitState
  {
    u32 compilerPC;
    u32 blockStart;
    int instructionNumber;
    int instructionsLeft;
    int downcountAmount;
    u32 numLoadStoreInst;
    u32 numFloatingPointInst;
    // If this is set, we need to generate an exception handler for the fastmem load.
    u8* fastmemLoadStore;
    // If this is set, a load or store already prepared a jump to the exception handler for us,
    // so just fixup that branch instead of testing for a DSI again.
    bool fixupExceptionHandler;
    Gen::FixupBranch exceptionHandler;

    bool assumeNoPairedQuantize;
    BitSet8 constantGqrValid;
    std::array<u32, 8> constantGqr;
    bool firstFPInstructionFound;
    bool isLastInstruction;
    int skipInstructions;
    CarryFlag carryFlag;

    bool generatingTrampoline = false;
    u8* trampolineExceptionHandler;

    bool mustCheckFifo;
    u32 fifoBytesSinceCheck;

    PPCAnalyst::BlockStats st;
    PPCAnalyst::BlockRegStats gpa;
    PPCAnalyst::BlockRegStats fpa;
    PPCAnalyst::CodeOp* op;
    BitSet32 fpr_is_store_safe;

    JitBlock* curBlock;

    std::unordered_set<u32> fifoWriteAddresses;
    std::unordered_set<u32> pairedQuantizeAddresses;
    std::unordered_set<u32> noSpeculativeConstantsAddresses;
  };

  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::CodeBuffer m_code_buffer;
  PPCAnalyst::PPCAnalyzer analyzer;

  size_t m_registered_config_callback_id;
  bool bJITOff = false;
  bool bJITLoadStoreOff = false;
  bool bJITLoadStorelXzOff = false;
  bool bJITLoadStorelwzOff = false;
  bool bJITLoadStorelbzxOff = false;
  bool bJITLoadStoreFloatingOff = false;
  bool bJITLoadStorePairedOff = false;
  bool bJITFloatingPointOff = false;
  bool bJITIntegerOff = false;
  bool bJITPairedOff = false;
  bool bJITSystemRegistersOff = false;
  bool bJITBranchOff = false;
  bool bJITRegisterCacheOff = false;
  bool m_enable_debugging = false;
  bool m_enable_float_exceptions = false;
  bool m_enable_div_by_zero_exceptions = false;
  bool m_low_dcbz_hack = false;
  bool m_fprf = false;
  bool m_accurate_nans = false;
  bool m_fastmem_enabled = false;
  bool m_mmu_enabled = false;
  bool m_pause_on_panic_enabled = false;
  bool m_accurate_cpu_cache_enabled = false;

  bool m_enable_blr_optimization = false;
  bool m_cleanup_after_stackfault = false;
  u8* m_stack_guard = nullptr;

  void RefreshConfig();

  void InitBLROptimization();
  void ProtectStack();
  void UnprotectStack();
  void CleanUpAfterStackFault();

  bool CanMergeNextInstructions(int count) const;

  void UpdateMemoryAndExceptionOptions();

  bool ShouldHandleFPExceptionForInstruction(const PPCAnalyst::CodeOp* op);

public:
  explicit JitBase(Core::System& system);
  JitBase(const JitBase&) = delete;
  JitBase(JitBase&&) = delete;
  JitBase& operator=(const JitBase&) = delete;
  JitBase& operator=(JitBase&&) = delete;
  ~JitBase() override;

  bool IsDebuggingEnabled() const { return m_enable_debugging; }

  static const u8* Dispatch(JitBase& jit);
  virtual JitBaseBlockCache* GetBlockCache() = 0;

  virtual void Jit(u32 em_address) = 0;

  virtual const CommonAsmRoutinesBase* GetAsmRoutines() = 0;

  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) = 0;
  bool HandleStackFault();

  static constexpr std::size_t code_buffer_size = 32000;

  // This should probably be removed from public:
  JitOptions jo{};
  JitState js{};

  Core::System& m_system;
  PowerPC::PowerPCState& m_ppc_state;
  PowerPC::MMU& m_mmu;
};

void JitTrampoline(JitBase& jit, u32 em_address);
