// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/JitAsm.h"

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Gen;

void Jit64AsmRoutineManager::Init(u8* stack_top)
{
  m_const_pool.Init(AllocChildCodeSpace(4096), 4096);
  m_stack_top = stack_top;
  Generate();
  WriteProtect();
}

// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.

void Jit64AsmRoutineManager::Generate()
{
  enterCode = AlignCode16();
  // We need to own the beginning of RSP, so we do an extra stack adjustment
  // for the shadow region before calls in this function.  This call will
  // waste a bit of space for a second shadow, but whatever.
  ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, /*frame*/ 16);

  // Two statically allocated registers.
  // MOV(64, R(RMEM), Imm64((u64)Memory::physical_base));
  MOV(64, R(RPPCSTATE), Imm64((u64)&PowerPC::ppcState + 0x80));

  if (m_stack_top)
  {
    // Pivot the stack to our custom one.
    MOV(64, R(RSCRATCH), R(RSP));
    MOV(64, R(RSP), ImmPtr(m_stack_top - 0x20));
    MOV(64, MDisp(RSP, 0x18), R(RSCRATCH));
  }
  else
  {
    MOV(64, PPCSTATE(stored_stack_pointer), R(RSP));
  }
  // something that can't pass the BLR test
  MOV(64, MDisp(RSP, 8), Imm32((u32)-1));

  const u8* outerLoop = GetCodePtr();
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(CoreTiming::Advance);
  ABI_PopRegistersAndAdjustStack({}, 0);
  FixupBranch skipToRealDispatch =
      J(SConfig::GetInstance().bEnableDebugging);  // skip the sync and compare first time
  dispatcherMispredictedBLR = GetCodePtr();
  AND(32, PPCSTATE(pc), Imm32(0xFFFFFFFC));

#if 0  // debug mispredicts
  MOV(32, R(ABI_PARAM1), MDisp(RSP, 8)); // guessed_pc
  ABI_PushRegistersAndAdjustStack(1 << RSCRATCH2, 0);
  CALL(reinterpret_cast<void *>(&ReportMispredict));
  ABI_PopRegistersAndAdjustStack(1 << RSCRATCH2, 0);
#endif

  ResetStack(*this);

  SUB(32, PPCSTATE(downcount), R(RSCRATCH2));

  dispatcher = GetCodePtr();
  // Expected result of SUB(32, PPCSTATE(downcount), Imm32(block_cycles)) is in RFLAGS.
  // Branch if downcount is <= 0 (signed).
  FixupBranch bail = J_CC(CC_LE, true);

  FixupBranch dbg_exit;

  if (SConfig::GetInstance().bEnableDebugging)
  {
    MOV(64, R(RSCRATCH), ImmPtr(CPU::GetStatePtr()));
    TEST(32, MatR(RSCRATCH), Imm32(static_cast<u32>(CPU::State::Stepping)));
    FixupBranch notStepping = J_CC(CC_Z);
    ABI_PushRegistersAndAdjustStack({}, 0);
    ABI_CallFunction(PowerPC::CheckBreakPoints);
    ABI_PopRegistersAndAdjustStack({}, 0);
    MOV(64, R(RSCRATCH), ImmPtr(CPU::GetStatePtr()));
    TEST(32, MatR(RSCRATCH), Imm32(0xFFFFFFFF));
    dbg_exit = J_CC(CC_NZ, true);
    SetJumpTarget(notStepping);
  }

  SetJumpTarget(skipToRealDispatch);

  dispatcherNoCheck = GetCodePtr();

  // The following is a translation of JitBaseBlockCache::Dispatch into assembly.
  const bool assembly_dispatcher = true;
  if (assembly_dispatcher)
  {
    // Fast block number lookup.
    // ((PC >> 2) & mask) * sizeof(JitBlock*) = (PC & (mask << 2)) * 2
    MOV(32, R(RSCRATCH), PPCSTATE(pc));
    u64 icache = reinterpret_cast<u64>(g_jit->GetBlockCache()->GetFastBlockMap());
    AND(32, R(RSCRATCH), Imm32(JitBaseBlockCache::FAST_BLOCK_MAP_MASK << 2));
    if (icache <= INT_MAX)
    {
      MOV(64, R(RSCRATCH), MScaled(RSCRATCH, SCALE_2, static_cast<s32>(icache)));
    }
    else
    {
      MOV(64, R(RSCRATCH2), Imm64(icache));
      MOV(64, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_2, 0));
    }

    // Check if we found a block.
    TEST(64, R(RSCRATCH), R(RSCRATCH));
    FixupBranch not_found = J_CC(CC_Z);

    // Check both block.effectiveAddress and block.msrBits.
    MOV(32, R(RSCRATCH2), PPCSTATE(msr));
    AND(32, R(RSCRATCH2), Imm32(JitBaseBlockCache::JIT_CACHE_MSR_MASK));
    SHL(64, R(RSCRATCH2), Imm8(32));
    MOV(32, R(RSCRATCH_EXTRA), PPCSTATE(pc));
    OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));
    CMP(64, R(RSCRATCH2), MDisp(RSCRATCH, static_cast<s32>(offsetof(JitBlock, effectiveAddress))));
    FixupBranch state_mismatch = J_CC(CC_NE);

    // Success; branch to the block we found.
    // Switch to the correct memory base, in case MSR.DR has changed.
    TEST(32, PPCSTATE(msr), Imm32(1 << (31 - 27)));
    FixupBranch physmem = J_CC(CC_Z);
    MOV(64, R(RMEM), ImmPtr(Memory::logical_base));
    JMPptr(MDisp(RSCRATCH, static_cast<s32>(offsetof(JitBlock, normalEntry))));
    SetJumpTarget(physmem);
    MOV(64, R(RMEM), ImmPtr(Memory::physical_base));
    JMPptr(MDisp(RSCRATCH, static_cast<s32>(offsetof(JitBlock, normalEntry))));

    SetJumpTarget(not_found);
    SetJumpTarget(state_mismatch);

    // Failure, fallback to the C++ dispatcher for calling the JIT.
  }

  // Ok, no block, let's call the slow dispatcher
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(JitBase::Dispatch);
  ABI_PopRegistersAndAdjustStack({}, 0);

  TEST(64, R(ABI_RETURN), R(ABI_RETURN));
  FixupBranch no_block_available = J_CC(CC_Z);

  // Switch to the correct memory base, in case MSR.DR has changed.
  TEST(32, PPCSTATE(msr), Imm32(1 << (31 - 27)));
  FixupBranch physmem = J_CC(CC_Z);
  MOV(64, R(RMEM), ImmPtr(Memory::logical_base));
  JMPptr(R(ABI_RETURN));
  SetJumpTarget(physmem);
  MOV(64, R(RMEM), ImmPtr(Memory::physical_base));
  JMPptr(R(ABI_RETURN));

  SetJumpTarget(no_block_available);

  // We reset the stack because Jit might clear the code cache.
  // Also if we are in the middle of disabling BLR optimization on windows
  // we need to reset the stack before _resetstkoflw() is called in Jit
  // otherwise we will generate a second stack overflow exception during DoJit()
  ResetStack(*this);

  ABI_PushRegistersAndAdjustStack({}, 0);
  MOV(32, R(ABI_PARAM1), PPCSTATE(pc));
  ABI_CallFunction(JitTrampoline);
  ABI_PopRegistersAndAdjustStack({}, 0);

  JMP(dispatcherNoCheck, true);

  SetJumpTarget(bail);
  doTiming = GetCodePtr();

  // make sure npc contains the next pc (needed for exception checking in CoreTiming::Advance)
  MOV(32, R(RSCRATCH), PPCSTATE(pc));
  MOV(32, PPCSTATE(npc), R(RSCRATCH));

  // Check the state pointer to see if we are exiting
  // Gets checked on at the end of every slice
  MOV(64, R(RSCRATCH), ImmPtr(CPU::GetStatePtr()));
  TEST(32, MatR(RSCRATCH), Imm32(0xFFFFFFFF));
  J_CC(CC_Z, outerLoop);

  // Landing pad for drec space
  if (SConfig::GetInstance().bEnableDebugging)
    SetJumpTarget(dbg_exit);
  ResetStack(*this);
  if (m_stack_top)
  {
    ADD(64, R(RSP), Imm8(0x18));
    POP(RSP);
  }

  ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
  RET();

  JitRegister::Register(enterCode, GetCodePtr(), "JIT_Loop");

  GenerateCommon();
}

void Jit64AsmRoutineManager::ResetStack(X64CodeBlock& emitter)
{
  if (m_stack_top)
    emitter.MOV(64, R(RSP), Imm64((u64)m_stack_top - 0x20));
  else
    emitter.MOV(64, R(RSP), PPCSTATE(stored_stack_pointer));
}

void Jit64AsmRoutineManager::GenerateCommon()
{
  fifoDirectWrite8 = AlignCode4();
  GenFifoWrite(8);
  fifoDirectWrite16 = AlignCode4();
  GenFifoWrite(16);
  fifoDirectWrite32 = AlignCode4();
  GenFifoWrite(32);
  fifoDirectWrite64 = AlignCode4();
  GenFifoWrite(64);
  frsqrte = AlignCode4();
  GenFrsqrte();
  fres = AlignCode4();
  GenFres();
  mfcr = AlignCode4();
  GenMfcr();

  GenQuantizedLoads();
  GenQuantizedSingleLoads();
  GenQuantizedStores();
  GenQuantizedSingleStores();

  // CMPSD(R(XMM0), M(&zero),
  // TODO

  // Fast write routines - special case the most common hardware write
  // TODO: use this.
  // Even in x86, the param values will be in the right registers.
  /*
  const u8 *fastMemWrite8 = AlignCode16();
  CMP(32, R(ABI_PARAM2), Imm32(0xCC008000));
  FixupBranch skip_fast_write = J_CC(CC_NE, false);
  MOV(32, RSCRATCH, M(&m_gatherPipeCount));
  MOV(8, MDisp(RSCRATCH, (u32)&m_gatherPipe), ABI_PARAM1);
  ADD(32, 1, M(&m_gatherPipeCount));
  RET();
  SetJumpTarget(skip_fast_write);
  CALL((void *)&PowerPC::Write_U8);*/
}
