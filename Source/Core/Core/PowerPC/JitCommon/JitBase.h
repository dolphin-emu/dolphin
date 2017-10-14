// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

//#define JIT_LOG_X86     // Enables logging of the generated x86 code
//#define JIT_LOG_GPR     // Enables logging of the PPC general purpose regs
//#define JIT_LOG_FPR     // Enables logging of the PPC floating point regs

#include <map>
#include <unordered_set>

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/CPUCoreBase.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"
#include "Core/PowerPC/JitCommon/JitCache.h"
#include "Core/PowerPC/PPCAnalyst.h"

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

class JitBase;

extern JitBase* g_jit;

class JitBase : public CPUCoreBase
{
protected:
  struct JitOptions
  {
    bool enableBlocklink;
    bool optimizeGatherPipe;
    bool accurateSinglePrecision;
    bool fastmem;
    bool memcheck;
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
    // If these are set, we've stored the old value of a register which will be loaded in
    // revertLoad,
    // which lets us revert it on the exception path.
    int revertGprLoad;
    int revertFprLoad;

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
    u8* rewriteStart;

    JitBlock* curBlock;

    std::unordered_set<u32> fifoWriteAddresses;
    std::unordered_set<u32> pairedQuantizeAddresses;
    std::unordered_set<u32> noSpeculativeConstantsAddresses;
  };

  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::PPCAnalyzer analyzer;

  bool CanMergeNextInstructions(int count) const;

  void UpdateMemoryOptions();

public:
  // This should probably be removed from public:
  JitOptions jo;
  JitState js;

  JitBase();
  ~JitBase() override;

  static const u8* Dispatch() { return g_jit->GetBlockCache()->Dispatch(); }
  virtual JitBaseBlockCache* GetBlockCache() = 0;

  virtual void Jit(u32 em_address) = 0;

  virtual const CommonAsmRoutinesBase* GetAsmRoutines() = 0;

  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) = 0;
  virtual bool HandleStackFault() { return false; }
};

void JitTrampoline(u32 em_address);

// Merged routines that should be moved somewhere better
u32 Helper_Mask(u8 mb, u8 me);
