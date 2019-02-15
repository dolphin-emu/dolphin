// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::ComputeRC0(ARM64Reg reg)
{
  gpr.BindCRToRegister(0, false);
  SXTW(gpr.CR(0), reg);
}

void JitArm64::ComputeRC0(u64 imm)
{
  gpr.BindCRToRegister(0, false);
  MOVI2R(gpr.CR(0), imm);
  if (imm & 0x80000000)
    SXTW(gpr.CR(0), DecodeReg(gpr.CR(0)));
}

void JitArm64::ComputeCarry(bool Carry)
{
  js.carryFlagSet = false;

  if (!js.op->wantsCA)
    return;

  if (Carry)
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, 1);
    STRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    gpr.Unlock(WA);
    return;
  }

  STRB(INDEX_UNSIGNED, WSP, PPC_REG, PPCSTATE_OFF(xer_ca));
}

void JitArm64::ComputeCarry()
{
  js.carryFlagSet = false;

  if (!js.op->wantsCA)
    return;

  js.carryFlagSet = true;
  if (CanMergeNextInstructions(1) && js.op[1].opinfo->type == ::OpType::Integer)
  {
    return;
  }

  FlushCarry();
}

void JitArm64::FlushCarry()
{
  if (!js.carryFlagSet)
    return;

  ARM64Reg WA = gpr.GetReg();
  CSINC(WA, WSP, WSP, CC_CC);
  STRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
  gpr.Unlock(WA);

  js.carryFlagSet = false;
}

void JitArm64::reg_imm(u32 d, u32 a, u32 value, u32 (*do_op)(u32, u32),
                       void (ARM64XEmitter::*op)(ARM64Reg, ARM64Reg, u64, ARM64Reg), bool Rc)
{
  if (gpr.IsImm(a))
  {
    gpr.SetImmediate(d, do_op(gpr.GetImm(a), value));
    if (Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a);
    ARM64Reg WA = gpr.GetReg();
    (this->*op)(gpr.R(d), gpr.R(a), value, WA);
    gpr.Unlock(WA);

    if (Rc)
      ComputeRC0(gpr.R(d));
  }
}

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

void JitArm64::arith_imm(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  u32 a = inst.RA, s = inst.RS;

  switch (inst.OPCD)
  {
  case 24:  // ori
  case 25:  // oris
  {
    // check for nop
    if (a == s && inst.UIMM == 0)
    {
      // NOP
      return;
    }

    const u32 immediate = inst.OPCD == 24 ? inst.UIMM : inst.UIMM << 16;
    reg_imm(a, s, immediate, BitOR, &ARM64XEmitter::ORRI2R);
    break;
  }
  case 28:  // andi
    reg_imm(a, s, inst.UIMM, BitAND, &ARM64XEmitter::ANDI2R, true);
    break;
  case 29:  // andis
    reg_imm(a, s, inst.UIMM << 16, BitAND, &ARM64XEmitter::ANDI2R, true);
    break;
  case 26:  // xori
  case 27:  // xoris
  {
    if (a == s && inst.UIMM == 0)
    {
      // NOP
      return;
    }

    const u32 immediate = inst.OPCD == 26 ? inst.UIMM : inst.UIMM << 16;
    reg_imm(a, s, immediate, BitXOR, &ARM64XEmitter::EORI2R);
    break;
  }
  }
}

void JitArm64::addix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  u32 d = inst.RD, a = inst.RA;

  u32 imm = (u32)(s32)inst.SIMM_16;
  if (inst.OPCD == 15)
  {
    imm <<= 16;
  }

  if (a)
  {
    if (gpr.IsImm(a))
    {
      gpr.SetImmediate(d, gpr.GetImm(a) + imm);
    }
    else
    {
      gpr.BindToRegister(d, d == a);

      ARM64Reg WA = gpr.GetReg();
      ADDI2R(gpr.R(d), gpr.R(a), imm, WA);
      gpr.Unlock(WA);
    }
  }
  else
  {
    // a == 0, implies zero register
    gpr.SetImmediate(d, imm);
  }
}

void JitArm64::boolX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA, s = inst.RS, b = inst.RB;

  if (gpr.IsImm(s) && gpr.IsImm(b))
  {
    if (inst.SUBOP10 == 28)  // andx
      gpr.SetImmediate(a, (u32)gpr.GetImm(s) & (u32)gpr.GetImm(b));
    else if (inst.SUBOP10 == 476)  // nandx
      gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) & (u32)gpr.GetImm(b)));
    else if (inst.SUBOP10 == 60)  // andcx
      gpr.SetImmediate(a, (u32)gpr.GetImm(s) & (~(u32)gpr.GetImm(b)));
    else if (inst.SUBOP10 == 444)  // orx
      gpr.SetImmediate(a, (u32)gpr.GetImm(s) | (u32)gpr.GetImm(b));
    else if (inst.SUBOP10 == 124)  // norx
      gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) | (u32)gpr.GetImm(b)));
    else if (inst.SUBOP10 == 412)  // orcx
      gpr.SetImmediate(a, (u32)gpr.GetImm(s) | (~(u32)gpr.GetImm(b)));
    else if (inst.SUBOP10 == 316)  // xorx
      gpr.SetImmediate(a, (u32)gpr.GetImm(s) ^ (u32)gpr.GetImm(b));
    else if (inst.SUBOP10 == 284)  // eqvx
      gpr.SetImmediate(a, ~((u32)gpr.GetImm(s) ^ (u32)gpr.GetImm(b)));

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else if (s == b)
  {
    if ((inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 444 /* orx */))
    {
      if (a != s)
      {
        gpr.BindToRegister(a, false);
        MOV(gpr.R(a), gpr.R(s));
      }
      if (inst.Rc)
        ComputeRC0(gpr.R(a));
    }
    else if ((inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */))
    {
      gpr.BindToRegister(a, a == s);
      MVN(gpr.R(a), gpr.R(s));
      if (inst.Rc)
        ComputeRC0(gpr.R(a));
    }
    else if ((inst.SUBOP10 == 412 /* orcx */) || (inst.SUBOP10 == 284 /* eqvx */))
    {
      gpr.SetImmediate(a, 0xFFFFFFFF);
      if (inst.Rc)
        ComputeRC0(gpr.GetImm(a));
    }
    else if ((inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 316 /* xorx */))
    {
      gpr.SetImmediate(a, 0);
      if (inst.Rc)
        ComputeRC0(gpr.GetImm(a));
    }
    else
    {
      PanicAlert("WTF!");
    }
  }
  else
  {
    gpr.BindToRegister(a, (a == s) || (a == b));
    if (inst.SUBOP10 == 28)  // andx
    {
      AND(gpr.R(a), gpr.R(s), gpr.R(b));
    }
    else if (inst.SUBOP10 == 476)  // nandx
    {
      AND(gpr.R(a), gpr.R(s), gpr.R(b));
      MVN(gpr.R(a), gpr.R(a));
    }
    else if (inst.SUBOP10 == 60)  // andcx
    {
      BIC(gpr.R(a), gpr.R(s), gpr.R(b));
    }
    else if (inst.SUBOP10 == 444)  // orx
    {
      ORR(gpr.R(a), gpr.R(s), gpr.R(b));
    }
    else if (inst.SUBOP10 == 124)  // norx
    {
      ORR(gpr.R(a), gpr.R(s), gpr.R(b));
      MVN(gpr.R(a), gpr.R(a));
    }
    else if (inst.SUBOP10 == 412)  // orcx
    {
      ORN(gpr.R(a), gpr.R(s), gpr.R(b));
    }
    else if (inst.SUBOP10 == 316)  // xorx
    {
      EOR(gpr.R(a), gpr.R(s), gpr.R(b));
    }
    else if (inst.SUBOP10 == 284)  // eqvx
    {
      EON(gpr.R(a), gpr.R(b), gpr.R(s));
    }
    else
    {
      PanicAlert("WTF!");
    }
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::addx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    s32 i = (s32)gpr.GetImm(a), j = (s32)gpr.GetImm(b);
    gpr.SetImmediate(d, i + j);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else if (gpr.IsImm(a) || gpr.IsImm(b))
  {
    int imm_reg = gpr.IsImm(a) ? a : b;
    int in_reg = gpr.IsImm(a) ? b : a;
    gpr.BindToRegister(d, d == in_reg);
    ARM64Reg WA = gpr.GetReg();
    ADDI2R(gpr.R(d), gpr.R(in_reg), gpr.GetImm(imm_reg), WA);
    gpr.Unlock(WA);
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    ADD(gpr.R(d), gpr.R(a), gpr.R(b));
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::extsXx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA, s = inst.RS;
  int size = inst.SUBOP10 == 922 ? 16 : 8;

  if (gpr.IsImm(s))
  {
    gpr.SetImmediate(a, (u32)(s32)(size == 16 ? (s16)gpr.GetImm(s) : (s8)gpr.GetImm(s)));
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else
  {
    gpr.BindToRegister(a, a == s);
    SBFM(gpr.R(a), gpr.R(s), 0, size - 1);
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::cntlzwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA;
  int s = inst.RS;

  if (gpr.IsImm(s))
  {
    gpr.SetImmediate(a, __builtin_clz(gpr.GetImm(s)));
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else
  {
    gpr.BindToRegister(a, a == s);
    CLZ(gpr.R(a), gpr.R(s));
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::negx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA;
  int d = inst.RD;

  FALLBACK_IF(inst.OE);

  if (gpr.IsImm(a))
  {
    gpr.SetImmediate(d, ~((u32)gpr.GetImm(a)) + 1);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a);
    SUB(gpr.R(d), WSP, gpr.R(a));
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::cmp(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int crf = inst.CRFD;
  u32 a = inst.RA, b = inst.RB;

  gpr.BindCRToRegister(crf, false);
  ARM64Reg CR = gpr.CR(crf);

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    s64 A = static_cast<s32>(gpr.GetImm(a));
    s64 B = static_cast<s32>(gpr.GetImm(b));
    MOVI2R(CR, A - B);
    return;
  }

  if (gpr.IsImm(b) && !gpr.GetImm(b))
  {
    SXTW(CR, gpr.R(a));
    return;
  }

  ARM64Reg WA = gpr.GetReg();
  ARM64Reg XA = EncodeRegTo64(WA);
  ARM64Reg RA = gpr.R(a);
  ARM64Reg RB = gpr.R(b);

  SXTW(XA, RA);
  SXTW(CR, RB);
  SUB(CR, XA, CR);

  gpr.Unlock(WA);
}

void JitArm64::cmpl(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int crf = inst.CRFD;
  u32 a = inst.RA, b = inst.RB;

  gpr.BindCRToRegister(crf, false);
  ARM64Reg CR = gpr.CR(crf);

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u64 A = gpr.GetImm(a);
    u64 B = gpr.GetImm(b);
    MOVI2R(CR, A - B);
    return;
  }

  if (gpr.IsImm(b) && !gpr.GetImm(b))
  {
    MOV(DecodeReg(CR), gpr.R(a));
    return;
  }

  SUB(gpr.CR(crf), EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(b)));
}

void JitArm64::cmpi(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  u32 a = inst.RA;
  s64 B = inst.SIMM_16;
  int crf = inst.CRFD;

  gpr.BindCRToRegister(crf, false);
  ARM64Reg CR = gpr.CR(crf);

  if (gpr.IsImm(a))
  {
    s64 A = static_cast<s32>(gpr.GetImm(a));
    MOVI2R(CR, A - B);
    return;
  }

  SXTW(CR, gpr.R(a));

  if (B != 0)
  {
    ARM64Reg WA = gpr.GetReg();
    SUBI2R(CR, CR, B, EncodeRegTo64(WA));
    gpr.Unlock(WA);
  }
}

void JitArm64::cmpli(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  u32 a = inst.RA;
  u64 B = inst.UIMM;
  int crf = inst.CRFD;

  gpr.BindCRToRegister(crf, false);
  ARM64Reg CR = gpr.CR(crf);

  if (gpr.IsImm(a))
  {
    u64 A = gpr.GetImm(a);
    MOVI2R(CR, A - B);
    return;
  }

  if (!B)
  {
    MOV(DecodeReg(CR), gpr.R(a));
    return;
  }

  SUBI2R(CR, EncodeRegTo64(gpr.R(a)), B, CR);
}

void JitArm64::rlwinmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  u32 a = inst.RA, s = inst.RS;

  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  if (gpr.IsImm(inst.RS))
  {
    gpr.SetImmediate(a, Common::RotateLeft(gpr.GetImm(s), inst.SH) & mask);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
    return;
  }

  gpr.BindToRegister(a, a == s);

  if (!inst.SH && mask == 0xFFFFFFFF)
  {
    if (a != s)
      MOV(gpr.R(a), gpr.R(s));
  }
  else if (!inst.SH)
  {
    // Immediate mask
    ANDI2R(gpr.R(a), gpr.R(s), mask);
  }
  else if (inst.ME == 31 && 31 < inst.SH + inst.MB)
  {
    // Bit select of the upper part
    UBFX(gpr.R(a), gpr.R(s), 32 - inst.SH, 32 - inst.MB);
  }
  else if (inst.ME == 31 - inst.SH && 32 > inst.SH + inst.MB)
  {
    // Bit select of the lower part
    UBFIZ(gpr.R(a), gpr.R(s), inst.SH, 32 - inst.SH - inst.MB);
  }
  else
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, mask);
    AND(gpr.R(a), WA, gpr.R(s), ArithOption(gpr.R(s), ST_ROR, 32 - inst.SH));
    gpr.Unlock(WA);
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(a));
}

void JitArm64::rlwnmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  const u32 a = inst.RA, b = inst.RB, s = inst.RS;
  const u32 mask = MakeRotationMask(inst.MB, inst.ME);

  if (gpr.IsImm(b) && gpr.IsImm(s))
  {
    gpr.SetImmediate(a, Common::RotateLeft(gpr.GetImm(s), gpr.GetImm(b) & 0x1F) & mask);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else if (gpr.IsImm(b))
  {
    gpr.BindToRegister(a, a == s);
    ARM64Reg WA = gpr.GetReg();
    ArithOption Shift(gpr.R(s), ST_ROR, 32 - (gpr.GetImm(b) & 0x1f));
    MOVI2R(WA, mask);
    AND(gpr.R(a), WA, gpr.R(s), Shift);
    gpr.Unlock(WA);
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
  else
  {
    gpr.BindToRegister(a, a == s || a == b);
    ARM64Reg WA = gpr.GetReg();
    NEG(WA, gpr.R(b));
    RORV(gpr.R(a), gpr.R(s), WA);
    ANDI2R(gpr.R(a), gpr.R(a), mask, WA);
    gpr.Unlock(WA);
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::srawix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA;
  int s = inst.RS;
  int amount = inst.SH;
  bool inplace_carry = CanMergeNextInstructions(1) && js.op[1].wantsCAInFlags;

  if (gpr.IsImm(s))
  {
    s32 imm = (s32)gpr.GetImm(s);
    gpr.SetImmediate(a, imm >> amount);

    if (amount != 0 && (imm < 0) && (imm << (32 - amount)))
      ComputeCarry(true);
    else
      ComputeCarry(false);

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else if (amount == 0)
  {
    gpr.BindToRegister(a, a == s);
    ARM64Reg RA = gpr.R(a);
    ARM64Reg RS = gpr.R(s);
    MOV(RA, RS);
    ComputeCarry(false);

    if (inst.Rc)
      ComputeRC0(RA);
  }
  else
  {
    gpr.BindToRegister(a, a == s);
    ARM64Reg RA = gpr.R(a);
    ARM64Reg RS = gpr.R(s);

    if (js.op->wantsCA)
    {
      ARM64Reg WA = gpr.GetReg();
      ARM64Reg dest = inplace_carry ? WA : WSP;
      if (a != s)
      {
        ASR(RA, RS, amount);
        ANDS(dest, RA, RS, ArithOption(RS, ST_LSL, 32 - amount));
      }
      else
      {
        LSL(WA, RS, 32 - amount);
        ASR(RA, RS, amount);
        ANDS(dest, WA, RA);
      }
      if (inplace_carry)
      {
        CMP(dest, 1);
        ComputeCarry();
      }
      else
      {
        CSINC(WA, WSP, WSP, CC_EQ);
        STRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      }
      gpr.Unlock(WA);
    }
    else
    {
      ASR(RA, RS, amount);
    }

    if (inst.Rc)
      ComputeRC0(RA);
  }
}

void JitArm64::addic(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, d = inst.RD;
  bool rc = inst.OPCD == 13;
  s32 simm = inst.SIMM_16;
  u32 imm = (u32)simm;

  if (gpr.IsImm(a))
  {
    u32 i = gpr.GetImm(a);
    gpr.SetImmediate(d, i + imm);

    bool has_carry = Interpreter::Helper_Carry(i, imm);
    ComputeCarry(has_carry);
    if (rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a);
    ARM64Reg WA = gpr.GetReg();
    ADDSI2R(gpr.R(d), gpr.R(a), simm, WA);
    gpr.Unlock(WA);

    ComputeCarry();
    if (rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::mulli(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, d = inst.RD;

  if (gpr.IsImm(a))
  {
    s32 i = (s32)gpr.GetImm(a);
    gpr.SetImmediate(d, i * inst.SIMM_16);
  }
  else
  {
    gpr.BindToRegister(d, d == a);
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, (u32)(s32)inst.SIMM_16);
    MUL(gpr.R(d), gpr.R(a), WA);
    gpr.Unlock(WA);
  }
}

void JitArm64::mullwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    s32 i = (s32)gpr.GetImm(a), j = (s32)gpr.GetImm(b);
    gpr.SetImmediate(d, i * j);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    MUL(gpr.R(d), gpr.R(a), gpr.R(b));
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::mulhwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    s32 i = (s32)gpr.GetImm(a), j = (s32)gpr.GetImm(b);
    gpr.SetImmediate(d, (u32)((u64)(((s64)i * (s64)j)) >> 32));
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    SMULL(EncodeRegTo64(gpr.R(d)), gpr.R(a), gpr.R(b));
    LSR(EncodeRegTo64(gpr.R(d)), EncodeRegTo64(gpr.R(d)), 32);

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::mulhwux(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
    gpr.SetImmediate(d, (u32)(((u64)i * (u64)j) >> 32));
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    UMULL(EncodeRegTo64(gpr.R(d)), gpr.R(a), gpr.R(b));
    LSR(EncodeRegTo64(gpr.R(d)), EncodeRegTo64(gpr.R(d)), 32);

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::addzex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, d = inst.RD;

  if (js.carryFlagSet)
  {
    gpr.BindToRegister(d, d == a);
    ADCS(gpr.R(d), gpr.R(a), WZR);
  }
  else if (d == a)
  {
    gpr.BindToRegister(d, true);
    ARM64Reg WA = gpr.GetReg();
    LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    ADDS(gpr.R(d), gpr.R(a), WA);
    gpr.Unlock(WA);
  }
  else
  {
    gpr.BindToRegister(d, false);
    LDRB(INDEX_UNSIGNED, gpr.R(d), PPC_REG, PPCSTATE_OFF(xer_ca));
    ADDS(gpr.R(d), gpr.R(a), gpr.R(d));
  }

  ComputeCarry();
  if (inst.Rc)
    ComputeRC0(gpr.R(d));
}

void JitArm64::subfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
    gpr.SetImmediate(d, j - i);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    SUB(gpr.R(d), gpr.R(b), gpr.R(a));
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::subfex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);

    gpr.BindToRegister(d, false);
    ARM64Reg WA = gpr.GetReg();
    if (js.carryFlagSet)
    {
      MOVI2R(WA, ~i + j, gpr.R(d));
      ADC(gpr.R(d), WA, WZR);
    }
    else
    {
      LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      ADDI2R(gpr.R(d), WA, ~i + j, gpr.R(d));
    }
    gpr.Unlock(WA);

    bool must_have_carry = Interpreter::Helper_Carry(~i, j);
    bool might_have_carry = (~i + j) == 0xFFFFFFFF;

    if (must_have_carry)
    {
      ComputeCarry(true);
    }
    else if (might_have_carry)
    {
      // carry stay as it is
    }
    else
    {
      ComputeCarry(false);
    }
  }
  else
  {
    ARM64Reg WA = gpr.GetReg();
    gpr.BindToRegister(d, d == a || d == b);

    // upload the carry state
    if (!js.carryFlagSet)
    {
      LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      CMP(WA, 1);
    }

    // d = ~a + b + carry;
    if (gpr.IsImm(a))
      MOVI2R(WA, ~gpr.GetImm(a));
    else
      MVN(WA, gpr.R(a));
    ADCS(gpr.R(d), WA, gpr.R(b));

    gpr.Unlock(WA);

    ComputeCarry();
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(d));
}

void JitArm64::subfcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 a_imm = gpr.GetImm(a), b_imm = gpr.GetImm(b);

    gpr.SetImmediate(d, b_imm - a_imm);
    ComputeCarry(a_imm == 0 || Interpreter::Helper_Carry(b_imm, 0u - a_imm));

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);

    // d = b - a
    SUBS(gpr.R(d), gpr.R(b), gpr.R(a));

    ComputeCarry();

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::subfzex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, d = inst.RD;

  gpr.BindToRegister(d, d == a);

  if (js.carryFlagSet)
  {
    MVN(gpr.R(d), gpr.R(a));
    ADCS(gpr.R(d), gpr.R(d), WZR);
  }
  else
  {
    ARM64Reg WA = gpr.GetReg();
    LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    MVN(gpr.R(d), gpr.R(a));
    ADDS(gpr.R(d), gpr.R(d), WA);
    gpr.Unlock(WA);
  }

  ComputeCarry();

  if (inst.Rc)
    ComputeRC0(gpr.R(d));
}

void JitArm64::subfic(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, d = inst.RD;
  s32 imm = inst.SIMM_16;

  if (gpr.IsImm(a))
  {
    u32 a_imm = gpr.GetImm(a);

    gpr.SetImmediate(d, imm - a_imm);
    ComputeCarry(a_imm == 0 || Interpreter::Helper_Carry(imm, 0u - a_imm));
  }
  else
  {
    gpr.BindToRegister(d, d == a);

    // d = imm - a
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, imm);
    SUBS(gpr.R(d), WA, gpr.R(a));
    gpr.Unlock(WA);

    ComputeCarry();
  }
}

void JitArm64::addex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);

    gpr.BindToRegister(d, false);
    ARM64Reg WA = gpr.GetReg();
    if (js.carryFlagSet)
    {
      MOVI2R(WA, i + j);
      ADC(gpr.R(d), WA, WZR);
    }
    else
    {
      LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      ADDI2R(gpr.R(d), WA, i + j, gpr.R(d));
    }
    gpr.Unlock(WA);

    bool must_have_carry = Interpreter::Helper_Carry(i, j);
    bool might_have_carry = (i + j) == 0xFFFFFFFF;

    if (must_have_carry)
    {
      ComputeCarry(true);
    }
    else if (might_have_carry)
    {
      // carry stay as it is
    }
    else
    {
      ComputeCarry(false);
    }
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);

    // upload the carry state
    if (!js.carryFlagSet)
    {
      ARM64Reg WA = gpr.GetReg();
      LDRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      CMP(WA, 1);
      gpr.Unlock(WA);
    }

    // d = a + b + carry;
    ADCS(gpr.R(d), gpr.R(a), gpr.R(b));

    ComputeCarry();
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(d));
}

void JitArm64::addcx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
    gpr.SetImmediate(d, i + j);

    bool has_carry = Interpreter::Helper_Carry(i, j);
    ComputeCarry(has_carry);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    ADDS(gpr.R(d), gpr.R(a), gpr.R(b));

    ComputeCarry();
    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::divwux(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(a), j = gpr.GetImm(b);
    gpr.SetImmediate(d, j == 0 ? 0 : i / j);

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);

    // d = a / b
    UDIV(gpr.R(d), gpr.R(a), gpr.R(b));

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
}

void JitArm64::divwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && gpr.IsImm(b))
  {
    s32 imm_a = gpr.GetImm(a);
    s32 imm_b = gpr.GetImm(b);
    u32 imm_d;
    if (imm_b == 0 || (static_cast<u32>(imm_a) == 0x80000000 && imm_b == -1))
    {
      if (imm_a < 0)
        imm_d = 0xFFFFFFFF;
      else
        imm_d = 0;
    }
    else
    {
      imm_d = static_cast<u32>(imm_a / imm_b);
    }
    gpr.SetImmediate(d, imm_d);

    if (inst.Rc)
      ComputeRC0(imm_d);
  }
  else if (gpr.IsImm(b) && gpr.GetImm(b) != 0 && gpr.GetImm(b) != -1u)
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, gpr.GetImm(b));

    gpr.BindToRegister(d, d == a);

    SDIV(gpr.R(d), gpr.R(a), WA);

    gpr.Unlock(WA);

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
  else
  {
    FlushCarry();

    gpr.BindToRegister(d, d == a || d == b);

    ARM64Reg WA = gpr.GetReg();
    ARM64Reg RA = gpr.R(a);
    ARM64Reg RB = gpr.R(b);
    ARM64Reg RD = gpr.R(d);

    FixupBranch slow1 = CBZ(RB);
    MOVI2R(WA, -0x80000000LL);
    CMP(RA, WA);
    CCMN(RB, 1, 0, CC_EQ);
    FixupBranch slow2 = B(CC_EQ);
    SDIV(RD, RA, RB);
    FixupBranch done = B();

    SetJumpTarget(slow1);
    SetJumpTarget(slow2);

    ASR(RD, RA, 31);

    SetJumpTarget(done);

    gpr.Unlock(WA);

    if (inst.Rc)
      ComputeRC0(RD);
  }
}

void JitArm64::slwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, b = inst.RB, s = inst.RS;

  if (gpr.IsImm(b) && gpr.IsImm(s))
  {
    u32 i = gpr.GetImm(s), j = gpr.GetImm(b);
    gpr.SetImmediate(a, (j & 0x20) ? 0 : i << (j & 0x1F));

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else if (gpr.IsImm(b))
  {
    u32 i = gpr.GetImm(b);
    if (i & 0x20)
    {
      gpr.SetImmediate(a, 0);
      if (inst.Rc)
        ComputeRC0(0);
    }
    else
    {
      gpr.BindToRegister(a, a == s);
      LSL(gpr.R(a), gpr.R(s), i & 0x1F);
      if (inst.Rc)
        ComputeRC0(gpr.R(a));
    }
  }
  else
  {
    gpr.BindToRegister(a, a == b || a == s);

    // PowerPC any shift in the 32-63 register range results in zero
    // Since it has 32bit registers
    // AArch64 it will use a mask of the register size for determining what shift amount
    // So if we use a 64bit so the bits will end up in the high 32bits, and
    // Later instructions will just eat high 32bits since it'll run 32bit operations for everything.
    LSLV(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(s)), EncodeRegTo64(gpr.R(b)));

    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::srwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, b = inst.RB, s = inst.RS;

  if (gpr.IsImm(b) && gpr.IsImm(s))
  {
    u32 i = gpr.GetImm(s), amount = gpr.GetImm(b);
    gpr.SetImmediate(a, (amount & 0x20) ? 0 : i >> (amount & 0x1F));

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
  }
  else if (gpr.IsImm(b))
  {
    u32 amount = gpr.GetImm(b);
    if (amount & 0x20)
    {
      gpr.SetImmediate(a, 0);
      if (inst.Rc)
        ComputeRC0(0);
    }
    else
    {
      gpr.BindToRegister(a, a == s);
      LSR(gpr.R(a), gpr.R(s), amount & 0x1F);
      if (inst.Rc)
        ComputeRC0(gpr.R(a));
    }
  }
  else
  {
    gpr.BindToRegister(a, a == b || a == s);

    // wipe upper bits. TODO: get rid of it, but then no instruction is allowed to emit some higher
    // bits.
    MOV(gpr.R(s), gpr.R(s));

    LSRV(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(s)), EncodeRegTo64(gpr.R(b)));

    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}

void JitArm64::srawx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  int a = inst.RA, b = inst.RB, s = inst.RS;
  bool inplace_carry = CanMergeNextInstructions(1) && js.op[1].wantsCAInFlags;

  if (gpr.IsImm(b) && gpr.IsImm(s))
  {
    s32 i = gpr.GetImm(s), amount = gpr.GetImm(b);
    if (amount & 0x20)
    {
      gpr.SetImmediate(a, i & 0x80000000 ? 0xFFFFFFFF : 0);
      ComputeCarry(i & 0x80000000 ? true : false);
    }
    else
    {
      amount &= 0x1F;
      gpr.SetImmediate(a, i >> amount);
      ComputeCarry(amount != 0 && i < 0 && (i << (32 - amount)));
    }

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
    return;
  }

  if (gpr.IsImm(b) && !js.op->wantsCA)
  {
    int amount = gpr.GetImm(b);
    if (amount & 0x20)
      amount = 0x1F;
    else
      amount &= 0x1F;
    gpr.BindToRegister(a, a == s);
    ASR(gpr.R(a), gpr.R(s), amount);
  }
  else if (!js.op->wantsCA)
  {
    gpr.BindToRegister(a, a == b || a == s);

    ARM64Reg WA = gpr.GetReg();

    LSL(EncodeRegTo64(WA), EncodeRegTo64(gpr.R(s)), 32);
    ASRV(EncodeRegTo64(WA), EncodeRegTo64(WA), EncodeRegTo64(gpr.R(b)));
    LSR(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(WA), 32);

    gpr.Unlock(WA);
  }
  else
  {
    gpr.BindToRegister(a, a == b || a == s);
    ARM64Reg WA = gpr.GetReg();
    ARM64Reg WB = gpr.GetReg();
    ARM64Reg WC = gpr.GetReg();
    ARM64Reg RB = gpr.R(b);
    ARM64Reg RS = gpr.R(s);

    ANDI2R(WA, RB, 32);
    FixupBranch bit_is_not_zero = TBNZ(RB, 5);

    ANDSI2R(WC, RB, 31);
    MOV(WB, RS);
    FixupBranch is_zero = B(CC_EQ);

    ASRV(WB, RS, WC);
    FixupBranch bit_is_zero = TBZ(RS, 31);

    MOVI2R(WA, 32);
    SUB(WC, WA, WC);
    LSL(WC, RS, WC);
    CMP(WC, 0);
    CSET(WA, CC_NEQ);
    FixupBranch end = B();

    SetJumpTarget(bit_is_not_zero);
    CMP(RS, 0);
    CSET(WA, CC_LT);
    CSINV(WB, WZR, WZR, CC_GE);

    SetJumpTarget(is_zero);
    SetJumpTarget(bit_is_zero);
    SetJumpTarget(end);

    MOV(gpr.R(a), WB);
    if (inplace_carry)
    {
      CMP(WA, 1);
      ComputeCarry();
    }
    else
    {
      STRB(INDEX_UNSIGNED, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    }

    gpr.Unlock(WA, WB, WC);
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(a));
}

void JitArm64::rlwimix(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  const int a = inst.RA, s = inst.RS;
  const u32 mask = MakeRotationMask(inst.MB, inst.ME);

  if (gpr.IsImm(a) && gpr.IsImm(s))
  {
    u32 res = (gpr.GetImm(a) & ~mask) | (Common::RotateLeft(gpr.GetImm(s), inst.SH) & mask);
    gpr.SetImmediate(a, res);
    if (inst.Rc)
      ComputeRC0(res);
  }
  else
  {
    if (mask == 0 || (a == s && inst.SH == 0))
    {
      // Do Nothing
    }
    else if (mask == 0xFFFFFFFF)
    {
      if (inst.SH || a != s)
        gpr.BindToRegister(a, a == s);

      if (inst.SH)
        ROR(gpr.R(a), gpr.R(s), 32 - inst.SH);
      else if (a != s)
        MOV(gpr.R(a), gpr.R(s));
    }
    else if (inst.SH == 0 && inst.MB <= inst.ME)
    {
      // No rotation
      // No mask inversion
      u32 lsb = 31 - inst.ME;
      u32 width = inst.ME - inst.MB + 1;

      gpr.BindToRegister(a, true);
      ARM64Reg WA = gpr.GetReg();
      UBFX(WA, gpr.R(s), lsb, width);
      BFI(gpr.R(a), WA, lsb, width);
      gpr.Unlock(WA);
    }
    else if (inst.SH && inst.MB <= inst.ME)
    {
      // No mask inversion
      u32 lsb = 31 - inst.ME;
      u32 width = inst.ME - inst.MB + 1;

      gpr.BindToRegister(a, true);
      ARM64Reg WA = gpr.GetReg();
      ROR(WA, gpr.R(s), 32 - inst.SH);
      UBFX(WA, WA, lsb, width);
      BFI(gpr.R(a), WA, lsb, width);
      gpr.Unlock(WA);
    }
    else
    {
      gpr.BindToRegister(a, true);
      ARM64Reg WA = gpr.GetReg();
      ARM64Reg WB = gpr.GetReg();

      MOVI2R(WA, mask);
      BIC(WB, gpr.R(a), WA);
      AND(WA, WA, gpr.R(s), ArithOption(gpr.R(s), ST_ROR, 32 - inst.SH));
      ORR(gpr.R(a), WB, WA);

      gpr.Unlock(WA, WB);
    }

    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}
