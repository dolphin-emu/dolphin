// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

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
           (u64{PowerPC::GetXER_SO()} << PowerPC::CR_EMU_SO_BIT);

  PowerPC::ppcState.cr.fields[0] = cr_val;
}

u32 Interpreter::Helper_Carry(u32 value1, u32 value2)
{
  return value2 > (~value1);
}

void Interpreter::addi(GeckoInstruction inst)
{
  if (inst.RA())
    rGPR[inst.RD()] = rGPR[inst.RA()] + u32(inst.SIMM_16());
  else
    rGPR[inst.RD()] = u32(inst.SIMM_16());
}

void Interpreter::addic(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 imm = u32(s32{inst.SIMM_16()});

  rGPR[inst.RD()] = a + imm;
  PowerPC::SetCarry(Helper_Carry(a, imm));
}

void Interpreter::addic_rc(GeckoInstruction inst)
{
  addic(inst);
  Helper_UpdateCR0(rGPR[inst.RD()]);
}

void Interpreter::addis(GeckoInstruction inst)
{
  if (inst.RA())
    rGPR[inst.RD()] = rGPR[inst.RA()] + u32(inst.SIMM_16() << 16);
  else
    rGPR[inst.RD()] = u32(inst.SIMM_16() << 16);
}

void Interpreter::andi_rc(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] & inst.UIMM();
  Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::andis_rc(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] & (u32{inst.UIMM()} << 16);
  Helper_UpdateCR0(rGPR[inst.RA()]);
}

template <typename T>
void Interpreter::Helper_IntCompare(GeckoInstruction inst, T a, T b)
{
  u32 cr_field;

  if (a < b)
    cr_field = PowerPC::CR_LT;
  else if (a > b)
    cr_field = PowerPC::CR_GT;
  else
    cr_field = PowerPC::CR_EQ;

  if (PowerPC::GetXER_SO())
    cr_field |= PowerPC::CR_SO;

  PowerPC::ppcState.cr.SetField(inst.CRFD(), cr_field);
}

void Interpreter::cmpi(GeckoInstruction inst)
{
  const s32 a = static_cast<s32>(rGPR[inst.RA()]);
  const s32 b = inst.SIMM_16();
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cmpli(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 b = inst.UIMM();
  Helper_IntCompare(inst, a, b);
}

void Interpreter::mulli(GeckoInstruction inst)
{
  rGPR[inst.RD()] = u32(s32(rGPR[inst.RA()]) * inst.SIMM_16());
}

void Interpreter::ori(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] | inst.UIMM();
}

void Interpreter::oris(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] | (u32{inst.UIMM()} << 16);
}

void Interpreter::subfic(GeckoInstruction inst)
{
  const s32 immediate = inst.SIMM_16();
  rGPR[inst.RD()] = u32(immediate - s32(rGPR[inst.RA()]));
  PowerPC::SetCarry((rGPR[inst.RA()] == 0) || (Helper_Carry(0 - rGPR[inst.RA()], u32(immediate))));
}

void Interpreter::twi(GeckoInstruction inst)
{
  const s32 a = s32(rGPR[inst.RA()]);
  const s32 b = inst.SIMM_16();
  const u32 TO = inst.TO();

  DEBUG_LOG_FMT(POWERPC, "twi rA {:x} SIMM {:x} TO {:x}", a, b, TO);

  if ((a < b && (TO & 0x10) != 0) || (a > b && (TO & 0x08) != 0) || (a == b && (TO & 0x04) != 0) ||
      (u32(a) < u32(b) && (TO & 0x02) != 0) || (u32(a) > u32(b) && (TO & 0x01) != 0))
  {
    GenerateProgramException(ProgramExceptionCause::Trap);
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xori(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] ^ inst.UIMM();
}

void Interpreter::xoris(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] ^ (u32{inst.UIMM()} << 16);
}

void Interpreter::rlwimix(GeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB(), inst.ME());
  rGPR[inst.RA()] =
      (rGPR[inst.RA()] & ~mask) | (Common::RotateLeft(rGPR[inst.RS()], inst.SH()) & mask);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::rlwinmx(GeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB(), inst.ME());
  rGPR[inst.RA()] = Common::RotateLeft(rGPR[inst.RS()], inst.SH()) & mask;

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::rlwnmx(GeckoInstruction inst)
{
  const u32 mask = MakeRotationMask(inst.MB(), inst.ME());
  rGPR[inst.RA()] = Common::RotateLeft(rGPR[inst.RS()], rGPR[inst.RB()] & 0x1F) & mask;

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::andx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] & rGPR[inst.RB()];

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::andcx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] & ~rGPR[inst.RB()];

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::cmp(GeckoInstruction inst)
{
  const s32 a = static_cast<s32>(rGPR[inst.RA()]);
  const s32 b = static_cast<s32>(rGPR[inst.RB()]);
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cmpl(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  Helper_IntCompare(inst, a, b);
}

void Interpreter::cntlzwx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = u32(Common::CountLeadingZeros(rGPR[inst.RS()]));

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::eqvx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = ~(rGPR[inst.RS()] ^ rGPR[inst.RB()]);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::extsbx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = u32(s32(s8(rGPR[inst.RS()])));

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::extshx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = u32(s32(s16(rGPR[inst.RS()])));

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::nandx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = ~(rGPR[inst.RS()] & rGPR[inst.RB()]);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::norx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = ~(rGPR[inst.RS()] | rGPR[inst.RB()]);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::orx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] | rGPR[inst.RB()];

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::orcx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] | (~rGPR[inst.RB()]);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::slwx(GeckoInstruction inst)
{
  const u32 amount = rGPR[inst.RB()];
  rGPR[inst.RA()] = (amount & 0x20) != 0 ? 0 : rGPR[inst.RS()] << (amount & 0x1f);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::srawx(GeckoInstruction inst)
{
  const u32 rb = rGPR[inst.RB()];

  if ((rb & 0x20) != 0)
  {
    if ((rGPR[inst.RS()] & 0x80000000) != 0)
    {
      rGPR[inst.RA()] = 0xFFFFFFFF;
      PowerPC::SetCarry(1);
    }
    else
    {
      rGPR[inst.RA()] = 0x00000000;
      PowerPC::SetCarry(0);
    }
  }
  else
  {
    const u32 amount = rb & 0x1f;
    const s32 rrs = s32(rGPR[inst.RS()]);
    rGPR[inst.RA()] = u32(rrs >> amount);

    PowerPC::SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);
  }

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::srawix(GeckoInstruction inst)
{
  const u32 amount = inst.SH();
  const s32 rrs = s32(rGPR[inst.RS()]);

  rGPR[inst.RA()] = u32(rrs >> amount);
  PowerPC::SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::srwx(GeckoInstruction inst)
{
  const u32 amount = rGPR[inst.RB()];
  rGPR[inst.RA()] = (amount & 0x20) != 0 ? 0 : (rGPR[inst.RS()] >> (amount & 0x1f));

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

void Interpreter::tw(GeckoInstruction inst)
{
  const s32 a = s32(rGPR[inst.RA()]);
  const s32 b = s32(rGPR[inst.RB()]);
  const u32 TO = inst.TO();

  DEBUG_LOG_FMT(POWERPC, "tw rA {:x} rB {:x} TO {:x}", a, b, TO);

  if ((a < b && (TO & 0x10) != 0) || (a > b && (TO & 0x08) != 0) || (a == b && (TO & 0x04) != 0) ||
      ((u32(a) < u32(b)) && (TO & 0x02) != 0) || ((u32(a) > u32(b)) && (TO & 0x01) != 0))
  {
    GenerateProgramException(ProgramExceptionCause::Trap);
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xorx(GeckoInstruction inst)
{
  rGPR[inst.RA()] = rGPR[inst.RS()] ^ rGPR[inst.RB()];

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RA()]);
}

static bool HasAddOverflowed(u32 x, u32 y, u32 result)
{
  // If x and y have the same sign, but the result is different
  // then an overflow has occurred.
  return (((x ^ result) & (y ^ result)) >> 31) != 0;
}

void Interpreter::addx(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 result = a + b;

  rGPR[inst.RD()] = result;

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::addcx(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 result = a + b;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, b));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::addex(GeckoInstruction inst)
{
  const u32 carry = PowerPC::GetCarry();
  const u32 a = rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 result = a + b + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, b) || (carry != 0 && Helper_Carry(a + b, carry)));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::addmex(GeckoInstruction inst)
{
  const u32 carry = PowerPC::GetCarry();
  const u32 a = rGPR[inst.RA()];
  const u32 b = 0xFFFFFFFF;
  const u32 result = a + b + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, carry - 1));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::addzex(GeckoInstruction inst)
{
  const u32 carry = PowerPC::GetCarry();
  const u32 a = rGPR[inst.RA()];
  const u32 result = a + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, carry));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, 0, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::divwx(GeckoInstruction inst)
{
  const auto a = s32(rGPR[inst.RA()]);
  const auto b = s32(rGPR[inst.RB()]);
  const bool overflow = b == 0 || (static_cast<u32>(a) == 0x80000000 && b == -1);

  if (overflow)
  {
    if (a < 0)
      rGPR[inst.RD()] = UINT32_MAX;
    else
      rGPR[inst.RD()] = 0;
  }
  else
  {
    rGPR[inst.RD()] = static_cast<u32>(a / b);
  }

  if (inst.OE())
    PowerPC::SetXER_OV(overflow);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RD()]);
}

void Interpreter::divwux(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const bool overflow = b == 0;

  if (overflow)
  {
    rGPR[inst.RD()] = 0;
  }
  else
  {
    rGPR[inst.RD()] = a / b;
  }

  if (inst.OE())
    PowerPC::SetXER_OV(overflow);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RD()]);
}

void Interpreter::mulhwx(GeckoInstruction inst)
{
  const s64 a = static_cast<s32>(rGPR[inst.RA()]);
  const s64 b = static_cast<s32>(rGPR[inst.RB()]);
  const u32 d = static_cast<u32>((a * b) >> 32);

  rGPR[inst.RD()] = d;

  if (inst.Rc())
    Helper_UpdateCR0(d);
}

void Interpreter::mulhwux(GeckoInstruction inst)
{
  const u64 a = rGPR[inst.RA()];
  const u64 b = rGPR[inst.RB()];
  const u32 d = static_cast<u32>((a * b) >> 32);

  rGPR[inst.RD()] = d;

  if (inst.Rc())
    Helper_UpdateCR0(d);
}

void Interpreter::mullwx(GeckoInstruction inst)
{
  const s64 a = static_cast<s32>(rGPR[inst.RA()]);
  const s64 b = static_cast<s32>(rGPR[inst.RB()]);
  const s64 result = a * b;

  rGPR[inst.RD()] = static_cast<u32>(result);

  if (inst.OE())
    PowerPC::SetXER_OV(result < -0x80000000LL || result > 0x7FFFFFFFLL);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RD()]);
}

void Interpreter::negx(GeckoInstruction inst)
{
  const u32 a = rGPR[inst.RA()];

  rGPR[inst.RD()] = (~a) + 1;

  if (inst.OE())
    PowerPC::SetXER_OV(a == 0x80000000);

  if (inst.Rc())
    Helper_UpdateCR0(rGPR[inst.RD()]);
}

void Interpreter::subfx(GeckoInstruction inst)
{
  const u32 a = ~rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 result = a + b + 1;

  rGPR[inst.RD()] = result;

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::subfcx(GeckoInstruction inst)
{
  const u32 a = ~rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 result = a + b + 1;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(a == 0xFFFFFFFF || Helper_Carry(b, a + 1));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

void Interpreter::subfex(GeckoInstruction inst)
{
  const u32 a = ~rGPR[inst.RA()];
  const u32 b = rGPR[inst.RB()];
  const u32 carry = PowerPC::GetCarry();
  const u32 result = a + b + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, b) || Helper_Carry(a + b, carry));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

// sub from minus one
void Interpreter::subfmex(GeckoInstruction inst)
{
  const u32 a = ~rGPR[inst.RA()];
  const u32 b = 0xFFFFFFFF;
  const u32 carry = PowerPC::GetCarry();
  const u32 result = a + b + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, carry - 1));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, b, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}

// sub from zero
void Interpreter::subfzex(GeckoInstruction inst)
{
  const u32 a = ~rGPR[inst.RA()];
  const u32 carry = PowerPC::GetCarry();
  const u32 result = a + carry;

  rGPR[inst.RD()] = result;
  PowerPC::SetCarry(Helper_Carry(a, carry));

  if (inst.OE())
    PowerPC::SetXER_OV(HasAddOverflowed(a, 0, result));

  if (inst.Rc())
    Helper_UpdateCR0(result);
}
