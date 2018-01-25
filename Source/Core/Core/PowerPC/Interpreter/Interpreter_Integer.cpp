// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/PowerPC/PowerPC.h"

void Interpreter::Helper_UpdateCR0(u32 value)
{
  s64 sign_extended = (s64)(s32)value;
  u64 cr_val = (u64)sign_extended;
  cr_val = (cr_val & ~(1ull << 61)) | ((u64)GetXER_SO() << 61);

  PowerPC::ppcState.cr_val[0] = cr_val;
}

u32 Interpreter::Helper_Carry(u32 value1, u32 value2)
{
  return value2 > (~value1);
}

u32 Interpreter::Helper_Mask(int mb, int me)
{
  // first make 001111111111111 part
  u32 begin = 0xFFFFFFFF >> mb;
  // then make 000000000001111 part, which is used to flip the bits of the first one
  u32 end = 0x7FFFFFFF >> me;
  // do the bitflip
  u32 mask = begin ^ end;
  // and invert if backwards
  if (me < mb)
    return ~mask;
  else
    return mask;
}

void Interpreter::addi(UGeckoInstruction inst)
{
  if (inst.RA)
    rGPR[inst.RD] = rGPR[inst.RA] + inst.SIMM_16;
  else
    rGPR[inst.RD] = inst.SIMM_16;
}

void Interpreter::addic(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 imm = (u32)(s32)inst.SIMM_16;
  // TODO(ector): verify this thing
  rGPR[inst.RD] = a + imm;
  SetCarry(Helper_Carry(a, imm));
}

void Interpreter::addic_rc(UGeckoInstruction inst)
{
  addic(inst);
  Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::addis(UGeckoInstruction inst)
{
  if (inst.RA)
    rGPR[inst.RD] = rGPR[inst.RA] + (inst.SIMM_16 << 16);
  else
    rGPR[inst.RD] = (inst.SIMM_16 << 16);
}

void Interpreter::andi_rc(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] & inst.UIMM;
  Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::andis_rc(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] & ((u32)inst.UIMM << 16);
  Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::cmpi(UGeckoInstruction inst)
{
  s32 a = rGPR[inst.RA];
  s32 b = inst.SIMM_16;
  int f;

  if (a < b)
    f = 0x8;
  else if (a > b)
    f = 0x4;
  else
    f = 0x2;  // equals

  if (GetXER_SO())
    f |= 0x1;

  SetCRField(inst.CRFD, f);
}

void Interpreter::cmpli(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = inst.UIMM;
  int f;

  if (a < b)
    f = 0x8;
  else if (a > b)
    f = 0x4;
  else
    f = 0x2;  // equals

  if (GetXER_SO())
    f |= 0x1;

  SetCRField(inst.CRFD, f);
}

void Interpreter::mulli(UGeckoInstruction inst)
{
  rGPR[inst.RD] = (s32)rGPR[inst.RA] * inst.SIMM_16;
}

void Interpreter::ori(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] | inst.UIMM;
}

void Interpreter::oris(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] | (inst.UIMM << 16);
}

void Interpreter::subfic(UGeckoInstruction inst)
{
  /*	u32 rra = ~rGPR[inst.RA];
    s32 immediate = (s16)inst.SIMM_16 + 1;

  //	#define CALC_XER_CA(X,Y) (((X) + (Y) < X) ? SET_XER_CA : CLEAR_XER_CA)
    if ((rra + immediate) < rra)
      SetCarry(1);
    else
      SetCarry(0);

    rGPR[inst.RD] = rra - immediate;
  */

  s32 immediate = inst.SIMM_16;
  rGPR[inst.RD] = immediate - (int)rGPR[inst.RA];
  SetCarry((rGPR[inst.RA] == 0) || (Helper_Carry(0 - rGPR[inst.RA], immediate)));
}

void Interpreter::twi(UGeckoInstruction inst)
{
  s32 a = rGPR[inst.RA];
  s32 b = inst.SIMM_16;
  s32 TO = inst.TO;

  DEBUG_LOG(POWERPC, "twi rA %x SIMM %x TO %0x", a, b, TO);

  if (((a < b) && (TO & 0x10)) || ((a > b) && (TO & 0x08)) || ((a == b) && (TO & 0x04)) ||
      (((u32)a < (u32)b) && (TO & 0x02)) || (((u32)a > (u32)b) && (TO & 0x01)))
  {
    PowerPC::ppcState.Exceptions |= EXCEPTION_PROGRAM;
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xori(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] ^ inst.UIMM;
}

void Interpreter::xoris(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] ^ (inst.UIMM << 16);
}

void Interpreter::rlwimix(UGeckoInstruction inst)
{
  u32 mask = Helper_Mask(inst.MB, inst.ME);
  rGPR[inst.RA] = (rGPR[inst.RA] & ~mask) | (_rotl(rGPR[inst.RS], inst.SH) & mask);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::rlwinmx(UGeckoInstruction inst)
{
  u32 mask = Helper_Mask(inst.MB, inst.ME);
  rGPR[inst.RA] = _rotl(rGPR[inst.RS], inst.SH) & mask;

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::rlwnmx(UGeckoInstruction inst)
{
  u32 mask = Helper_Mask(inst.MB, inst.ME);
  rGPR[inst.RA] = _rotl(rGPR[inst.RS], rGPR[inst.RB] & 0x1F) & mask;

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::andx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] & rGPR[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::andcx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] & ~rGPR[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::cmp(UGeckoInstruction inst)
{
  s32 a = (s32)rGPR[inst.RA];
  s32 b = (s32)rGPR[inst.RB];
  int fTemp;

  if (a < b)
    fTemp = 0x8;
  else if (a > b)
    fTemp = 0x4;
  else  // Equals
    fTemp = 0x2;

  if (GetXER_SO())
    fTemp |= 0x1;

  SetCRField(inst.CRFD, fTemp);
}

void Interpreter::cmpl(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  u32 fTemp;

  if (a < b)
    fTemp = 0x8;
  else if (a > b)
    fTemp = 0x4;
  else  // Equals
    fTemp = 0x2;

  if (GetXER_SO())
    fTemp |= 0x1;

  SetCRField(inst.CRFD, fTemp);
}

void Interpreter::cntlzwx(UGeckoInstruction inst)
{
  u32 val = rGPR[inst.RS];
  u32 mask = 0x80000000;

  int i = 0;
  for (; i < 32; i++, mask >>= 1)
  {
    if (val & mask)
      break;
  }

  rGPR[inst.RA] = i;

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::eqvx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = ~(rGPR[inst.RS] ^ rGPR[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::extsbx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = (u32)(s32)(s8)rGPR[inst.RS];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::extshx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = (u32)(s32)(s16)rGPR[inst.RS];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::nandx(UGeckoInstruction _inst)
{
  rGPR[_inst.RA] = ~(rGPR[_inst.RS] & rGPR[_inst.RB]);

  if (_inst.Rc)
    Helper_UpdateCR0(rGPR[_inst.RA]);
}

void Interpreter::norx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = ~(rGPR[inst.RS] | rGPR[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::orx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] | rGPR[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::orcx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] | (~rGPR[inst.RB]);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::slwx(UGeckoInstruction inst)
{
  u32 amount = rGPR[inst.RB];
  rGPR[inst.RA] = (amount & 0x20) ? 0 : rGPR[inst.RS] << (amount & 0x1f);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::srawx(UGeckoInstruction inst)
{
  int rb = rGPR[inst.RB];

  if (rb & 0x20)
  {
    if (rGPR[inst.RS] & 0x80000000)
    {
      rGPR[inst.RA] = 0xFFFFFFFF;
      SetCarry(1);
    }
    else
    {
      rGPR[inst.RA] = 0x00000000;
      SetCarry(0);
    }
  }
  else
  {
    int amount = rb & 0x1f;
    s32 rrs = rGPR[inst.RS];
    rGPR[inst.RA] = rrs >> amount;

    SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);
  }

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::srawix(UGeckoInstruction inst)
{
  int amount = inst.SH;

  s32 rrs = rGPR[inst.RS];
  rGPR[inst.RA] = rrs >> amount;

  SetCarry(rrs < 0 && amount > 0 && (u32(rrs) << (32 - amount)) != 0);

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::srwx(UGeckoInstruction inst)
{
  u32 amount = rGPR[inst.RB];
  rGPR[inst.RA] = (amount & 0x20) ? 0 : (rGPR[inst.RS] >> (amount & 0x1f));

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::tw(UGeckoInstruction inst)
{
  s32 a = rGPR[inst.RA];
  s32 b = rGPR[inst.RB];
  s32 TO = inst.TO;

  DEBUG_LOG(POWERPC, "tw rA %0x rB %0x TO %0x", a, b, TO);

  if (((a < b) && (TO & 0x10)) || ((a > b) && (TO & 0x08)) || ((a == b) && (TO & 0x04)) ||
      (((u32)a < (u32)b) && (TO & 0x02)) || (((u32)a > (u32)b) && (TO & 0x01)))
  {
    PowerPC::ppcState.Exceptions |= EXCEPTION_PROGRAM;
    PowerPC::CheckExceptions();
    m_end_block = true;  // Dunno about this
  }
}

void Interpreter::xorx(UGeckoInstruction inst)
{
  rGPR[inst.RA] = rGPR[inst.RS] ^ rGPR[inst.RB];

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RA]);
}

void Interpreter::addx(UGeckoInstruction inst)
{
  rGPR[inst.RD] = rGPR[inst.RA] + rGPR[inst.RB];

  if (inst.OE)
    PanicAlert("OE: addx");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::addcx(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  rGPR[inst.RD] = a + b;
  SetCarry(Helper_Carry(a, b));

  if (inst.OE)
    PanicAlert("OE: addcx");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::addex(UGeckoInstruction inst)
{
  int carry = GetCarry();
  int a = rGPR[inst.RA];
  int b = rGPR[inst.RB];
  rGPR[inst.RD] = a + b + carry;
  SetCarry(Helper_Carry(a, b) || (carry != 0 && Helper_Carry(a + b, carry)));

  if (inst.OE)
    PanicAlert("OE: addex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::addmex(UGeckoInstruction inst)
{
  int carry = GetCarry();
  int a = rGPR[inst.RA];
  rGPR[inst.RD] = a + carry - 1;
  SetCarry(Helper_Carry(a, carry - 1));

  if (inst.OE)
    PanicAlert("OE: addmex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::addzex(UGeckoInstruction inst)
{
  int carry = GetCarry();
  int a = rGPR[inst.RA];
  rGPR[inst.RD] = a + carry;
  SetCarry(Helper_Carry(a, carry));

  if (inst.OE)
    PanicAlert("OE: addzex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::divwx(UGeckoInstruction inst)
{
  s32 a = rGPR[inst.RA];
  s32 b = rGPR[inst.RB];

  if (b == 0 || ((u32)a == 0x80000000 && b == -1))
  {
    if (inst.OE)
    {
      // should set OV
      PanicAlert("OE: divwx");
    }

    if (((u32)a & 0x80000000) && b == 0)
      rGPR[inst.RD] = UINT32_MAX;
    else
      rGPR[inst.RD] = 0;
  }
  else
  {
    rGPR[inst.RD] = (u32)(a / b);
  }

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::divwux(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];

  if (b == 0)
  {
    if (inst.OE)
    {
      // should set OV
      PanicAlert("OE: divwux");
    }

    rGPR[inst.RD] = 0;
  }
  else
  {
    rGPR[inst.RD] = a / b;
  }

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::mulhwx(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];

  // This can be done better. Not in plain C/C++ though.
  u32 d = (u32)((u64)(((s64)(s32)a * (s64)(s32)b)) >> 32);

  rGPR[inst.RD] = d;

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::mulhwux(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  u32 d = (u32)(((u64)a * (u64)b) >> 32);
  rGPR[inst.RD] = d;

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::mullwx(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  u32 d = (u32)((s32)a * (s32)b);
  rGPR[inst.RD] = d;

  if (inst.OE)
    PanicAlert("OE: mullwx");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::negx(UGeckoInstruction inst)
{
  rGPR[inst.RD] = (~rGPR[inst.RA]) + 1;

  if (rGPR[inst.RD] == 0x80000000)
  {
    if (inst.OE)
      PanicAlert("OE: negx");
  }

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::subfx(UGeckoInstruction inst)
{
  rGPR[inst.RD] = rGPR[inst.RB] - rGPR[inst.RA];

  if (inst.OE)
    PanicAlert("OE: subfx");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::subfcx(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  rGPR[inst.RD] = b - a;
  SetCarry(a == 0 || Helper_Carry(b, 0 - a));

  if (inst.OE)
    PanicAlert("OE: subfcx");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

void Interpreter::subfex(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  u32 b = rGPR[inst.RB];
  int carry = GetCarry();
  rGPR[inst.RD] = (~a) + b + carry;
  SetCarry(Helper_Carry(~a, b) || Helper_Carry((~a) + b, carry));

  if (inst.OE)
    PanicAlert("OE: subfex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

// sub from minus one
void Interpreter::subfmex(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  int carry = GetCarry();
  rGPR[inst.RD] = (~a) + carry - 1;
  SetCarry(Helper_Carry(~a, carry - 1));

  if (inst.OE)
    PanicAlert("OE: subfmex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}

// sub from zero
void Interpreter::subfzex(UGeckoInstruction inst)
{
  u32 a = rGPR[inst.RA];
  int carry = GetCarry();
  rGPR[inst.RD] = (~a) + carry;
  SetCarry(Helper_Carry(~a, carry));

  if (inst.OE)
    PanicAlert("OE: subfzex");

  if (inst.Rc)
    Helper_UpdateCR0(rGPR[inst.RD]);
}
