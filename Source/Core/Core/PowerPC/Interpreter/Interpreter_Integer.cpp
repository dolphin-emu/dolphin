// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <bit>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/PowerPC.h"

void Interpreter::Helper_UpdateCR0(u32 value)
{
  const s64 sign_extended = s64{s32(value)};
  u64 cr_val = u64(sign_extended);
  cr_val = (cr_val & ~(1ULL << PowerPC::CR_EMU_SO_BIT)) |
           (u64{PowerPC::ppcState.GetXER_SO()} << PowerPC::CR_EMU_SO_BIT);

  PowerPC::ppcState.cr.fields[0] = cr_val;
}

u32 Interpreter::Helper_Carry(u32 value1, u32 value2)
{
  return value2 > (~value1);
}

void Interpreter::addi(UGeckoInstruction inst)
{
  if (inst.RA)
    PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.gpr[inst.RA] + u32(inst.SIMM_16);
  else
    PowerPC::ppcState.gpr[inst.RD] = u32(inst.SIMM_16);
}

void Interpreter::addic(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 imm = u32(s32{inst.SIMM_16});

  PowerPC::ppcState.gpr[inst.RD] = a + imm;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, imm));
}

void Interpreter::addic_rc(UGeckoInstruction inst)
{
  addic(inst);
  Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RD]);
}

void Interpreter::addis(UGeckoInstruction inst)
{
  if (inst.RA)
    PowerPC::ppcState.gpr[inst.RD] = PowerPC::ppcState.gpr[inst.RA] + u32(inst.SIMM_16 << 16);
  else
    PowerPC::ppcState.gpr[inst.RD] = u32(inst.SIMM_16 << 16);
}

void Interpreter::andi_rc(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] & inst.UIMM;
  Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::andis_rc(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] & (u32{inst.UIMM} << 16);
  Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

template <typename T>
void Interpreter::Helper_IntCompare(UGeckoInstruction inst, T a, T b)
{
  u32 cr_field;

  if (a < b)
    cr_field = PowerPC::CR_LT;
  else if (a > b)
    cr_field = PowerPC::CR_GT;
  else
    cr_field = PowerPC::CR_EQ;

  if (PowerPC::ppcState.GetXER_SO())
    cr_field |= PowerPC::CR_SO;

  PowerPC::ppcState.cr.SetField(inst.CRFD, cr_field);
}

void Interpreter::cmpi(UGeckoInstruction inst)
{
  const s32 a = static_cast<s32>(PowerPC::ppcState.gpr[inst.RA]);
  const s32 b = inst.SIMM_16;
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cmpli(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = inst.UIMM;
  Helper_IntCompare(inst, a, b);
}

void Interpreter::mulli(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RD] = u32(s32(PowerPC::ppcState.gpr[inst.RA]) * inst.SIMM_16);
}

void Interpreter::ori(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] | inst.UIMM;
}

void Interpreter::oris(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] | (u32{inst.UIMM} << 16);
}

void Interpreter::subfic(UGeckoInstruction inst)
{
  const s32 immediate = inst.SIMM_16;
  PowerPC::ppcState.gpr[inst.RD] = u32(immediate - s32(PowerPC::ppcState.gpr[inst.RA]));
  PowerPC::ppcState.SetCarry((PowerPC::ppcState.gpr[inst.RA] == 0) ||
                             (Helper_Carry(0 - PowerPC::ppcState.gpr[inst.RA], u32(immediate))));
}

void Interpreter::twi(UGeckoInstruction inst)
{
  const s32 a = s32(PowerPC::ppcState.gpr[inst.RA]);
  const s32 b = inst.SIMM_16;
  const u32 TO = inst.TO;

  DEBUG_LOG_FMT(POWERPC, "twi rA {:x} SIMM {:x} TO {:x}", a, b, TO);

  if ((a < b && (TO & 0x10) != 0) || (a > b && (TO & 0x08) != 0) || (a == b && (TO & 0x04) != 0) ||
      (u32(a) < u32(b) && (TO & 0x02) != 0) || (u32(a) > u32(b) && (TO & 0x01) != 0))
  {
    GenerateProgramException(ProgramExceptionCause::Trap);
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xori(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] ^ inst.UIMM;
}

void Interpreter::xoris(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] ^ (u32{inst.UIMM} << 16);
}

void Interpreter::rlwimix(UGeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  PowerPC::ppcState.gpr[inst.RA] = (PowerPC::ppcState.gpr[inst.RA] & ~mask) |
                                   (std::rotl(PowerPC::ppcState.gpr[inst.RS], inst.SH) & mask);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::rlwinmx(UGeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  PowerPC::ppcState.gpr[inst.RA] = std::rotl(PowerPC::ppcState.gpr[inst.RS], inst.SH) & mask;

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::rlwnmx(UGeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  PowerPC::ppcState.gpr[inst.RA] =
      std::rotl(PowerPC::ppcState.gpr[inst.RS], PowerPC::ppcState.gpr[inst.RB] & 0x1F) & mask;

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::andx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] & PowerPC::ppcState.gpr[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::andcx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] & ~PowerPC::ppcState.gpr[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::cmp(UGeckoInstruction inst)
{
  const s32 a = static_cast<s32>(PowerPC::ppcState.gpr[inst.RA]);
  const s32 b = static_cast<s32>(PowerPC::ppcState.gpr[inst.RB]);
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cmpl(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cntlzwx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = u32(std::countl_zero(PowerPC::ppcState.gpr[inst.RS]));

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::eqvx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] =
      ~(PowerPC::ppcState.gpr[inst.RS] ^ PowerPC::ppcState.gpr[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::extsbx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = u32(s32(s8(PowerPC::ppcState.gpr[inst.RS])));

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::extshx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = u32(s32(s16(PowerPC::ppcState.gpr[inst.RS])));

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::nandx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] =
      ~(PowerPC::ppcState.gpr[inst.RS] & PowerPC::ppcState.gpr[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::norx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] =
      ~(PowerPC::ppcState.gpr[inst.RS] | PowerPC::ppcState.gpr[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::orx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] | PowerPC::ppcState.gpr[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::orcx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] =
      PowerPC::ppcState.gpr[inst.RS] | (~PowerPC::ppcState.gpr[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::slwx(UGeckoInstruction inst)
{
  const u32 amount = PowerPC::ppcState.gpr[inst.RB];
  PowerPC::ppcState.gpr[inst.RA] =
      (amount & 0x20) != 0 ? 0 : PowerPC::ppcState.gpr[inst.RS] << (amount & 0x1f);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::srawx(UGeckoInstruction inst)
{
  const u32 rb = PowerPC::ppcState.gpr[inst.RB];

  if ((rb & 0x20) != 0)
  {
    if ((PowerPC::ppcState.gpr[inst.RS] & 0x80000000) != 0)
    {
      PowerPC::ppcState.gpr[inst.RA] = 0xFFFFFFFF;
      PowerPC::ppcState.SetCarry(1);
    }
    else
    {
      PowerPC::ppcState.gpr[inst.RA] = 0x00000000;
      PowerPC::ppcState.SetCarry(0);
    }
  }
  else
  {
    const u32 amount = rb & 0x1f;
    const s32 rrs = s32(PowerPC::ppcState.gpr[inst.RS]);
    PowerPC::ppcState.gpr[inst.RA] = u32(rrs >> amount);

    PowerPC::ppcState.SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);
  }

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::srawix(UGeckoInstruction inst)
{
  const u32 amount = inst.SH;
  const s32 rrs = s32(PowerPC::ppcState.gpr[inst.RS]);

  PowerPC::ppcState.gpr[inst.RA] = u32(rrs >> amount);
  PowerPC::ppcState.SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::srwx(UGeckoInstruction inst)
{
  const u32 amount = PowerPC::ppcState.gpr[inst.RB];
  PowerPC::ppcState.gpr[inst.RA] =
      (amount & 0x20) != 0 ? 0 : (PowerPC::ppcState.gpr[inst.RS] >> (amount & 0x1f));

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

void Interpreter::tw(UGeckoInstruction inst)
{
  const s32 a = s32(PowerPC::ppcState.gpr[inst.RA]);
  const s32 b = s32(PowerPC::ppcState.gpr[inst.RB]);
  const u32 TO = inst.TO;

  DEBUG_LOG_FMT(POWERPC, "tw rA {:x} rB {:x} TO {:x}", a, b, TO);

  if ((a < b && (TO & 0x10) != 0) || (a > b && (TO & 0x08) != 0) || (a == b && (TO & 0x04) != 0) ||
      ((u32(a) < u32(b)) && (TO & 0x02) != 0) || ((u32(a) > u32(b)) && (TO & 0x01) != 0))
  {
    GenerateProgramException(ProgramExceptionCause::Trap);
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xorx(UGeckoInstruction inst)
{
  PowerPC::ppcState.gpr[inst.RA] = PowerPC::ppcState.gpr[inst.RS] ^ PowerPC::ppcState.gpr[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RA]);
}

static bool HasAddOverflowed(u32 x, u32 y, u32 result)
{
  // If x and y have the same sign, but the result is different
  // then an overflow has occurred.
  return (((x ^ result) & (y ^ result)) >> 31) != 0;
}

void Interpreter::addx(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 result = a + b;

  PowerPC::ppcState.gpr[inst.RD] = result;

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::addcx(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 result = a + b;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, b));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::addex(UGeckoInstruction inst)
{
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 result = a + b + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, b) || (carry != 0 && Helper_Carry(a + b, carry)));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::addmex(UGeckoInstruction inst)
{
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = 0xFFFFFFFF;
  const u32 result = a + b + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, carry - 1));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::addzex(UGeckoInstruction inst)
{
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 result = a + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, carry));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, 0, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::divwx(UGeckoInstruction inst)
{
  const auto a = s32(PowerPC::ppcState.gpr[inst.RA]);
  const auto b = s32(PowerPC::ppcState.gpr[inst.RB]);
  const bool overflow = b == 0 || (static_cast<u32>(a) == 0x80000000 && b == -1);

  if (overflow)
  {
    if (a < 0)
      PowerPC::ppcState.gpr[inst.RD] = UINT32_MAX;
    else
      PowerPC::ppcState.gpr[inst.RD] = 0;
  }
  else
  {
    PowerPC::ppcState.gpr[inst.RD] = static_cast<u32>(a / b);
  }

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(overflow);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RD]);
}

void Interpreter::divwux(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const bool overflow = b == 0;

  if (overflow)
  {
    PowerPC::ppcState.gpr[inst.RD] = 0;
  }
  else
  {
    PowerPC::ppcState.gpr[inst.RD] = a / b;
  }

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(overflow);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RD]);
}

void Interpreter::mulhwx(UGeckoInstruction inst)
{
  const s64 a = static_cast<s32>(PowerPC::ppcState.gpr[inst.RA]);
  const s64 b = static_cast<s32>(PowerPC::ppcState.gpr[inst.RB]);
  const u32 d = static_cast<u32>((a * b) >> 32);

  PowerPC::ppcState.gpr[inst.RD] = d;

  if (inst.Rc)
    Helper_UpdateCR0(d);
}

void Interpreter::mulhwux(UGeckoInstruction inst)
{
  const u64 a = PowerPC::ppcState.gpr[inst.RA];
  const u64 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 d = static_cast<u32>((a * b) >> 32);

  PowerPC::ppcState.gpr[inst.RD] = d;

  if (inst.Rc)
    Helper_UpdateCR0(d);
}

void Interpreter::mullwx(UGeckoInstruction inst)
{
  const s64 a = static_cast<s32>(PowerPC::ppcState.gpr[inst.RA]);
  const s64 b = static_cast<s32>(PowerPC::ppcState.gpr[inst.RB]);
  const s64 result = a * b;

  PowerPC::ppcState.gpr[inst.RD] = static_cast<u32>(result);

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(result < -0x80000000LL || result > 0x7FFFFFFFLL);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RD]);
}

void Interpreter::negx(UGeckoInstruction inst)
{
  const u32 a = PowerPC::ppcState.gpr[inst.RA];

  PowerPC::ppcState.gpr[inst.RD] = (~a) + 1;

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(a == 0x80000000);

  if (inst.Rc)
    Helper_UpdateCR0(PowerPC::ppcState.gpr[inst.RD]);
}

void Interpreter::subfx(UGeckoInstruction inst)
{
  const u32 a = ~PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 result = a + b + 1;

  PowerPC::ppcState.gpr[inst.RD] = result;

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::subfcx(UGeckoInstruction inst)
{
  const u32 a = ~PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 result = a + b + 1;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(a == 0xFFFFFFFF || Helper_Carry(b, a + 1));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

void Interpreter::subfex(UGeckoInstruction inst)
{
  const u32 a = ~PowerPC::ppcState.gpr[inst.RA];
  const u32 b = PowerPC::ppcState.gpr[inst.RB];
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 result = a + b + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, b) || Helper_Carry(a + b, carry));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

// sub from minus one
void Interpreter::subfmex(UGeckoInstruction inst)
{
  const u32 a = ~PowerPC::ppcState.gpr[inst.RA];
  const u32 b = 0xFFFFFFFF;
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 result = a + b + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, carry - 1));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}

// sub from zero
void Interpreter::subfzex(UGeckoInstruction inst)
{
  const u32 a = ~PowerPC::ppcState.gpr[inst.RA];
  const u32 carry = PowerPC::ppcState.GetCarry();
  const u32 result = a + carry;

  PowerPC::ppcState.gpr[inst.RD] = result;
  PowerPC::ppcState.SetCarry(Helper_Carry(a, carry));

  if (inst.OE)
    PowerPC::ppcState.SetXER_OV(HasAddOverflowed(a, 0, result));

  if (inst.Rc)
    Helper_UpdateCR0(result);
}
