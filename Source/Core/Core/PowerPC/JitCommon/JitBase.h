// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCAnalyst.h"

//#define JIT_LOG_GENERATED_CODE  // Enables logging of generated code
//#define JIT_LOG_GPR             // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR             // Enables logging of the PPC floating point regs

// Use these to control the instruction selection
// #define INSTRUCTION_START FallBackToInterpreter(inst); return;
// #define INSTRUCTION_START PPCTables::CountInstruction(inst);
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

#define JITDISABLE(setting)                                                                        \
  FALLBACK_IF(SConfig::GetInstance().bJITOff || SConfig::GetInstance().setting)

class JitBase : public CPUCoreBase
{
public:
  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) = 0;
  virtual bool HandleStackFault() { return false; }
  virtual void ClearCache() = 0;
  virtual void ClearSafe() = 0;
  virtual void InvalidateICache(u32 address, u32 size, bool forced) = 0;
};

class JitBaseBlockCache;

class JitCommonBase : public JitBase
{
  friend void JitInterface::CompileExceptionCheck(JitInterface::ExceptionType type);
  friend void JitInterface::SetProfilingState(JitInterface::ProfilingState state);
  friend class JitBaseBlockCache;

protected:
  struct JitOptions
  {
    bool enableBlocklink;
    bool optimizeGatherPipe;
    bool accurateSinglePrecision;
    bool fastmem;
    bool memcheck;
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
    std::map<u8, u32> constantGqr;
    bool firstFPInstructionFound;
    bool isLastInstruction;
    int skipInstructions;
    bool carryFlagSet;
    bool carryFlagInverted;

    bool generatingTrampoline = false;
    u8* trampolineExceptionHandler;

    bool mustCheckFifo;
    int fifoBytesSinceCheck;

    PPCAnalyst::BlockStats st;
    PPCAnalyst::BlockRegStats gpa;
    PPCAnalyst::BlockRegStats fpa;
    PPCAnalyst::CodeOp* op;

    JitBlock* curBlock;

    std::unordered_set<u32> fifoWriteAddresses;
    std::unordered_set<u32> pairedQuantizeAddresses;
    std::unordered_set<u32> noSpeculativeConstantsAddresses;
  };

  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::CodeBuffer m_code_buffer;
  PPCAnalyst::PPCAnalyzer analyzer;
  JitOptions jo{};
  JitState js{};

  bool CanMergeNextInstructions(int count) const;

  void UpdateMemoryOptions();

public:
  JitCommonBase();
  ~JitCommonBase() override;

  virtual JitBaseBlockCache* GetBlockCache() = 0;

  virtual void Jit(u32 em_address) = 0;

  virtual const CommonAsmRoutinesBase* GetAsmRoutines() = 0;
  virtual void InvalidateICache(u32 address, u32 size, bool forced)
  {
    GetBlockCache()->InvalidateICache(address, size, forced);
  }
  void ClearSafe() { GetBlockCache()->Clear(); }

  static const u8* Dispatch(JitCommonBase& jit);

  static constexpr std::size_t code_buffer_size = 32000;
};

void JitTrampoline(JitCommonBase& jit, u32 em_address);
