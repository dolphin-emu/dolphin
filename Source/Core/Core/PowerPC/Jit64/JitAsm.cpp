// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/JitAsm.h"

#include <climits>

#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "Core/Config/MainSettings.h"
#include "Core/CoreTiming.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

using namespace Gen;

Jit64AsmRoutineManager::Jit64AsmRoutineManager(Jit64& jit) : CommonAsmRoutines(jit)
{
}

void Jit64AsmRoutineManager::Init()
{
  m_const_pool.Init(AllocChildCodeSpace(4096), 4096);
  Generate();
  WriteProtect(true);
}

void Jit64AsmRoutineManager::Regenerate()
{
  UnWriteProtect(false);
  ResetCodePtr();
  Generate();
  WriteProtect(true);
}

// PLAN: no more block numbers - crazy opcodes just contain offset within
// dynarec buffer
// At this offset - 4, there is an int specifying the block number.

void Jit64AsmRoutineManager::Generate()
{
  const bool enable_debugging = Config::IsDebuggingEnabled();

  enter_code = AlignCode16();
  // We need to own the beginning of RSP, so we do an extra stack adjustment
  // for the shadow region before calls in this function.  This call will
  // waste a bit of space for a second shadow, but whatever.
  ABI_PushRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, /*frame*/ 16);

  auto& ppc_state = m_jit.m_ppc_state;

  // Two statically allocated registers.
  // MOV(64, R(RMEM), Imm64((u64)Memory::physical_base));
  MOV(64, R(RPPCSTATE), Imm64((u64)&ppc_state + 0x80));

  MOV(64, PPCSTATE(stored_stack_pointer), R(RSP));

  // something that can't pass the BLR test
  MOV(64, MDisp(RSP, 8), Imm32((u32)-1));

  const u8* outerLoop = GetCodePtr();
  ABI_PushRegistersAndAdjustStack({}, 0);
  ABI_CallFunction(CoreTiming::GlobalAdvance);
  ABI_PopRegistersAndAdjustStack({}, 0);

  // When we've just entered the jit we need to update the membase
  // GlobalAdvance also checks exceptions after which we need to
  // update the membase so it makes sense to do this here.
  MOV(64, R(RMEM), PPCSTATE(mem_ptr));

  // skip the sync and compare first time
  FixupBranch skipToRealDispatch = J(enable_debugging ? Jump::Near : Jump::Short);

  dispatcher_mispredicted_blr = GetCodePtr();
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
  FixupBranch bail = J_CC(CC_LE, Jump::Near);

  dispatcher_no_timing_check = GetCodePtr();

  auto& system = m_jit.m_system;

  FixupBranch dbg_exit;
  if (enable_debugging)
  {
    MOV(64, R(RSCRATCH), ImmPtr(system.GetCPU().GetStatePtr()));
    CMP(32, MatR(RSCRATCH), Imm32(Common::ToUnderlying(CPU::State::Running)));
    dbg_exit = J_CC(CC_NE, Jump::Near);
  }

  SetJumpTarget(skipToRealDispatch);

  dispatcher_no_check = GetCodePtr();

  // The following is a translation of JitBaseBlockCache::Dispatch into assembly.
  const bool assembly_dispatcher = true;
  if (assembly_dispatcher)
  {
    if (m_jit.GetBlockCache()->GetEntryPoints())
    {
      MOV(32, R(RSCRATCH2), PPCSTATE(feature_flags));
      SHL(64, R(RSCRATCH2), Imm8(32));

      MOV(32, R(RSCRATCH_EXTRA), PPCSTATE(pc));
      OR(64, R(RSCRATCH_EXTRA), R(RSCRATCH2));

      u64 icache = reinterpret_cast<u64>(m_jit.GetBlockCache()->GetEntryPoints());
      MOV(64, R(RSCRATCH2), Imm64(icache));
      // The entry points map is indexed by ((feature_flags << 30) | (pc >> 2)).
      // The map contains 8-byte pointers and that means we need to shift feature_flags
      // left by 33 bits and pc left by 1 bit to get the correct offset in the map.
      MOV(64, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH_EXTRA, SCALE_2, 0));
    }
    else
    {
      // Fast block number lookup.
      // ((PC >> 2) & mask) * sizeof(JitBlock*) = (PC & (mask << 2)) * 2
      MOV(32, R(RSCRATCH), PPCSTATE(pc));
      // Keep a copy for later.
      MOV(32, R(RSCRATCH_EXTRA), R(RSCRATCH));
      u64 icache = reinterpret_cast<u64>(m_jit.GetBlockCache()->GetFastBlockMapFallback());
      AND(32, R(RSCRATCH), Imm32(JitBaseBlockCache::FAST_BLOCK_MAP_FALLBACK_MASK << 2));
      if (icache <= INT_MAX)
      {
        MOV(64, R(RSCRATCH), MScaled(RSCRATCH, SCALE_2, static_cast<s32>(icache)));
      }
      else
      {
        MOV(64, R(RSCRATCH2), Imm64(icache));
        MOV(64, R(RSCRATCH), MComplex(RSCRATCH2, RSCRATCH, SCALE_2, 0));
      }
    }

    // Check if we found a block.
    TEST(64, R(RSCRATCH), R(RSCRATCH));
    FixupBranch not_found = J_CC(CC_Z);
    FixupBranch state_mismatch;

    if (!m_jit.GetBlockCache()->GetEntryPoints())
    {
      // Check block.feature_flags.
      MOV(32, R(RSCRATCH2), PPCSTATE(feature_flags));
      // Also check the block.effectiveAddress. RSCRATCH_EXTRA still has the PC.
      SHL(64, R(RSCRATCH_EXTRA), Imm8(32));
      OR(64, R(RSCRATCH2), R(RSCRATCH_EXTRA));

      static_assert(offsetof(JitBlockData, feature_flags) + 4 ==
                    offsetof(JitBlockData, effectiveAddress));

      CMP(64, R(RSCRATCH2),
          MDisp(RSCRATCH, static_cast<s32>(offsetof(JitBlockData, feature_flags))));

      state_mismatch = J_CC(CC_NE);
      // Success; branch to the block we found.
      JMPptr(MDisp(RSCRATCH, static_cast<s32>(offsetof(JitBlockData, normalEntry))));
    }
    else
    {
      // Success; branch to the block we found.
      JMPptr(R(RSCRATCH));
    }

    SetJumpTarget(not_found);
    if (!m_jit.GetBlockCache()->GetEntryPoints())
    {
      SetJumpTarget(state_mismatch);
    }

    // Failure, fallback to the C++ dispatcher for calling the JIT.
  }

  // There is no point in calling the dispatcher in the fast lookup table
  // case, since the assembly dispatcher would already have found a block.
  if (!assembly_dispatcher || !m_jit.GetBlockCache()->GetEntryPoints())
  {
    // Ok, no block, let's call the slow dispatcher
    ABI_PushRegistersAndAdjustStack({}, 0);
    MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(&m_jit)));
    ABI_CallFunction(JitBase::Dispatch);
    ABI_PopRegistersAndAdjustStack({}, 0);

    TEST(64, R(ABI_RETURN), R(ABI_RETURN));
    FixupBranch no_block_available = J_CC(CC_Z);

    // Jump to the block
    JMPptr(R(ABI_RETURN));

    SetJumpTarget(no_block_available);
  }

  // We reset the stack because Jit might clear the code cache.
  // Also if we are in the middle of disabling BLR optimization on windows
  // we need to reset the stack before _resetstkoflw() is called in Jit
  // otherwise we will generate a second stack overflow exception during DoJit()
  ResetStack(*this);

  ABI_PushRegistersAndAdjustStack({}, 0);
  MOV(64, R(ABI_PARAM1), Imm64(reinterpret_cast<u64>(&m_jit)));
  MOV(32, R(ABI_PARAM2), PPCSTATE(pc));
  ABI_CallFunction(JitTrampoline);
  ABI_PopRegistersAndAdjustStack({}, 0);

  // If jitting triggered an ISI exception, MSR.DR may have changed
  MOV(64, R(RMEM), PPCSTATE(mem_ptr));

  JMP(dispatcher_no_check, Jump::Near);

  SetJumpTarget(bail);
  do_timing = GetCodePtr();

  // make sure npc contains the next pc (needed for exception checking in CoreTiming::Advance)
  MOV(32, R(RSCRATCH), PPCSTATE(pc));
  MOV(32, PPCSTATE(npc), R(RSCRATCH));

  // Check the state pointer to see if we are exiting
  // Gets checked on at the end of every slice
  MOV(64, R(RSCRATCH), ImmPtr(system.GetCPU().GetStatePtr()));
  CMP(32, MatR(RSCRATCH), Imm32(Common::ToUnderlying(CPU::State::Running)));
  J_CC(CC_E, outerLoop);

  // Landing pad for drec space
  dispatcher_exit = GetCodePtr();
  if (enable_debugging)
    SetJumpTarget(dbg_exit);

  // Reset the stack pointer, since the BLR optimization may have pushed things onto the stack
  // without popping them.
  ResetStack(*this);

  ABI_PopRegistersAndAdjustStack(ABI_ALL_CALLEE_SAVED, 8, 16);
  RET();

  Common::JitRegister::Register(enter_code, GetCodePtr(), "JIT_Loop");

  GenerateCommon();
}

void Jit64AsmRoutineManager::ResetStack(X64CodeBlock& emitter)
{
  emitter.MOV(64, R(RSP), PPCSTATE(stored_stack_pointer));
}

void Jit64AsmRoutineManager::GenerateCommon()
{
  frsqrte = AlignCode4();
  GenFrsqrte();
  fres = AlignCode4();
  GenFres();
  mfcr = AlignCode4();
  GenMfcr();
  cdts = AlignCode4();
  GenConvertDoubleToSingle();

  GenQuantizedLoads();
  GenQuantizedSingleLoads();
  GenQuantizedStores();
  GenQuantizedSingleStores();
}
