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

static const int CODE_SIZE = 1024 * 1024 * 32;

// a bit of a hack; the MMU results in a vast amount more code ending up in the far cache,
// mostly exception handling, so give it a whole bunch more space if the MMU is on.
static const int FARCODE_SIZE = 1024 * 1024 * 8;
static const int FARCODE_SIZE_MMU = 1024 * 1024 * 48;

// same for the trampoline code cache, because fastmem results in far more backpatches in MMU mode
static const int TRAMPOLINE_CODE_SIZE = 1024 * 1024 * 8;
static const int TRAMPOLINE_CODE_SIZE_MMU = 1024 * 1024 * 32;

class JitBase;

extern JitBase* jit;

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
    bool alwaysUseMemFuncs;
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
    u8* exceptionHandler_ptr;
    int exceptionHandler_type;
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
    int fifoBytesThisBlock;

    PPCAnalyst::BlockStats st;
    PPCAnalyst::BlockRegStats gpa;
    PPCAnalyst::BlockRegStats fpa;
    PPCAnalyst::CodeOp* op;
    u8* rewriteStart;

    JitBlock* curBlock;

    std::unordered_set<u32> fifoWriteAddresses;
    std::unordered_set<u32> pairedQuantizeAddresses;
  };

  PPCAnalyst::CodeBlock code_block;
  PPCAnalyst::PPCAnalyzer analyzer;

  bool MergeAllowedNextInstructions(int count);

  void UpdateMemoryOptions();

public:
  // This should probably be removed from public:
  JitOptions jo;
  JitState js;

  static const u8* Dispatch() { return jit->GetBlockCache()->Dispatch(); };
  virtual JitBaseBlockCache* GetBlockCache() = 0;

  virtual void Jit(u32 em_address) = 0;

  virtual const CommonAsmRoutinesBase* GetAsmRoutines() = 0;

  virtual bool HandleFault(uintptr_t access_address, SContext* ctx) = 0;
  virtual bool HandleStackFault() { return false; }
};

void Jit(u32 em_address);
