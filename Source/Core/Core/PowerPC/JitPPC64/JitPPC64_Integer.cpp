// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Integer instructions. Guest values are 32-bit, in the low halves of 64-bit host registers whose
// upper halves are undefined, so the host's 32-bit ("word") instructions do most of the work.
// Flags are the exception: the host computes XER[CA] and CR0 from 64-bit results, so carries and
// CR0 are computed explicitly for 32 bits.

#include "Core/PowerPC/JitPPC64/Jit.h"

#include <vector>

#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "Core/PowerPC/ConditionRegister.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace PPC64Gen;

void JitPPC64::ComputeRC0(GPR value)
{
  // As Interpreter::Helper_UpdateCR0: the sign-extended value, with SO from XER.
  CompareForNextBranch(0, value, 0, true);
  const GPR cr = gpr.CRW(0);
  EXTSW(cr, value);
  SetCRFieldSO(cr);
}

void JitPPC64::SetCRFieldSO(GPR cr)
{
  // Bit 59 is SO, which a negative value has set, so it gets XER[SO] either way. With SO set, a
  // zero value gets bit 63 too, so that it doesn't become positive and so GT, as the interpreter
  // does: rare, in far code.
  const auto so = gpr.GetScopedReg();
  LBZ(so, PPC_REG, PPCSTATE_OFF(xer_so_ov));
  RLWINM(so, so, 31, 31, 31, true);
  RLDIMI(cr, so, PowerPC::CR_EMU_SO_BIT, 63 - PowerPC::CR_EMU_SO_BIT);
  const FixupBranch so_set = BCFar(CCFlags::NE, CRF::CR0);
  const u8* done = GetCodePtr();

  SwitchToFarCode();
  SetJumpTarget(so_set);
  CMPWI(CRF::CR0, cr, 0);
  const FixupBranch not_zero = BC(CCFlags::NE, CRF::CR0);
  LI(so, 1);
  RLDIMI(cr, so, 63, 0);
  SetJumpTarget(not_zero);
  B(done);
  SwitchToNearCode();
}

void JitPPC64::SetCRFieldFromCompare(u32 field, GPR a, GPR b, bool is_signed)
{
  // As Helper_IntCompare: the difference of the sign- or zero-extended values, which is negative,
  // zero or positive as the comparison, and has bit 62 set if negative.
  CompareForNextBranch(field, a, b, is_signed);
  const GPR cr = gpr.CRW(field);
  const auto tmp = gpr.GetScopedReg();
  is_signed ? EXTSW(cr, a) : CLRLDI(cr, a, 32);
  is_signed ? EXTSW(tmp, b) : CLRLDI(tmp, b, 32);
  SUBF(cr, tmp, cr);
  SetCRFieldSO(cr);
}

void JitPPC64::SetCRFieldFromCompare(u32 field, GPR a, s32 imm, bool is_signed)
{
  CompareForNextBranch(field, a, imm, is_signed);
  const GPR cr = gpr.CRW(field);
  is_signed ? EXTSW(cr, a) : CLRLDI(cr, a, 32);
  if (imm >= -32767 && imm <= 32768)
  {
    if (imm != 0)
      ADDI(cr, cr, static_cast<s16>(-imm));
  }
  else
  {
    const auto tmp = gpr.GetScopedReg();
    MOVI2R(tmp, static_cast<u64>(static_cast<s64>(imm)));
    SUBF(cr, tmp, cr);
  }
  SetCRFieldSO(cr);
}

void JitPPC64::GetCRField(GPR out, u32 field)
{
  // As ConditionRegister::GetField.
  const GPR cr = gpr.CR(field);
  const auto tmp = gpr.GetScopedReg();
  // LT and SO: bits 62 and 59 to bits 3 and 0.
  SRDI(out, cr, PowerPC::CR_EMU_SO_BIT);
  ANDI_(out, out, PowerPC::CR_LT | PowerPC::CR_SO);
  // EQ: the low 32 bits are zero.
  CNTLZW(tmp, cr);
  RLWINM(tmp, tmp, 28, 30, 30);  // (cntlzw >> 5) << 1
  OR(out, out, tmp);
  // GT: the value is positive.
  if (m_isa_3_1)
  {
    CMPDI(CRF::CR1, cr, 0);
    SETBC(tmp, CRBit(CRF::CR1, CCFlags::GT));
  }
  else
  {
    NEG(tmp, cr);
    ANDC(tmp, tmp, cr);  // Negative only if cr > 0
    SRDI(tmp, tmp, 63);
  }
  RLDIMI(out, tmp, PowerPC::CR_GT_BIT, 63 - PowerPC::CR_GT_BIT);
}

void JitPPC64::SetCRField(u32 field, GPR value)
{
  // As ConditionRegister::SetField: look the field's value up in its table.
  const GPR cr = gpr.CRW(field);
  const auto table = gpr.GetScopedReg();
  const auto offset = gpr.GetScopedReg();
  MOVP2R(table, PowerPC::ConditionRegister::s_crTable.data());
  RLDIC(offset, value, 3, 64 - 7);  // (value & 0xF) * 8
  LDX(cr, table, offset);
}

bool JitPPC64::NextBranchTestsCRField(u32 field) const
{
  // As Jit64::CheckMergedBranch, for bcx and bclrx, but the branch is compiled as usual, except
  // for its test. SO isn't in the host's comparison, and bcctrx with a condition is interpreted.
  if (js.instructionsLeft == 0 || js.op[1].skip)
    return false;
  const UGeckoInstruction next = js.op[1].inst;
  return (next.OPCD == 16 || (next.OPCD == 19 && next.SUBOP10 == 16)) &&
         (next.BO & BO_DONT_CHECK_CONDITION) == 0 && next.BI >> 2 == field &&
         3 - (next.BI & 3) != PowerPC::CR_SO_BIT;
}

void JitPPC64::CompareForNextBranch(u32 field, GPR a, GPR b, bool is_signed)
{
  if (!NextBranchTestsCRField(field))
    return;
  is_signed ? CMPW(CRF::CR7, a, b) : CMPLW(CRF::CR7, a, b);
  m_next_cr7_field = field;
}

void JitPPC64::CompareForNextBranch(u32 field, GPR a, s32 imm, bool is_signed)
{
  if (!NextBranchTestsCRField(field))
    return;
  // Only immediates of compare instructions, and 0.
  is_signed ? CMPWI(CRF::CR7, a, static_cast<s16>(imm)) :
              CMPLWI(CRF::CR7, a, static_cast<u16>(imm));
  m_next_cr7_field = field;
}

FixupBranch JitPPC64::JumpIfCRFieldBit(u32 field, u32 bit, bool jump_if_set)
{
  // The CR value is negative, zero or positive as the comparison it was set from, as is the host's
  // comparison: SO, and bit 63 set for a zero value with SO set, change neither.
  if (m_cr7_field == field && bit != PowerPC::CR_SO_BIT)
  {
    const CCFlags cond = bit == PowerPC::CR_LT_BIT ? CCFlags::LT :
                         bit == PowerPC::CR_GT_BIT ? CCFlags::GT :
                                                     CCFlags::EQ;
    return BC(jump_if_set ? cond : Invert(cond), CRF::CR7);
  }

  // R0, which isn't allocated, takes the tested bit.
  const GPR cr = gpr.CR(field);
  switch (bit)
  {
  case PowerPC::CR_SO_BIT:  // bit 59 set
    RLDICL(GPR::R0, cr, 64 - PowerPC::CR_EMU_SO_BIT, 63, true);
    return BC(jump_if_set ? CCFlags::NE : CCFlags::EQ, CRF::CR0);
  case PowerPC::CR_EQ_BIT:  // bits 31-0 == 0
    CMPWI(CRF::CR0, cr, 0);
    return BC(jump_if_set ? CCFlags::EQ : CCFlags::NE, CRF::CR0);
  case PowerPC::CR_GT_BIT:  // val > 0
    CMPDI(CRF::CR0, cr, 0);
    return BC(jump_if_set ? CCFlags::GT : CCFlags::LE, CRF::CR0);
  case PowerPC::CR_LT_BIT:  // bit 62 set
  default:
    RLDICL(GPR::R0, cr, 64 - PowerPC::CR_EMU_LT_BIT, 63, true);
    return BC(jump_if_set ? CCFlags::NE : CCFlags::EQ, CRF::CR0);
  }
}

void JitPPC64::AddWithCarry(GPR d, GPR x, GPR y, CarryIn carry_in)
{
  // Add the zero-extended values in 64 bits, where the carry out of 32 bits is bit 32.
  const auto sum = gpr.GetScopedReg();
  const auto tmp = gpr.GetScopedReg();
  CLRLDI(sum, x, 32);
  CLRLDI(tmp, y, 32);
  ADD(sum, sum, tmp);
  if (carry_in == CarryIn::One)
  {
    ADDI(sum, sum, 1);
  }
  else if (carry_in == CarryIn::XER)
  {
    LBZ(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
    ADD(sum, sum, tmp);
  }
  SRDI(tmp, sum, 32);
  STB(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
  MR(d, sum);
}

void JitPPC64::ComputeOverflowOf64(GPR overflow, GPR value)
{
  // 1 if the 64-bit value doesn't fit in 32 bits, signed.
  EXTSW(overflow, value);
  XOR(overflow, overflow, value);
  CNTLZD(overflow, overflow);
  RLDICL(overflow, overflow, 64 - 6, 63);  // 1 if it fits
  XORI(overflow, overflow, 1);
}

void JitPPC64::ComputeAddOverflow(GPR overflow, GPR x, GPR y, CarryIn carry_in)
{
  // A signed 32-bit sum overflows when the sum of the sign-extended values doesn't fit. This reads
  // XER[CA], so it comes before the sum that writes it.
  const auto sum = gpr.GetScopedReg();
  EXTSW(sum, x);
  EXTSW(overflow, y);
  ADD(sum, sum, overflow);
  if (carry_in == CarryIn::One)
  {
    ADDI(sum, sum, 1);
  }
  else if (carry_in == CarryIn::XER)
  {
    LBZ(overflow, PPC_REG, PPCSTATE_OFF(xer_ca));
    ADD(sum, sum, overflow);
  }
  ComputeOverflowOf64(overflow, sum);
}

void JitPPC64::StoreOverflow(GPR overflow)
{
  // As SetXER_OV: OV is the overflow, and SO accumulates it. Clobbers overflow.
  const auto so_ov = gpr.GetScopedReg();
  LBZ(so_ov, PPC_REG, PPCSTATE_OFF(xer_so_ov));
  RLWINM(so_ov, so_ov, 0, 30, 30);  // SO
  MULLI(overflow, overflow, 3);
  OR(so_ov, so_ov, overflow);
  STB(so_ov, PPC_REG, PPCSTATE_OFF(xer_so_ov));
}

void JitPPC64::StoreHostCarry()
{
  // The host's word shifts compute CA the way the guest's do.
  const auto tmp = gpr.GetScopedReg();
  MFXER(tmp);
  RLWINM(tmp, tmp, 3, 31, 31);  // XER[CA] is 0x20000000
  STB(tmp, PPC_REG, PPCSTATE_OFF(xer_ca));
}

void JitPPC64::addix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const u32 imm = inst.OPCD == 15 ? static_cast<u32>(inst.SIMM_16) << 16 :
                                    static_cast<u32>(static_cast<s32>(inst.SIMM_16));
  if (inst.RA == 0)
  {
    // li or lis
    MOVI2R(gpr.RW(inst.RD), imm);
    return;
  }

  const GPR a = gpr.R(inst.RA);
  const GPR d = gpr.RW(inst.RD);
  if (inst.OPCD == 15)
    ADDIS(d, a, inst.SIMM_16);
  else
    ADDI(d, a, inst.SIMM_16);
}

void JitPPC64::addic(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const auto imm = gpr.GetScopedReg();
  MOVI2R(imm, static_cast<u32>(static_cast<s32>(inst.SIMM_16)));
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, a, imm, CarryIn::Zero);
  // addic. is opcode 13
  if (inst.OPCD == 13)
    ComputeRC0(d);
}

void JitPPC64::subfic(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // d = imm - a = ~a + imm + 1
  const auto not_a = gpr.GetScopedReg();
  NOT(not_a, gpr.R(inst.RA));
  const auto imm = gpr.GetScopedReg();
  MOVI2R(imm, static_cast<u32>(static_cast<s32>(inst.SIMM_16)));
  AddWithCarry(gpr.RW(inst.RD), not_a, imm, CarryIn::One);
}

void JitPPC64::mulli(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  MULLI(gpr.RW(inst.RD), a, inst.SIMM_16);
}

void JitPPC64::arith_imm(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA);
  switch (inst.OPCD)
  {
  case 24:  // ori
    ORI(a, s, inst.UIMM);
    break;
  case 25:  // oris
    ORIS(a, s, inst.UIMM);
    break;
  case 26:  // xori
    XORI(a, s, inst.UIMM);
    break;
  case 27:  // xoris
    XORIS(a, s, inst.UIMM);
    break;
  case 28:  // andi.
    ANDI_(a, s, inst.UIMM);
    ComputeRC0(a);
    break;
  case 29:  // andis.
    ANDIS_(a, s, inst.UIMM);
    ComputeRC0(a);
    break;
  }
}

void JitPPC64::boolX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR b = gpr.R(inst.RB);
  const GPR a = gpr.RW(inst.RA);
  switch (inst.SUBOP10)
  {
  case 28:  // and
    AND(a, s, b);
    break;
  case 60:  // andc
    ANDC(a, s, b);
    break;
  case 444:  // or
    OR(a, s, b);
    break;
  case 412:  // orc
    ORC(a, s, b);
    break;
  case 316:  // xor
    XOR(a, s, b);
    break;
  case 476:  // nand
    NAND(a, s, b);
    break;
  case 124:  // nor
    NOR(a, s, b);
    break;
  case 284:  // eqv
    EQV(a, s, b);
    break;
  }
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::extsXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA);
  if (inst.SUBOP10 == 954)
    EXTSB(a, s);
  else
    EXTSH(a, s);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::cntlzwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA);
  CNTLZW(a, s);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::addx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, a, b, CarryIn::Zero);
  const GPR d = gpr.RW(inst.RD);
  ADD(d, a, b);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::addcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, a, b, CarryIn::Zero);
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, a, b, CarryIn::Zero);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::addex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // d = a + b + CA, or a - 1 + CA for addme
  const GPR a = gpr.R(inst.RA);
  const auto minus_one = gpr.GetScopedReg();
  GPR b = minus_one;
  if ((inst.SUBOP10 & 0x1FF) == 234)  // addme, with or without OE
    LI(minus_one, -1);
  else
    b = gpr.R(inst.RB);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, a, b, CarryIn::XER);
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, a, b, CarryIn::XER);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::addzex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const auto zero = gpr.GetScopedReg();
  LI(zero, 0);
  const GPR a = gpr.R(inst.RA);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, a, zero, CarryIn::XER);
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, a, zero, CarryIn::XER);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::subfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // d = b - a = ~a + b + 1
  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const bool carry = (inst.SUBOP10 & 0x1FF) == 8;  // subfc, with or without OE
  const auto not_a = gpr.GetScopedReg();
  if (carry || inst.OE)
    NOT(not_a, a);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, not_a, b, CarryIn::One);
  const GPR d = gpr.RW(inst.RD);
  if (carry)
    AddWithCarry(d, not_a, b, CarryIn::One);
  else
    SUBF(d, a, b);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::subfex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // d = ~a + b + CA, or ~a - 1 + CA for subfme
  const auto not_a = gpr.GetScopedReg();
  NOT(not_a, gpr.R(inst.RA));
  const auto minus_one = gpr.GetScopedReg();
  GPR b = minus_one;
  if ((inst.SUBOP10 & 0x1FF) == 232)  // subfme, with or without OE
    LI(minus_one, -1);
  else
    b = gpr.R(inst.RB);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, not_a, b, CarryIn::XER);
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, not_a, b, CarryIn::XER);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::subfzex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // d = ~a + CA
  const auto not_a = gpr.GetScopedReg();
  NOT(not_a, gpr.R(inst.RA));
  const auto zero = gpr.GetScopedReg();
  LI(zero, 0);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
    ComputeAddOverflow(overflow, not_a, zero, CarryIn::XER);
  const GPR d = gpr.RW(inst.RD);
  AddWithCarry(d, not_a, zero, CarryIn::XER);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::negx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
  {
    // Only -0x80000000 doesn't fit.
    const auto negated = gpr.GetScopedReg();
    EXTSW(negated, a);
    NEG(negated, negated);
    ComputeOverflowOf64(overflow, negated);
  }
  const GPR d = gpr.RW(inst.RD);
  NEG(d, a);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::mullwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const auto overflow = gpr.GetScopedReg();
  if (inst.OE)
  {
    // The host's mullw computes the 64-bit product of the words.
    const auto product = gpr.GetScopedReg();
    MULLW(product, a, b);
    ComputeOverflowOf64(overflow, product);
  }
  const GPR d = gpr.RW(inst.RD);
  MULLW(d, a, b);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::mulhwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const GPR d = gpr.RW(inst.RD);
  MULHW(d, a, b);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::mulhwux(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const GPR d = gpr.RW(inst.RD);
  MULHWU(d, a, b);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::divwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // The host leaves the result undefined when dividing by zero, or 0x80000000 by -1. As the
  // interpreter, the result is then -1 if the dividend is negative, and 0 otherwise, and these
  // cases overflow.
  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const GPR d = gpr.RW(inst.RD);
  const auto tmp = gpr.GetScopedReg();
  const auto overflow = gpr.GetScopedReg();

  CMPWI(CRF::CR1, b, 0);
  const FixupBranch by_zero = BC(CCFlags::EQ, CRF::CR1);
  CMPWI(CRF::CR1, b, -1);
  const FixupBranch not_minus_one = BC(CCFlags::NE, CRF::CR1);
  LIS(tmp, static_cast<s16>(0x8000));
  CMPW(CRF::CR1, a, tmp);
  const FixupBranch too_large = BC(CCFlags::EQ, CRF::CR1);

  SetJumpTarget(not_minus_one);
  DIVW(d, a, b);
  if (inst.OE)
    LI(overflow, 0);
  const FixupBranch done = B();

  SetJumpTarget(by_zero);
  SetJumpTarget(too_large);
  SRAWI(d, a, 31);
  if (inst.OE)
    LI(overflow, 1);

  SetJumpTarget(done);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::divwux(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // The interpreter returns 0 when dividing by zero, where the host leaves it undefined, and it
  // overflows.
  const GPR a = gpr.R(inst.RA);
  const GPR b = gpr.R(inst.RB);
  const GPR d = gpr.RW(inst.RD);
  const auto overflow = gpr.GetScopedReg();

  CMPLWI(CRF::CR1, b, 0);
  const FixupBranch by_zero = BC(CCFlags::EQ, CRF::CR1);
  DIVWU(d, a, b);
  if (inst.OE)
    LI(overflow, 0);
  const FixupBranch done = B();
  SetJumpTarget(by_zero);
  LI(d, 0);
  if (inst.OE)
    LI(overflow, 1);
  SetJumpTarget(done);
  if (inst.OE)
    StoreOverflow(overflow);
  if (inst.Rc)
    ComputeRC0(d);
}

void JitPPC64::slwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR b = gpr.R(inst.RB);
  const GPR a = gpr.RW(inst.RA);
  SLW(a, s, b);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::srwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR b = gpr.R(inst.RB);
  const GPR a = gpr.RW(inst.RA);
  SRW(a, s, b);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::srawx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR b = gpr.R(inst.RB);
  const GPR a = gpr.RW(inst.RA);
  SRAW(a, s, b);
  StoreHostCarry();
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::srawix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA);
  SRAWI(a, s, inst.SH);
  StoreHostCarry();
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::rlwinmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA);
  RLWINM(a, s, inst.SH, inst.MB, inst.ME);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::rlwnmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR b = gpr.R(inst.RB);
  const GPR a = gpr.RW(inst.RA);
  RLWNM(a, s, b, inst.MB, inst.ME);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::rlwimix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const GPR s = gpr.R(inst.RS);
  const GPR a = gpr.RW(inst.RA, true);
  RLWIMI(a, s, inst.SH, inst.MB, inst.ME);
  if (inst.Rc)
    ComputeRC0(a);
}

void JitPPC64::cmp(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  SetCRFieldFromCompare(inst.CRFD, gpr.R(inst.RA), gpr.R(inst.RB), true);
}

void JitPPC64::cmpl(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  SetCRFieldFromCompare(inst.CRFD, gpr.R(inst.RA), gpr.R(inst.RB), false);
}

void JitPPC64::cmpi(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  SetCRFieldFromCompare(inst.CRFD, gpr.R(inst.RA), static_cast<s32>(inst.SIMM_16), true);
}

void JitPPC64::cmpli(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  SetCRFieldFromCompare(inst.CRFD, gpr.R(inst.RA), static_cast<s32>(inst.UIMM), false);
}

void JitPPC64::twx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  // As the interpreter: trap if any condition that TO selects holds, comparing rA with rB, or for
  // twi with the immediate, signed (LT, GT, EQ) and unsigned (LT, GT).
  const u32 to = inst.TO;
  std::vector<FixupBranch> traps;
  if (to == 0x1F)
  {
    traps.push_back(B());
  }
  else
  {
    const GPR a = gpr.R(inst.RA);
    if (inst.OPCD == 3)
    {
      const auto b = gpr.GetScopedReg();
      MOVI2R(b, static_cast<u32>(static_cast<s32>(inst.SIMM_16)));
      CMPW(CRF::CR0, a, b);
      CMPLW(CRF::CR1, a, b);
    }
    else
    {
      CMPW(CRF::CR0, a, gpr.R(inst.RB));
      CMPLW(CRF::CR1, a, gpr.R(inst.RB));
    }
    if (to & 0x10)
      traps.push_back(BCFar(CCFlags::LT, CRF::CR0));
    if (to & 0x08)
      traps.push_back(BCFar(CCFlags::GT, CRF::CR0));
    if (to & 0x04)
      traps.push_back(BCFar(CCFlags::EQ, CRF::CR0));
    if (to & 0x02)
      traps.push_back(BCFar(CCFlags::LT, CRF::CR1));
    if (to & 0x01)
      traps.push_back(BCFar(CCFlags::GT, CRF::CR1));
  }

  if (!traps.empty())
  {
    SwitchToFarCode();
    for (const FixupBranch& trap : traps)
      SetJumpTarget(trap);
    WriteProgramExceptionExit(static_cast<u32>(ProgramExceptionCause::Trap));
    SwitchToNearCode();
  }

  // The analyzer continues the block after a trap only with conditional continue.
  if (!analyzer.HasOption(PPCAnalyst::PPCAnalyzer::OPTION_CONDITIONAL_CONTINUE))
  {
    FlushRegisters(FlushMode::All);
    WriteExit(js.compilerPC + 4);
  }
}
