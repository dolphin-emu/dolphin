// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/ConstantPropagation.h"

#include <bit>

#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/PPCTables.h"

namespace JitCommon
{
static constexpr u32 BitOR(u32 a, u32 b)
{
  return a | b;
}

static constexpr u32 BitAND(u32 a, u32 b)
{
  return a & b;
}

static constexpr u32 BitXOR(u32 a, u32 b)
{
  return a ^ b;
}

ConstantPropagationResult ConstantPropagation::EvaluateInstruction(UGeckoInstruction inst,
                                                                   u64 flags) const
{
  switch (inst.OPCD)
  {
  case 7:  // mulli
    return EvaluateMulImm(inst);
  case 8:  // subfic
    return EvaluateSubImmCarry(inst);
  case 12:  // addic
  case 13:  // addic.
    return EvaluateAddImmCarry(inst);
  case 14:  // addi
  case 15:  // addis
    return EvaluateAddImm(inst);
  case 20:  // rlwimix
    return EvaluateRlwimix(inst);
  case 21:  // rlwinmx
    return EvaluateRlwinmxRlwnmx(inst, inst.SH);
  case 23:  // rlwnmx
    if (HasGPR(inst.RB))
      return EvaluateRlwinmxRlwnmx(inst, GetGPR(inst.RB) & 0x1F);
    else
      return {};
  case 24:  // ori
  case 25:  // oris
    return EvaluateBitwiseImm(inst, BitOR);
  case 26:  // xori
  case 27:  // xoris
    return EvaluateBitwiseImm(inst, BitXOR);
  case 28:  // andi
  case 29:  // andis
    return EvaluateBitwiseImm(inst, BitAND);
  case 31:
    return EvaluateTable31(inst, flags);
  default:
    return {};
  }
}

ConstantPropagationResult ConstantPropagation::EvaluateMulImm(UGeckoInstruction inst) const
{
  if (inst.SIMM_16 == 0)
    return ConstantPropagationResult(inst.RD, 0);

  if (!HasGPR(inst.RA))
    return {};

  return ConstantPropagationResult(inst.RD, m_gpr_values[inst.RA] * inst.SIMM_16);
}

ConstantPropagationResult ConstantPropagation::EvaluateSubImmCarry(UGeckoInstruction inst) const
{
  if (!HasGPR(inst.RA))
    return {};

  const u32 a = GetGPR(inst.RA);
  const u32 imm = s32(inst.SIMM_16);

  ConstantPropagationResult result(inst.RD, imm - a);
  result.carry = imm >= a;
  return result;
}

ConstantPropagationResult ConstantPropagation::EvaluateAddImm(UGeckoInstruction inst) const
{
  const s32 immediate = inst.OPCD & 1 ? inst.SIMM_16 << 16 : inst.SIMM_16;

  if (inst.RA == 0)
    return ConstantPropagationResult(inst.RD, immediate);

  if (!HasGPR(inst.RA))
    return {};

  return ConstantPropagationResult(inst.RD, m_gpr_values[inst.RA] + immediate);
}

ConstantPropagationResult ConstantPropagation::EvaluateAddImmCarry(UGeckoInstruction inst) const
{
  if (!HasGPR(inst.RA))
    return {};

  const u32 a = m_gpr_values[inst.RA];
  const bool rc = inst.OPCD & 1;

  ConstantPropagationResult result(inst.RD, a + inst.SIMM_16, rc);
  result.carry = Interpreter::Helper_Carry(a, inst.SIMM_16);
  return result;
}

ConstantPropagationResult ConstantPropagation::EvaluateRlwimix(UGeckoInstruction inst) const
{
  if (!HasGPR(inst.RS))
    return {};

  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  if (mask == 0xFFFFFFFF)
    return ConstantPropagationResult(inst.RA, std::rotl(GetGPR(inst.RS), inst.SH), inst.Rc);

  if (!HasGPR(inst.RA))
    return {};

  return ConstantPropagationResult(
      inst.RA, (GetGPR(inst.RA) & ~mask) | (std::rotl(GetGPR(inst.RS), inst.SH) & mask), inst.Rc);
}

ConstantPropagationResult ConstantPropagation::EvaluateRlwinmxRlwnmx(UGeckoInstruction inst,
                                                                     u32 shift) const
{
  if (!HasGPR(inst.RS))
    return {};

  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  return ConstantPropagationResult(inst.RA, std::rotl(GetGPR(inst.RS), shift) & mask, inst.Rc);
}

ConstantPropagationResult ConstantPropagation::EvaluateBitwiseImm(UGeckoInstruction inst,
                                                                  u32 (*do_op)(u32, u32)) const
{
  const bool is_and = do_op == &BitAND;
  const u32 immediate = inst.OPCD & 1 ? inst.UIMM << 16 : inst.UIMM;

  if (inst.UIMM == 0 && !is_and && inst.RA == inst.RS)
    return DO_NOTHING;

  if (!HasGPR(inst.RS))
    return {};

  return ConstantPropagationResult(inst.RA, do_op(m_gpr_values[inst.RS], immediate), is_and);
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31(UGeckoInstruction inst,
                                                               u64 flags) const
{
  if (flags & FL_IN_B)
  {
    if (flags & FL_OUT_D)
    {
      // input a, b -> output d
      return EvaluateTable31AB(inst, flags);
    }
    else
    {
      // input s, b -> output a
      return EvaluateTable31SB(inst);
    }
  }
  else
  {
    switch (inst.SUBOP10)
    {
    case 104:  // negx
    case 616:  // negox
      // input a -> output d
      return EvaluateTable31Negx(inst, flags);
    default:
      // input s -> output a
      return EvaluateTable31S(inst);
    }
  }
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31Negx(UGeckoInstruction inst,
                                                                   u64 flags) const
{
  if (!HasGPR(inst.RA))
    return {};

  const s64 out = -s64(s32(GetGPR(inst.RA)));

  ConstantPropagationResult result(inst.RD, u32(out), inst.Rc);
  if (flags & FL_SET_OE)
    result.overflow = (out != s64(s32(out)));
  return result;
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31S(UGeckoInstruction inst) const
{
  if (!HasGPR(inst.RS))
    return {};

  std::optional<bool> carry;
  u32 a;
  const u32 s = GetGPR(inst.RS);

  switch (inst.SUBOP10)
  {
  case 26:  // cntlzwx
    a = std::countl_zero(s);
    break;
  case 824:  // srawix
    a = s32(s) >> inst.SH;
    carry = inst.SH != 0 && s32(s) < 0 && (s << (32 - inst.SH));
    break;
  case 922:  // extshx
    a = s32(s16(s));
    break;
  case 954:  // extsbx
    a = s32(s8(s));
    break;
  default:
    return {};
  }

  ConstantPropagationResult result(ConstantPropagationResult(inst.RA, a, inst.Rc));
  result.carry = carry;
  return result;
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31AB(UGeckoInstruction inst,
                                                                 u64 flags) const
{
  const bool has_a = HasGPR(inst.RA);
  const bool has_b = HasGPR(inst.RB);
  if (!has_a || !has_b)
  {
    if (has_a)
      return EvaluateTable31ABOneRegisterKnown(inst, flags, GetGPR(inst.RA), false);
    else if (has_b)
      return EvaluateTable31ABOneRegisterKnown(inst, flags, GetGPR(inst.RB), true);
    else if (inst.RA == inst.RB)
      return EvaluateTable31ABIdenticalRegisters(inst, flags);
    else
      return {};
  }

  u64 d;
  s64 d_overflow;
  const u32 a = GetGPR(inst.RA);
  const u32 b = GetGPR(inst.RB);

  switch (inst.SUBOP10)
  {
  case 8:    // subfcx
  case 40:   // subfx
  case 520:  // subfcox
  case 552:  // subfox
    d = u64(u32(~a)) + u64(b) + 1;
    d_overflow = s64(s32(b)) - s64(s32(a));
    break;
  case 10:   // addcx
  case 522:  // addcox
  case 266:  // addx
  case 778:  // addox
    d = u64(a) + u64(b);
    d_overflow = s64(s32(a)) + s64(s32(b));
    break;
  case 11:  // mulhwux
    d = d_overflow = (u64(a) * u64(b)) >> 32;
    break;
  case 75:  // mulhwx
    d = d_overflow = u64(s64(s32(a)) * s64(s32(b))) >> 32;
    break;
  case 235:  // mullwx
  case 747:  // mullwox
    d = d_overflow = s64(s32(a)) * s64(s32(b));
    break;
  case 459:  // divwux
  case 971:  // divwuox
    d = d_overflow = b == 0 ? 0x1'0000'0000 : u64(a / b);
    break;
  case 491:   // divwx
  case 1003:  // divwox
    d = d_overflow = b == 0 || (a == 0x80000000 && b == 0xFFFFFFFF) ?
                         (s32(a) < 0 ? 0xFFFFFFFF : 0x1'0000'0000) :
                         s32(a) / s32(b);
    break;
  default:
    return {};
  }

  ConstantPropagationResult result(inst.RD, u32(d), inst.Rc);
  if (flags & FL_SET_CA)
    result.carry = (d >> 32 != 0);
  if (flags & FL_SET_OE)
    result.overflow = (d_overflow != s64(s32(d_overflow)));
  return result;
}

ConstantPropagationResult
ConstantPropagation::EvaluateTable31ABOneRegisterKnown(UGeckoInstruction inst, u64 flags, u32 value,
                                                       bool known_reg_is_b) const
{
  switch (inst.SUBOP10)
  {
  case 11:   // mulhwux
  case 75:   // mulhwx
  case 235:  // mullwx
  case 747:  // mullwox
    if (value == 0)
    {
      ConstantPropagationResult result(inst.RD, 0, inst.Rc);
      if (flags & FL_SET_OE)
        result.overflow = false;
      return result;
    }
    break;
  case 459:  // divwux
  case 971:  // divwuox
    if (known_reg_is_b && value == 0)
    {
      ConstantPropagationResult result(inst.RD, 0, inst.Rc);
      if (flags & FL_SET_OE)
        result.overflow = true;
      return result;
    }
    [[fallthrough]];
  case 491:   // divwx
  case 1003:  // divwox
    if (!known_reg_is_b && value == 0 && !(flags & FL_SET_OE))
    {
      return ConstantPropagationResult(inst.RD, 0, inst.Rc);
    }
    break;
  }

  return {};
}

ConstantPropagationResult
ConstantPropagation::EvaluateTable31ABIdenticalRegisters(UGeckoInstruction inst, u64 flags) const
{
  switch (inst.SUBOP10)
  {
  case 8:    // subfcx
  case 40:   // subfx
  case 520:  // subfcox
  case 552:  // subfox
  {
    ConstantPropagationResult result(inst.RD, 0, inst.Rc);
    if (flags & FL_SET_CA)
      result.carry = true;
    if (flags & FL_SET_OE)
      result.overflow = false;
    return result;
  }
  default:
    return {};
  }
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31SB(UGeckoInstruction inst) const
{
  const bool has_s = HasGPR(inst.RS);
  const bool has_b = HasGPR(inst.RB);
  if (!has_s || !has_b)
  {
    if (has_s)
      return EvaluateTable31SBOneRegisterKnown(inst, GetGPR(inst.RS), false);
    else if (has_b)
      return EvaluateTable31SBOneRegisterKnown(inst, GetGPR(inst.RB), true);
    else if (inst.RS == inst.RB)
      return EvaluateTable31SBIdenticalRegisters(inst);
    else
      return {};
  }

  u32 a;
  const u32 s = GetGPR(inst.RS);
  const u32 b = GetGPR(inst.RB);

  switch (inst.SUBOP10)
  {
  case 24:  // slwx
    a = u32(u64(s) << b);
    break;
  case 28:  // andx
    a = s & b;
    break;
  case 60:  // andcx
    a = s & (~b);
    break;
  case 124:  // norx
    a = ~(s | b);
    break;
  case 284:  // eqvx
    a = ~(s ^ b);
    break;
  case 316:  // xorx
    a = s ^ b;
    break;
  case 412:  // orcx
    a = s | (~b);
    break;
  case 444:  // orx
    a = s | b;
    break;
  case 476:  // nandx
    a = ~(s & b);
    break;
  case 536:  // srwx
    a = u32(u64(s) >> b);
    break;
  case 792:  // srawx
  {
    const u64 temp = (s64(s32(s)) << 32) >> b;
    a = u32(temp >> 32);

    ConstantPropagationResult result(inst.RA, a, inst.Rc);
    result.carry = (temp & a) != 0;
    return result;
  }
  default:
    return {};
  }

  return ConstantPropagationResult(inst.RA, a, inst.Rc);
}

ConstantPropagationResult
ConstantPropagation::EvaluateTable31SBOneRegisterKnown(UGeckoInstruction inst, u32 value,
                                                       bool known_reg_is_b) const
{
  u32 a;

  switch (inst.SUBOP10)
  {
  case 24:   // slwx
  case 536:  // srwx
    if (!known_reg_is_b && value == 0)
      a = 0;
    else if (known_reg_is_b && (value & 0x20))
      a = 0;
    else
      return {};
    break;
  case 60:  // andcx
    if (known_reg_is_b)
      value = ~value;
    [[fallthrough]];
  case 28:  // andx
    if (value == 0)
      a = 0;
    else
      return {};
    break;
  case 124:  // norx
    if (value == 0xFFFFFFFF)
      a = 0;
    else
      return {};
    break;
  case 412:  // orcx
    if (known_reg_is_b)
      value = ~value;
    [[fallthrough]];
  case 444:  // orx
    if (value == 0xFFFFFFFF)
      a = 0xFFFFFFFF;
    else
      return {};
    break;
  case 476:  // nandx
    if (value == 0)
      a = 0xFFFFFFFF;
    else
      return {};
    break;
  case 792:  // srawx
    if (!known_reg_is_b && value == 0)
    {
      ConstantPropagationResult result(inst.RA, 0, inst.Rc);
      result.carry = false;
      return result;
    }
    else
    {
      return {};
    }
    break;
  default:
    return {};
  }

  return ConstantPropagationResult(inst.RA, a, inst.Rc);
}

ConstantPropagationResult
ConstantPropagation::EvaluateTable31SBIdenticalRegisters(UGeckoInstruction inst) const
{
  u32 a;

  switch (inst.SUBOP10)
  {
  case 60:  // andcx
    a = 0;
    break;
  case 284:  // eqvx
    a = 0xFFFFFFFF;
    break;
  case 316:  // xorx
    a = 0;
    break;
  case 412:  // orcx
    a = 0xFFFFFFFF;
    break;
  default:
    return {};
  }

  return ConstantPropagationResult(inst.RA, a, inst.Rc);
}

void ConstantPropagation::Apply(ConstantPropagationResult result, BitSet32 gprs_out)
{
  if (!result.instruction_fully_executed)
    m_gpr_values_known &= ~gprs_out;

  if (result.gpr >= 0)
    SetGPR(result.gpr, result.gpr_value);
}

}  // namespace JitCommon
