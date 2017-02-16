// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/ARBruteForcer.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HLE/HLE.h"
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

  if (NPC == PC && SConfig::GetInstance().bSkipIdle)
  {
    CoreTiming::Idle();
  }
}

// bcx - ugly, straight from PPC manual equations :)
void Interpreter::bcx(UGeckoInstruction inst)
{
  if ((inst.BO & BO_DONT_DECREMENT_FLAG) == 0)
    CTR--;

  const bool true_false = ((inst.BO >> 3) & 1);
  const bool only_counter_check = ((inst.BO >> 4) & 1);
  const bool only_condition_check = ((inst.BO >> 2) & 1);
  int ctr_check = ((CTR != 0) ^ (inst.BO >> 1)) & 1;
  bool counter = only_condition_check || ctr_check;
  bool condition = only_counter_check || (GetCRBit(inst.BI) == u32(true_false));

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

  // this code trys to detect the most common idle loop:
  // lwz r0, XXXX(r13)
  // cmpXwi r0,0
  // beq -8
  if (NPC == PC - 8 && inst.hex == 0x4182fff8 /* beq */ && SConfig::GetInstance().bSkipIdle)
  {
    if (PowerPC::HostRead_U32(PC - 8) >> 16 == 0x800D /* lwz */)
    {
      u32 last_inst = PowerPC::HostRead_U32(PC - 4);

      if (last_inst == 0x28000000 /* cmplwi */ ||
          (last_inst == 0x2C000000 /* cmpwi */ && SConfig::GetInstance().bWii))
      {
        CoreTiming::Idle();
      }
    }
  }
}

void Interpreter::bcctrx(UGeckoInstruction inst)
{
  _dbg_assert_msg_(POWERPC, inst.BO_2 & BO_DONT_DECREMENT_FLAG,
                   "bcctrx with decrement and test CTR option is invalid!");

  int condition = ((inst.BO_2 >> 4) | (GetCRBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

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

  int counter = ((inst.BO_2 >> 2) | ((CTR != 0) ^ (inst.BO_2 >> 1))) & 1;
  int condition = ((inst.BO_2 >> 4) | (GetCRBit(inst.BI_2) == ((inst.BO_2 >> 3) & 1))) & 1;

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
  // Restore saved bits from SRR1 to MSR.
  // Gecko/Broadway can save more bits than explicitly defined in ppc spec
  const int mask = 0x87C0FFFF;
  MSR = (MSR & ~mask) | (SRR1 & mask);
  // MSR[13] is set to 0.
  MSR &= 0xFFFBFFFF;
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
