// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/ConstantPropagation.h"

#include <bit>

#include "Core/PowerPC/Gekko.h"
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
  case 14:  // addi
  case 15:  // addis
    return EvaluateAddImm(inst);
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

ConstantPropagationResult ConstantPropagation::EvaluateAddImm(UGeckoInstruction inst) const
{
  const s32 immediate = inst.OPCD & 1 ? inst.SIMM_16 << 16 : inst.SIMM_16;

  if (inst.RA == 0)
    return ConstantPropagationResult(inst.RD, immediate);

  if (!HasGPR(inst.RA))
    return {};

  return ConstantPropagationResult(inst.RD, m_gpr_values[inst.RA] + immediate);
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

  u32 a;
  const u32 s = GetGPR(inst.RS);

  switch (inst.SUBOP10)
  {
  case 26:  // cntlzwx
    a = std::countl_zero(s);
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

  return ConstantPropagationResult(inst.RA, a, inst.Rc);
}

ConstantPropagationResult ConstantPropagation::EvaluateTable31AB(UGeckoInstruction inst,
                                                                 u64 flags) const
{
  if (!HasGPR(inst.RA, inst.RB))
    return {};

  u64 d;
  s64 d_overflow;
  const u32 a = GetGPR(inst.RA);
  const u32 b = GetGPR(inst.RB);

  switch (inst.SUBOP10)
  {
  case 10:   // addcx
  case 522:  // addcox
  case 266:  // addx
  case 778:  // addox
    d = u64(a) + u64(b);
    d_overflow = s64(s32(a)) + s64(s32(b));
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

void ConstantPropagation::Apply(ConstantPropagationResult result)
{
  if (result.gpr >= 0)
    SetGPR(result.gpr, result.gpr_value);
}

}  // namespace JitCommon
