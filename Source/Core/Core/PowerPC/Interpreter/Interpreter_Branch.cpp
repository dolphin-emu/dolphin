// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

void Interpreter::bx(UGeckoInstruction inst)
{
  if (inst.LK)
    LR = PC + 4;

  if (inst.AA)
    NPC = SignExt26(inst.LI << 2);
  else
    NPC = PC + SignExt26(inst.LI << 2);

  m_end_block = true;
}

// bcx - ugly, straight from PPC manual equations :)
void Interpreter::bcx(UGeckoInstruction inst)
{
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    CTR--;

  const bool true_false = ((inst.BO >> 3) & 1);
  const bool only_counter_check = ((inst.BO >> 4) & 1);
  const bool only_condition_check = ((inst.BO >> 2) & 1);
  const u32 ctr_check = ((CTR != 0) ^ (inst.BO >> 1)) & 1;
  const bool counter = only_condition_check || ctr_check;
  const bool condition =
      only_counter_check || (PowerPC::ppcState.cr.GetBit(inst.BI) == u32(true_false));

  if (counter && condition)
  {
    if (inst.LK)
      LR = PC + 4;

    if (inst.AA)
      NPC = SignExt16(inst.BD << 2);
    else
      NPC = PC + SignExt16(inst.BD << 2);
  }

  m_end_block = true;
}

void Interpreter::bcctrx(UGeckoInstruction inst)
{
  DEBUG_ASSERT_MSG(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG,
                   "bcctrx with decrement and test CTR option is invalid!");

  const u32 condition =
      ((inst.BO_2 >> 4) | (PowerPC::ppcState.cr.GetBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

  if (condition)
  {
    NPC = CTR & (~3);
    if (inst.LK_3)
      LR = PC + 4;
  }

  m_end_block = true;
}

void Interpreter::bclrx(UGeckoInstruction inst)
{
  if ((inst.BO_2 & BO_DONT_DECREMENT_FLAG) == 0)
    CTR--;

  const u32 counter = ((inst.BO_2 >> 2) | ((CTR != 0) ^ (inst.BO_2 >> 1))) & 1;
  const u32 condition =
      ((inst.BO_2 >> 4) | (PowerPC::ppcState.cr.GetBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

  if (counter & condition)
  {
    NPC = LR & (~3);
    if (inst.LK_3)
      LR = PC + 4;
  }

  m_end_block = true;
}

void Interpreter::HLEFunction(UGeckoInstruction inst)
{
  m_end_block = true;
  HLE::Execute(PC, inst.hex);
}

void Interpreter::rfi(UGeckoInstruction inst)
{
  if (MSR.PR)
  {
    GenerateProgramException();
    return;
  }

  // Restore saved bits from SRR1 to MSR.
  // Gecko/Broadway can save more bits than explicitly defined in ppc spec
  const u32 mask = 0x87C0FFFF;
  MSR.Hex = (MSR.Hex & ~mask) | (SRR1 & mask);
  // MSR[13] is set to 0.
  MSR.Hex &= 0xFFFBFFFF;
  // Here we should check if there are pending exceptions, and if their corresponding enable bits
  // are set
  // if above is true, we'd do:
  // PowerPC::CheckExceptions();
  // else
  // set NPC to saved offset and resume
  NPC = SRR0;
  m_end_block = true;
}

// sc isn't really used for anything important in GameCube games (just for a write barrier) so we
// really don't have to emulate it.
// We do it anyway, though :P
void Interpreter::sc(UGeckoInstruction inst)
{
  PowerPC::ppcState.Exceptions |= EXCEPTION_SYSCALL;
  PowerPC::CheckExceptions();
  m_end_block = true;
}
