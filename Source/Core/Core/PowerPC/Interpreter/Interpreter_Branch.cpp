// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/BranchWatch.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

void Interpreter::bx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;

  if (inst.LK)
    LR(ppc_state) = ppc_state.pc + 4;

  u32 destination_addr = u32(SignExt26(inst.LI << 2));
  if (!inst.AA)
    destination_addr += ppc_state.pc;
  ppc_state.npc = destination_addr;

  if (auto& branch_watch = interpreter.m_branch_watch; branch_watch.GetRecordingActive())
    branch_watch.HitTrue(ppc_state.pc, destination_addr, inst, ppc_state.msr.IR);

  interpreter.m_end_block = true;
}

// bcx - ugly, straight from PPC manual equations :)
void Interpreter::bcx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  auto& branch_watch = interpreter.m_branch_watch;

  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    CTR(ppc_state)--;

  const bool true_false = ((inst.BO >> 3) & 1) != 0;
  const bool only_counter_check = ((inst.BO >> 4) & 1) != 0;
  const bool only_condition_check = ((inst.BO >> 2) & 1) != 0;
  const u32 ctr_check = ((CTR(ppc_state) != 0) ^ (inst.BO >> 1)) & 1;
  const bool counter = only_condition_check || ctr_check != 0;
  const bool condition = only_counter_check || (ppc_state.cr.GetBit(inst.BI) == u32(true_false));

  if (counter && condition)
  {
    if (inst.LK)
      LR(ppc_state) = ppc_state.pc + 4;

    u32 destination_addr = u32(SignExt16(s16(inst.BD << 2)));
    if (!inst.AA)
      destination_addr += ppc_state.pc;
    ppc_state.npc = destination_addr;

    if (branch_watch.GetRecordingActive())
      branch_watch.HitTrue(ppc_state.pc, destination_addr, inst, ppc_state.msr.IR);
  }
  else if (branch_watch.GetRecordingActive())
  {
    branch_watch.HitFalse(ppc_state.pc, ppc_state.pc + 4, inst, ppc_state.msr.IR);
  }

  interpreter.m_end_block = true;
}

void Interpreter::bcctrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  auto& branch_watch = interpreter.m_branch_watch;

  DEBUG_ASSERT_MSG(POWERPC, (inst.BO_2 & BO_DONT_DECREMENT_FLAG) != 0,
                   "bcctrx with decrement and test CTR option is invalid!");

  const u32 condition =
      ((inst.BO_2 >> 4) | (ppc_state.cr.GetBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

  if (condition != 0)
  {
    const u32 destination_addr = CTR(ppc_state) & (~3);
    ppc_state.npc = destination_addr;
    if (inst.LK_3)
      LR(ppc_state) = ppc_state.pc + 4;

    if (branch_watch.GetRecordingActive())
      branch_watch.HitTrue(ppc_state.pc, destination_addr, inst, ppc_state.msr.IR);
  }
  else if (branch_watch.GetRecordingActive())
  {
    branch_watch.HitFalse(ppc_state.pc, ppc_state.pc + 4, inst, ppc_state.msr.IR);
  }

  interpreter.m_end_block = true;
}

void Interpreter::bclrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  auto& branch_watch = interpreter.m_branch_watch;

  if ((inst.BO_2 & BO_DONT_DECREMENT_FLAG) == 0)
    CTR(ppc_state)--;

  const u32 counter = ((inst.BO_2 >> 2) | ((CTR(ppc_state) != 0) ^ (inst.BO_2 >> 1))) & 1;
  const u32 condition =
      ((inst.BO_2 >> 4) | (ppc_state.cr.GetBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

  if ((counter & condition) != 0)
  {
    const u32 destination_addr = LR(ppc_state) & (~3);
    ppc_state.npc = destination_addr;
    if (inst.LK_3)
      LR(ppc_state) = ppc_state.pc + 4;

    if (branch_watch.GetRecordingActive())
      branch_watch.HitTrue(ppc_state.pc, destination_addr, inst, ppc_state.msr.IR);
  }
  else if (branch_watch.GetRecordingActive())
  {
    branch_watch.HitFalse(ppc_state.pc, ppc_state.pc + 4, inst, ppc_state.msr.IR);
  }

  interpreter.m_end_block = true;
}

void Interpreter::HLEFunction(Interpreter& interpreter, UGeckoInstruction inst)
{
  interpreter.m_end_block = true;

  ASSERT(Core::IsCPUThread());
  Core::CPUThreadGuard guard(interpreter.m_system);

  HLE::Execute(guard, interpreter.m_ppc_state.pc, inst.hex);
}

void Interpreter::rfi(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;

  if (ppc_state.ibat_update_pending)
  {
    interpreter.m_mmu.IBATUpdated();
    ppc_state.ibat_update_pending = false;
  }

  if (ppc_state.dbat_update_pending)
  {
    interpreter.m_mmu.DBATUpdated();
    ppc_state.dbat_update_pending = false;
  }

  if (ppc_state.msr.PR)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::PrivilegedInstruction);
    return;
  }

  // Restore saved bits from SRR1 to MSR.
  // Gecko/Broadway can save more bits than explicitly defined in ppc spec
  const u32 mask = 0x87C0FFFF;
  ppc_state.msr.Hex = (ppc_state.msr.Hex & ~mask) | (SRR1(ppc_state) & mask);
  // MSR[13] is set to 0.
  ppc_state.msr.Hex &= 0xFFFBFFFF;
  // Here we should check if there are pending exceptions, and if their corresponding enable bits
  // are set
  // if above is true, we'd do:
  // PowerPC::CheckExceptions();
  // else
  // set NPC to saved offset and resume
  ppc_state.npc = SRR0(ppc_state);

  PowerPC::MSRUpdated(ppc_state);

  interpreter.m_end_block = true;
}

// sc isn't really used for anything important in GameCube games (just for a write barrier) so we
// really don't have to emulate it.
// We do it anyway, though :P
void Interpreter::sc(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;

  ppc_state.Exceptions |= EXCEPTION_SYSCALL;
  interpreter.m_system.GetPowerPC().CheckExceptions();
  interpreter.m_end_block = true;
}
