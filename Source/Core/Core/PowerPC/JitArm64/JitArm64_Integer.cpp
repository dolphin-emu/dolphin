// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/Jit.h"

#include <bit>

#include "Common/Arm64Emitter.h"
#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/JitCommon/DivUtils.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;
using namespace JitCommon;

#define CARRY_IF_NEEDED(inst_without_carry, inst_with_carry, ...)                                  \
  do                                                                                               \
  {                                                                                                \
    if (js.op->wantsCA)                                                                            \
      inst_with_carry(__VA_ARGS__);                                                                \
    else                                                                                           \
      inst_without_carry(__VA_ARGS__);                                                             \
  } while (0)

void JitArm64::ComputeRC0(ARM64Reg reg)
{
  gpr.BindCRToRegister(0, false);
  SXTW(gpr.CR(0), reg);
}

void JitArm64::ComputeRC0(u32 imm)
{
  gpr.BindCRToRegister(0, false);
  MOVI2R(gpr.CR(0), s64(s32(imm)));
}

void JitArm64::GenerateConstantOverflow(bool overflow)
{
  ARM64Reg WA = gpr.GetReg();

  if (overflow)
  {
    MOVI2R(WA, XER_OV_MASK | XER_SO_MASK);
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_so_ov));
  }
  else
  {
    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_so_ov));
    AND(WA, WA, LogicalImm(~XER_OV_MASK, GPRSize::B32));
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_so_ov));
  }

  gpr.Unlock(WA);
}

void JitArm64::ComputeCarry(ARM64Reg reg)
{
  js.carryFlag = CarryFlag::InPPCState;

  if (!js.op->wantsCA)
    return;

  if (CanMergeNextInstructions(1) && js.op[1].wantsCAInFlags)
  {
    CMP(reg, 1);
    js.carryFlag = CarryFlag::InHostCarry;
  }
  else
  {
    STRB(IndexType::Unsigned, reg, PPC_REG, PPCSTATE_OFF(xer_ca));
  }
}

void JitArm64::ComputeCarry(bool carry)
{
  js.carryFlag = carry ? CarryFlag::ConstantTrue : CarryFlag::ConstantFalse;
}

void JitArm64::ComputeCarry()
{
  js.carryFlag = CarryFlag::InPPCState;

  if (!js.op->wantsCA)
    return;

  js.carryFlag = CarryFlag::InHostCarry;
  if (CanMergeNextInstructions(1) && js.op[1].opinfo->type == ::OpType::Integer)
    return;

  FlushCarry();
}

void JitArm64::LoadCarry()
{
  switch (js.carryFlag)
  {
  case CarryFlag::InPPCState:
  {
    ARM64Reg WA = gpr.GetReg();
    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    CMP(WA, 1);
    gpr.Unlock(WA);
    break;
  }
  case CarryFlag::InHostCarry:
  {
    break;
  }
  case CarryFlag::ConstantTrue:
  {
    CMP(ARM64Reg::WZR, ARM64Reg::WZR);
    break;
  }
  case CarryFlag::ConstantFalse:
  {
    CMN(ARM64Reg::WZR, ARM64Reg::WZR);
    break;
  }
  }
}

void JitArm64::FlushCarry()
{
  switch (js.carryFlag)
  {
  case CarryFlag::InPPCState:
  {
    break;
  }
  case CarryFlag::InHostCarry:
  {
    ARM64Reg WA = gpr.GetReg();
    CSET(WA, CC_CS);
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    gpr.Unlock(WA);
    break;
  }
  case CarryFlag::ConstantTrue:
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, 1);
    STRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    gpr.Unlock(WA);
    break;
  }
  case CarryFlag::ConstantFalse:
  {
    STRB(IndexType::Unsigned, ARM64Reg::WZR, PPC_REG, PPCSTATE_OFF(xer_ca));
    break;
  }
  }

  js.carryFlag = CarryFlag::InPPCState;
}

void JitArm64::reg_imm(u32 d, u32 a, u32 value,
                       void (ARM64XEmitter::*op)(ARM64Reg, ARM64Reg, u64, ARM64Reg), bool Rc)
{
  gpr.BindToRegister(d, d == a);
  ARM64Reg WA = gpr.GetReg();
  (this->*op)(gpr.R(d), gpr.R(a), value, WA);
  gpr.Unlock(WA);

  if (Rc)
    ComputeRC0(gpr.R(d));
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
    const u32 immediate = inst.OPCD == 24 ? inst.UIMM : inst.UIMM << 16;
    reg_imm(a, s, immediate, &ARM64XEmitter::ORRI2R);
    break;
  }
  case 28:  // andi
    reg_imm(a, s, inst.UIMM, &ARM64XEmitter::ANDI2R, true);
    break;
  case 29:  // andis
    reg_imm(a, s, inst.UIMM << 16, &ARM64XEmitter::ANDI2R, true);
    break;
  case 26:  // xori
  case 27:  // xoris
  {
    const u32 immediate = inst.OPCD == 26 ? inst.UIMM : inst.UIMM << 16;
    reg_imm(a, s, immediate, &ARM64XEmitter::EORI2R);
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
    gpr.BindToRegister(d, d == a);

    ARM64Reg WA = gpr.GetReg();
    ADDI2R(gpr.R(d), gpr.R(a), imm, WA);
    gpr.Unlock(WA);
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

  if (s == b)
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
      PanicAlertFmt("WTF!");
    }
  }
  else if ((gpr.IsImm(s) && (gpr.GetImm(s) == 0 || gpr.GetImm(s) == 0xFFFFFFFF ||
                             LogicalImm(gpr.GetImm(s), GPRSize::B32))) ||
           (gpr.IsImm(b) && (gpr.GetImm(b) == 0 || gpr.GetImm(b) == 0xFFFFFFFF ||
                             LogicalImm(gpr.GetImm(b), GPRSize::B32))))
  {
    int i, j;
    if (gpr.IsImm(s))
    {
      i = s;
      j = b;
    }
    else
    {
      i = b;
      j = s;
    }

    bool complement_b = (inst.SUBOP10 == 60 /* andcx */) || (inst.SUBOP10 == 412 /* orcx */);
    const bool final_not = (inst.SUBOP10 == 476 /* nandx */) || (inst.SUBOP10 == 124 /* norx */);
    const bool is_and = (inst.SUBOP10 == 28 /* andx */) || (inst.SUBOP10 == 60 /* andcx */) ||
                        (inst.SUBOP10 == 476 /* nandx */);
    const bool is_or = (inst.SUBOP10 == 444 /* orx */) || (inst.SUBOP10 == 412 /* orcx */) ||
                       (inst.SUBOP10 == 124 /* norx */);
    const bool is_xor = (inst.SUBOP10 == 316 /* xorx */) || (inst.SUBOP10 == 284 /* eqvx */);

    u32 imm = gpr.GetImm(i);
    if ((complement_b && i == b) || (inst.SUBOP10 == 284 /* eqvx */))
    {
      imm = ~imm;
      complement_b = false;
    }

    const bool is_zero = imm == 0;
    const bool is_ones = imm == 0xFFFFFFFF;
    // If imm can be represented as LogicalImm, so can ~imm.
    const auto log_imm = LogicalImm(imm, GPRSize::B32);

    if (is_xor)
    {
      if (is_zero)
      {
        if (a != j)
        {
          gpr.BindToRegister(a, false);
          MOV(gpr.R(a), gpr.R(j));
        }
      }
      else
      {
        gpr.BindToRegister(a, a == j);
        if (is_ones)
        {
          MVN(gpr.R(a), gpr.R(j));
        }
        else
        {
          EOR(gpr.R(a), gpr.R(j), log_imm);
        }
      }
      if (inst.Rc)
        ComputeRC0(gpr.R(a));
    }
    else if (is_and)
    {
      if (is_zero)
      {
        gpr.SetImmediate(a, final_not ? 0xFFFFFFFF : 0);
        if (inst.Rc)
          ComputeRC0(gpr.GetImm(a));
      }
      else if (is_ones)
      {
        if (final_not || complement_b)
        {
          gpr.BindToRegister(a, a == j);
          MVN(gpr.R(a), gpr.R(j));
        }
        else if (a != j)
        {
          gpr.BindToRegister(a, false);
          MOV(gpr.R(a), gpr.R(j));
        }
        if (inst.Rc)
          ComputeRC0(gpr.R(a));
      }
      else
      {
        if (!complement_b)
        {
          gpr.BindToRegister(a, a == j);
          AND(gpr.R(a), gpr.R(j), log_imm);
          if (final_not)
            MVN(gpr.R(a), gpr.R(a));
        }
        else
        {
          // No shorter instruction sequence is possible. Just materialize the
          // immediate in a register as usual, so subsequent uses can leech off
          // of it.
          gpr.BindToRegister(a, (a == i) || (a == j));
          BIC(gpr.R(a), gpr.R(i), gpr.R(j));
        }
        if (inst.Rc)
          ComputeRC0(gpr.R(a));
      }
    }
    else if (is_or)
    {
      if (is_ones)
      {
        gpr.SetImmediate(a, final_not ? 0 : 0xFFFFFFFF);
        if (inst.Rc)
          ComputeRC0(gpr.GetImm(a));
      }
      else if (is_zero)
      {
        if (final_not || complement_b)
        {
          gpr.BindToRegister(a, a == j);
          MVN(gpr.R(a), gpr.R(j));
        }
        else if (a != j)
        {
          gpr.BindToRegister(a, false);
          MOV(gpr.R(a), gpr.R(j));
        }
        if (inst.Rc)
          ComputeRC0(gpr.R(a));
      }
      else
      {
        if (!complement_b)
        {
          gpr.BindToRegister(a, a == j);
          ORR(gpr.R(a), gpr.R(j), log_imm);
          if (final_not)
            MVN(gpr.R(a), gpr.R(a));
        }
        else
        {
          // No shorter instruction sequence is possible. Just materialize the
          // immediate in a register as usual, so subsequent uses can leech off
          // of it.
          gpr.BindToRegister(a, (a == i) || (a == j));
          ORN(gpr.R(a), gpr.R(i), gpr.R(j));
        }
        if (inst.Rc)
          ComputeRC0(gpr.R(a));
      }
    }
    else
    {
      PanicAlertFmt("WTF!");
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
      PanicAlertFmt("WTF!");
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

  if (gpr.IsImm(a) || gpr.IsImm(b))
  {
    int imm_reg = gpr.IsImm(a) ? a : b;
    int in_reg = gpr.IsImm(a) ? b : a;
    int imm_value = gpr.GetImm(imm_reg);

    gpr.BindToRegister(d, d == in_reg);
    ARM64Reg WA = gpr.GetReg();
    ADDI2R(gpr.R(d), gpr.R(in_reg), imm_value, WA);
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

  gpr.BindToRegister(a, a == s);
  SBFM(gpr.R(a), gpr.R(s), 0, size - 1);
  if (inst.Rc)
    ComputeRC0(gpr.R(a));
}

void JitArm64::cntlzwx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA;
  int s = inst.RS;

  gpr.BindToRegister(a, a == s);
  CLZ(gpr.R(a), gpr.R(s));
  if (inst.Rc)
    ComputeRC0(gpr.R(a));
}

void JitArm64::negx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  int a = inst.RA;
  int d = inst.RD;

  FALLBACK_IF(inst.OE);

  gpr.BindToRegister(d, d == a);
  SUB(gpr.R(d), ARM64Reg::WSP, gpr.R(a));
  if (inst.Rc)
    ComputeRC0(gpr.R(d));
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
  }
  else if (gpr.IsImm(a) && !gpr.GetImm(a))
  {
    SXTW(CR, gpr.R(b));
    NEG(CR, CR);
  }
  else if (gpr.IsImm(a) && gpr.GetImm(a) == 0xFFFFFFFF)
  {
    SXTW(CR, gpr.R(b));
    MVN(CR, CR);
  }
  else if (gpr.IsImm(b) && !gpr.GetImm(b))
  {
    SXTW(CR, gpr.R(a));
  }
  else
  {
    ARM64Reg RA = gpr.R(a);
    ARM64Reg RB = gpr.R(b);

    SXTW(CR, RA);
    SUB(CR, CR, RB, ArithOption(RB, ExtendSpecifier::SXTW));
  }
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
  }
  else if (gpr.IsImm(a) && !gpr.GetImm(a))
  {
    NEG(CR, EncodeRegTo64(gpr.R(b)));
  }
  else if (gpr.IsImm(b) && !gpr.GetImm(b))
  {
    MOV(EncodeRegTo32(CR), gpr.R(a));
  }
  else
  {
    SUB(CR, EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(b)));
  }
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
    MOV(EncodeRegTo32(CR), gpr.R(a));
    return;
  }

  SUBI2R(CR, EncodeRegTo64(gpr.R(a)), B, CR);
}

void JitArm64::rlwinmx_internal(UGeckoInstruction inst, u32 sh)
{
  u32 a = inst.RA, s = inst.RS;

  const u32 mask = MakeRotationMask(inst.MB, inst.ME);
  if (gpr.IsImm(inst.RS))
  {
    gpr.SetImmediate(a, std::rotl(gpr.GetImm(s), sh) & mask);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
    return;
  }

  gpr.BindToRegister(a, a == s);

  if (sh == 0 && mask == 0xFFFFFFFF)
  {
    if (a != s)
      MOV(gpr.R(a), gpr.R(s));
  }
  else if (sh == 0)
  {
    // Immediate mask
    AND(gpr.R(a), gpr.R(s), LogicalImm(mask, GPRSize::B32));
  }
  else if (mask == 0xFFFFFFFF)
  {
    ROR(gpr.R(a), gpr.R(s), 32 - sh);
  }
  else if (inst.ME == 31 && 31 < sh + inst.MB)
  {
    // Bit select of the upper part
    UBFX(gpr.R(a), gpr.R(s), 32 - sh, 32 - inst.MB);
  }
  else if (inst.ME == 31 - sh && 32 > sh + inst.MB)
  {
    // Bit select of the lower part
    UBFIZ(gpr.R(a), gpr.R(s), sh, 32 - sh - inst.MB);
  }
  else
  {
    ARM64Reg WA = gpr.GetReg();
    MOVI2R(WA, mask);
    AND(gpr.R(a), WA, gpr.R(s), ArithOption(gpr.R(s), ShiftType::ROR, 32 - sh));
    gpr.Unlock(WA);
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(a));
}

void JitArm64::rlwinmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);

  rlwinmx_internal(inst, inst.SH);
}

void JitArm64::rlwnmx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  const u32 a = inst.RA, b = inst.RB, s = inst.RS;

  if (gpr.IsImm(b))
  {
    rlwinmx_internal(inst, gpr.GetImm(b) & 0x1F);
    return;
  }

  const u32 mask = MakeRotationMask(inst.MB, inst.ME);

  gpr.BindToRegister(a, a == s || a == b);
  ARM64Reg WA = gpr.GetReg();
  NEG(WA, gpr.R(b));
  RORV(gpr.R(a), gpr.R(s), WA);
  ANDI2R(gpr.R(a), gpr.R(a), mask, WA);
  gpr.Unlock(WA);

  if (inst.Rc)
    ComputeRC0(gpr.R(a));
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

    ComputeCarry(amount != 0 && (imm < 0) && (u32(imm) << (32 - amount)));

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
      ARM64Reg dest = inplace_carry ? WA : ARM64Reg::WSP;
      if (a != s)
      {
        ASR(RA, RS, amount);
        ANDS(dest, RA, RS, ArithOption(RS, ShiftType::LSL, 32 - amount));
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
        CSINC(WA, ARM64Reg::WSP, ARM64Reg::WSP, CC_EQ);
        ComputeCarry(WA);
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
    CARRY_IF_NEEDED(ADDI2R, ADDSI2R, gpr.R(d), gpr.R(a), simm, WA);
    gpr.Unlock(WA);

    ComputeCarry();
    if (rc)
      ComputeRC0(gpr.R(d));
  }
}

bool JitArm64::MultiplyImmediate(u32 imm, int a, int d, bool rc)
{
  if (imm == 0)
  {
    // Multiplication by zero (0).
    gpr.SetImmediate(d, 0);
    if (rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else if (imm == 1)
  {
    // Multiplication by one (1).
    if (d != a)
    {
      gpr.BindToRegister(d, false);
      MOV(gpr.R(d), gpr.R(a));
    }
    if (rc)
      ComputeRC0(gpr.R(d));
  }
  else if (MathUtil::IsPow2(imm))
  {
    // Multiplication by a power of two (2^n).
    const int shift = MathUtil::IntLog2(imm);

    gpr.BindToRegister(d, d == a);
    LSL(gpr.R(d), gpr.R(a), shift);
    if (rc)
      ComputeRC0(gpr.R(d));
  }
  else if (MathUtil::IsPow2(imm - 1))
  {
    // Multiplication by a power of two plus one (2^n + 1).
    const int shift = MathUtil::IntLog2(imm - 1);

    gpr.BindToRegister(d, d == a);
    ADD(gpr.R(d), gpr.R(a), gpr.R(a), ArithOption(gpr.R(a), ShiftType::LSL, shift));
    if (rc)
      ComputeRC0(gpr.R(d));
  }
  else if (MathUtil::IsPow2(~imm + 1))
  {
    // Multiplication by a negative power of two (-(2^n)).
    const int shift = MathUtil::IntLog2(~imm + 1);

    gpr.BindToRegister(d, d == a);
    NEG(gpr.R(d), gpr.R(a), ArithOption(gpr.R(a), ShiftType::LSL, shift));
    if (rc)
      ComputeRC0(gpr.R(d));
  }
  else if (MathUtil::IsPow2(~imm + 2))
  {
    // Multiplication by a negative power of two plus one (-(2^n) + 1).
    const int shift = MathUtil::IntLog2(~imm + 2);

    gpr.BindToRegister(d, d == a);
    SUB(gpr.R(d), gpr.R(a), gpr.R(a), ArithOption(gpr.R(a), ShiftType::LSL, shift));
    if (rc)
      ComputeRC0(gpr.R(d));
  }
  else
  {
    // Immediate did not match any known special cases.
    return false;
  }

  return true;
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
  else if (MultiplyImmediate((u32)(s32)inst.SIMM_16, a, d, false))
  {
    // Code is generated inside MultiplyImmediate, nothing to be done here.
  }
  else
  {
    const bool allocate_reg = d == a;
    gpr.BindToRegister(d, allocate_reg);

    // Reuse d to hold the immediate if possible, allocate a register otherwise.
    ARM64Reg WA = allocate_reg ? gpr.GetReg() : gpr.R(d);

    MOVI2R(WA, (u32)(s32)inst.SIMM_16);
    MUL(gpr.R(d), gpr.R(a), WA);
    if (allocate_reg)
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
  else if ((gpr.IsImm(a) && MultiplyImmediate(gpr.GetImm(a), b, d, inst.Rc)) ||
           (gpr.IsImm(b) && MultiplyImmediate(gpr.GetImm(b), a, d, inst.Rc)))
  {
    // Code is generated inside MultiplyImmediate, nothing to be done here.
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

  switch (js.carryFlag)
  {
  case CarryFlag::InPPCState:
  {
    gpr.BindToRegister(d, d == a);
    ARM64Reg WA = d == a ? gpr.GetReg() : gpr.R(d);

    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    CARRY_IF_NEEDED(ADD, ADDS, gpr.R(d), gpr.R(a), WA);
    ComputeCarry();

    if (d == a)
      gpr.Unlock(WA);

    break;
  }
  case CarryFlag::InHostCarry:
  {
    gpr.BindToRegister(d, d == a);
    CARRY_IF_NEEDED(ADC, ADCS, gpr.R(d), gpr.R(a), ARM64Reg::WZR);
    ComputeCarry();
    break;
  }
  case CarryFlag::ConstantTrue:
  {
    gpr.BindToRegister(d, d == a);
    CARRY_IF_NEEDED(ADD, ADDS, gpr.R(d), gpr.R(a), 1);
    ComputeCarry();
    break;
  }
  case CarryFlag::ConstantFalse:
  {
    if (d != a)
    {
      gpr.BindToRegister(d, false);
      MOV(gpr.R(d), gpr.R(a));
    }

    ComputeCarry(false);
    break;
  }
  }

  if (inst.Rc)
    ComputeRC0(gpr.R(d));
}

void JitArm64::subfx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (a == b)
  {
    gpr.SetImmediate(d, 0);
    if (inst.Rc)
      ComputeRC0(gpr.GetImm(d));
  }
  else if (gpr.IsImm(a) && gpr.IsImm(b))
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

  bool mex = inst.SUBOP10 & 32;
  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && (mex || gpr.IsImm(b)))
  {
    u32 i = gpr.GetImm(a), j = mex ? -1 : gpr.GetImm(b);

    gpr.BindToRegister(d, false);

    switch (js.carryFlag)
    {
    case CarryFlag::InPPCState:
    {
      ARM64Reg WA = gpr.GetReg();
      LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      ADDI2R(gpr.R(d), WA, ~i + j, gpr.R(d));
      gpr.Unlock(WA);
      break;
    }
    case CarryFlag::InHostCarry:
    {
      ARM64Reg WA = gpr.GetReg();
      MOVI2R(WA, ~i + j);
      ADC(gpr.R(d), WA, ARM64Reg::WZR);
      gpr.Unlock(WA);
      break;
    }
    case CarryFlag::ConstantTrue:
    {
      gpr.SetImmediate(d, ~i + j + 1);
      break;
    }
    case CarryFlag::ConstantFalse:
    {
      gpr.SetImmediate(d, ~i + j);
      break;
    }
    }

    const bool must_have_carry = Interpreter::Helper_Carry(~i, j);
    const bool might_have_carry = (~i + j) == 0xFFFFFFFF;

    if (must_have_carry)
    {
      ComputeCarry(true);
    }
    else if (might_have_carry)
    {
      // carry stays as it is
    }
    else
    {
      ComputeCarry(false);
    }
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    ARM64Reg RB = mex ? gpr.GetReg() : gpr.R(b);
    if (mex)
      MOVI2R(RB, -1);

    if (js.carryFlag == CarryFlag::ConstantTrue)
    {
      CARRY_IF_NEEDED(SUB, SUBS, gpr.R(d), RB, gpr.R(a));
    }
    else
    {
      LoadCarry();
      CARRY_IF_NEEDED(SBC, SBCS, gpr.R(d), RB, gpr.R(a));
    }

    ComputeCarry();
    if (mex)
      gpr.Unlock(RB);
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
    CARRY_IF_NEEDED(SUB, SUBS, gpr.R(d), gpr.R(b), gpr.R(a));

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

  switch (js.carryFlag)
  {
  case CarryFlag::InPPCState:
  {
    ARM64Reg WA = gpr.GetReg();
    LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
    MVN(gpr.R(d), gpr.R(a));
    CARRY_IF_NEEDED(ADD, ADDS, gpr.R(d), gpr.R(d), WA);
    ComputeCarry();
    gpr.Unlock(WA);
    break;
  }
  case CarryFlag::InHostCarry:
  {
    CARRY_IF_NEEDED(SBC, SBCS, gpr.R(d), ARM64Reg::WZR, gpr.R(a));
    ComputeCarry();
    break;
  }
  case CarryFlag::ConstantTrue:
  {
    CARRY_IF_NEEDED(NEG, NEGS, gpr.R(d), gpr.R(a));
    ComputeCarry();
    break;
  }
  case CarryFlag::ConstantFalse:
  {
    MVN(gpr.R(d), gpr.R(a));
    ComputeCarry(false);
    break;
  }
  }

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
    const bool will_read = d == a;
    const bool is_zero = imm == 0;
    const bool allocate_reg = will_read && !is_zero;
    gpr.BindToRegister(d, will_read);

    // d = imm - a
    ARM64Reg RD = gpr.R(d);
    ARM64Reg WA = ARM64Reg::WZR;
    if (!is_zero)
    {
      WA = will_read ? gpr.GetReg() : RD;
      MOVI2R(WA, imm);
    }
    CARRY_IF_NEEDED(SUB, SUBS, RD, WA, gpr.R(a));

    if (allocate_reg)
      gpr.Unlock(WA);

    ComputeCarry();
  }
}

void JitArm64::addex(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITIntegerOff);
  FALLBACK_IF(inst.OE);

  bool mex = inst.SUBOP10 & 32;
  int a = inst.RA, b = inst.RB, d = inst.RD;

  if (gpr.IsImm(a) && (mex || gpr.IsImm(b)))
  {
    u32 i = gpr.GetImm(a), j = mex ? -1 : gpr.GetImm(b);

    gpr.BindToRegister(d, false);

    switch (js.carryFlag)
    {
    case CarryFlag::InPPCState:
    {
      ARM64Reg WA = gpr.GetReg();
      LDRB(IndexType::Unsigned, WA, PPC_REG, PPCSTATE_OFF(xer_ca));
      ADDI2R(gpr.R(d), WA, i + j, gpr.R(d));
      gpr.Unlock(WA);
      break;
    }
    case CarryFlag::InHostCarry:
    {
      ARM64Reg RD = gpr.R(d);
      MOVI2R(RD, i + j);
      ADC(RD, RD, ARM64Reg::WZR);
      break;
    }
    case CarryFlag::ConstantTrue:
    {
      gpr.SetImmediate(d, i + j + 1);
      break;
    }
    case CarryFlag::ConstantFalse:
    {
      gpr.SetImmediate(d, i + j);
      break;
    }
    }

    const bool must_have_carry = Interpreter::Helper_Carry(i, j);
    const bool might_have_carry = (i + j) == 0xFFFFFFFF;

    if (must_have_carry)
    {
      ComputeCarry(true);
    }
    else if (might_have_carry)
    {
      // carry stays as it is
    }
    else
    {
      ComputeCarry(false);
    }
  }
  else
  {
    gpr.BindToRegister(d, d == a || d == b);
    ARM64Reg RB = mex ? gpr.GetReg() : gpr.R(b);
    if (mex)
      MOVI2R(RB, -1);

    if (js.carryFlag == CarryFlag::ConstantFalse)
    {
      CARRY_IF_NEEDED(ADD, ADDS, gpr.R(d), gpr.R(a), RB);
    }
    else
    {
      LoadCarry();
      CARRY_IF_NEEDED(ADC, ADCS, gpr.R(d), gpr.R(a), RB);
    }

    ComputeCarry();
    if (mex)
      gpr.Unlock(RB);
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

  gpr.BindToRegister(d, d == a || d == b);
  CARRY_IF_NEEDED(ADD, ADDS, gpr.R(d), gpr.R(a), gpr.R(b));

  ComputeCarry();
  if (inst.Rc)
    ComputeRC0(gpr.R(d));
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
  else if (gpr.IsImm(b))
  {
    const u32 divisor = gpr.GetImm(b);

    if (divisor == 0)
    {
      gpr.SetImmediate(d, 0);
      if (inst.Rc)
        ComputeRC0(0);
    }
    else
    {
      const bool allocate_reg = d == a;
      gpr.BindToRegister(d, allocate_reg);

      ARM64Reg RD = gpr.R(d);
      ARM64Reg RA = gpr.R(a);

      if (MathUtil::IsPow2(divisor))
      {
        int shift = MathUtil::IntLog2(divisor);
        if (shift)
          LSR(RD, RA, shift);
        else if (d != a)
          MOV(RD, RA);
      }
      else
      {
        UnsignedMagic m = UnsignedDivisionConstants(divisor);

        ARM64Reg WI = allocate_reg ? gpr.GetReg() : RD;
        ARM64Reg XD = EncodeRegTo64(RD);

        MOVI2R(WI, m.multiplier);

        if (m.fast)
        {
          UMULL(XD, RA, WI);
        }
        else
        {
          UMADDL(XD, RA, WI, EncodeRegTo64(WI));
        }

        LSR(XD, XD, 32 + m.shift);

        if (allocate_reg)
          gpr.Unlock(WI);
      }

      if (inst.Rc)
        ComputeRC0(gpr.R(d));
    }
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
  else if (gpr.IsImm(a) && gpr.GetImm(a) == 0)
  {
    // Zero divided by anything is always zero
    gpr.SetImmediate(d, 0);
    if (inst.Rc)
      ComputeRC0(0);
  }
  else if (gpr.IsImm(a))
  {
    const u32 dividend = gpr.GetImm(a);

    gpr.BindToRegister(d, d == a || d == b);

    ARM64Reg RB = gpr.R(b);
    ARM64Reg RD = gpr.R(d);

    FixupBranch overflow1 = CBZ(RB);
    FixupBranch overflow2;
    if (dividend == 0x80000000)
    {
      CMN(RB, 1);
      overflow2 = B(CC_EQ);
    }
    SDIV(RD, gpr.R(a), RB);
    FixupBranch done = B();

    SetJumpTarget(overflow1);
    if (dividend == 0x80000000)
      SetJumpTarget(overflow2);

    MOVI2R(RD, dividend & 0x80000000 ? 0xFFFFFFFF : 0);

    SetJumpTarget(done);

    if (inst.Rc)
      ComputeRC0(RD);
  }
  else if (gpr.IsImm(b))
  {
    const s32 divisor = s32(gpr.GetImm(b));

    const bool allocate_reg = a == d;
    gpr.BindToRegister(d, allocate_reg);

    // Handle 0, 1, and -1 explicitly
    if (divisor == 0)
    {
      ASR(gpr.R(d), gpr.R(a), 31);
    }
    else if (divisor == 1)
    {
      if (d != a)
        MOV(gpr.R(d), gpr.R(a));
    }
    else if (divisor == -1)
    {
      // Rd = (Ra == 0x80000000) ? 0xFFFFFFFF : -Ra
      NEGS(gpr.R(d), gpr.R(a));
      CSINV(gpr.R(d), gpr.R(d), ARM64Reg::WZR, CCFlags::CC_VC);
    }
    else if (divisor == 2 || divisor == -2)
    {
      ARM64Reg RA = gpr.R(a);
      ARM64Reg RD = gpr.R(d);

      ADD(RD, RA, RA, ArithOption(RA, ShiftType::LSR, 31));

      if (divisor < 0)
        NEG(RD, RD, ArithOption(RD, ShiftType::ASR, 1));
      else
        ASR(RD, RD, 1);
    }
    else if (MathUtil::IsPow2(divisor) || MathUtil::IsPow2(-static_cast<s64>(divisor)))
    {
      const u32 abs_val = static_cast<u32>(std::abs(static_cast<s64>(divisor)));

      ARM64Reg RA = gpr.R(a);
      ARM64Reg RD = gpr.R(d);

      ARM64Reg WA = allocate_reg ? gpr.GetReg() : RD;

      TST(RA, RA);
      ADDI2R(WA, RA, abs_val - 1, WA);
      CSEL(WA, RA, WA, CCFlags::CC_PL);

      if (divisor < 0)
        NEG(RD, WA, ArithOption(WA, ShiftType::ASR, MathUtil::IntLog2(abs_val)));
      else
        ASR(RD, WA, MathUtil::IntLog2(abs_val));

      if (allocate_reg)
        gpr.Unlock(WA);
    }
    else
    {
      // Optimize signed 32-bit integer division by a constant
      SignedMagic m = SignedDivisionConstants(divisor);

      ARM64Reg RD = gpr.R(d);
      ARM64Reg WA = gpr.GetReg();
      ARM64Reg WB = allocate_reg ? gpr.GetReg() : RD;

      ARM64Reg XD = EncodeRegTo64(RD);
      ARM64Reg XA = EncodeRegTo64(WA);
      ARM64Reg XB = EncodeRegTo64(WB);

      SXTW(XA, gpr.R(a));
      MOVI2R(XB, s64(m.multiplier));

      if (divisor > 0 && m.multiplier < 0)
      {
        MUL(XD, XA, XB);
        ADD(XD, XA, XD, ArithOption(XD, ShiftType::LSR, 32));
        LSR(WA, WA, 31);
        ADD(RD, WA, RD, ArithOption(RD, ShiftType::ASR, m.shift));
      }
      else if (divisor < 0 && m.multiplier > 0)
      {
        MNEG(XD, XA, XB);
        ADD(XA, XD, XA, ArithOption(XA, ShiftType::LSR, 32));
        LSR(RD, WA, 31);
        ADD(RD, RD, WA, ArithOption(WA, ShiftType::ASR, m.shift));
      }
      else if (m.multiplier > 0)
      {
        MUL(XD, XA, XB);
        ASR(XD, XD, 32 + m.shift);
        ADD(RD, RD, WA, ArithOption(WA, ShiftType::LSR, 31));
      }
      else
      {
        MUL(XD, XA, XB);
        LSR(XA, XD, 63);
        ASR(XD, XD, 32 + m.shift);
        ADD(RD, WA, RD);
      }

      gpr.Unlock(WA);
      if (allocate_reg)
        gpr.Unlock(WB);
    }

    if (inst.Rc)
      ComputeRC0(gpr.R(d));
  }
  else
  {
    FlushCarry();

    gpr.BindToRegister(d, d == a || d == b);

    ARM64Reg RA = gpr.R(a);
    ARM64Reg RB = gpr.R(b);
    ARM64Reg RD = gpr.R(d);

    FixupBranch overflow1 = CBZ(RB);
    NEGS(ARM64Reg::WZR, RA);  // Is RA 0x80000000?
    CCMN(RB, 1, 0, CC_VS);    // Is RB -1?
    FixupBranch overflow2 = B(CC_EQ);
    SDIV(RD, RA, RB);
    FixupBranch done = B();

    SetJumpTarget(overflow1);
    SetJumpTarget(overflow2);

    ASR(RD, RA, 31);

    SetJumpTarget(done);

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
  else if (gpr.IsImm(s) && gpr.GetImm(s) == 0)
  {
    gpr.SetImmediate(a, 0);
    if (inst.Rc)
      ComputeRC0(0);
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

    // On PowerPC, shifting a 32-bit register by an amount from 32 to 63 results in 0.
    // We emulate this by using a 64-bit operation and then discarding the top 32 bits.
    LSLV(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(gpr.R(s)), EncodeRegTo64(gpr.R(b)));
    if (inst.Rc)
      ComputeRC0(gpr.R(a));
    MOV(gpr.R(a), gpr.R(a));
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
      ComputeCarry(amount != 0 && i < 0 && (u32(i) << (32 - amount)));
    }

    if (inst.Rc)
      ComputeRC0(gpr.GetImm(a));
    return;
  }
  else if (gpr.IsImm(s) && gpr.GetImm(s) == 0)
  {
    gpr.SetImmediate(a, 0);
    ComputeCarry(false);
    if (inst.Rc)
      ComputeRC0(0);
    return;
  }
  else if (gpr.IsImm(b))
  {
    int amount = gpr.GetImm(b);

    bool special = amount & 0x20;
    amount &= 0x1f;

    if (special)
    {
      gpr.BindToRegister(a, a == s);

      if (js.op->wantsCA)
      {
        // Set the carry flag to the sign bit of s
        CMN(gpr.R(s), gpr.R(s));
        ComputeCarry();
      }

      ASR(gpr.R(a), gpr.R(s), 31);
    }
    else if (amount == 0)
    {
      if (a != s)
      {
        gpr.BindToRegister(a, false);

        MOV(gpr.R(a), gpr.R(s));
      }

      ComputeCarry(false);
    }
    else if (!js.op->wantsCA)
    {
      gpr.BindToRegister(a, a == s);

      ASR(gpr.R(a), gpr.R(s), amount);
    }
    else
    {
      gpr.BindToRegister(a, a == s);

      ARM64Reg WA = gpr.GetReg();

      if (a != s)
      {
        ASR(gpr.R(a), gpr.R(s), amount);

        // To compute the PPC carry flag, we do the following:
        // 1. Take the bits which were shifted out, and create a temporary where they are in the
        //    most significant positions (followed by zeroes).
        // 2. Bitwise AND this temporary with the result of ASR. (Each bit which was shifted out
        //    gets ANDed with a copy of the sign bit.)
        // 3. Set the carry to the inverse of the Z flag. (The carry is set iff the sign bit was 1
        //    and at least one of the bits which were shifted out were 1.)
        TST(gpr.R(a), gpr.R(s), ArithOption(gpr.R(s), ShiftType::LSL, 32 - amount));
      }
      else
      {
        // TODO: If we implement register renaming, we can use the above approach for a == s too

        LSL(WA, gpr.R(s), 32 - amount);
        ASR(gpr.R(a), gpr.R(s), amount);
        TST(WA, gpr.R(a));
      }

      CSET(WA, CC_NEQ);
      ComputeCarry(WA);

      gpr.Unlock(WA);
    }
  }
  else
  {
    const bool will_read = a == b || a == s;
    gpr.BindToRegister(a, will_read);

    const bool allocate_reg = will_read || js.op->wantsCA;
    ARM64Reg WA = allocate_reg ? gpr.GetReg() : gpr.R(a);

    LSL(EncodeRegTo64(WA), EncodeRegTo64(gpr.R(s)), 32);
    ASRV(EncodeRegTo64(WA), EncodeRegTo64(WA), EncodeRegTo64(gpr.R(b)));
    LSR(EncodeRegTo64(gpr.R(a)), EncodeRegTo64(WA), 32);

    if (js.op->wantsCA)
    {
      TST(gpr.R(a), WA);
      CSET(WA, CC_NEQ);
      ComputeCarry(WA);
    }

    if (allocate_reg)
      gpr.Unlock(WA);
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

  const u32 lsb = 31 - inst.ME;
  const u32 width = inst.ME - inst.MB + 1;
  const u32 rot_dist = inst.SH ? 32 - inst.SH : 0;

  if (gpr.IsImm(a) && gpr.IsImm(s))
  {
    u32 res = (gpr.GetImm(a) & ~mask) | (std::rotl(gpr.GetImm(s), inst.SH) & mask);
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
        ROR(gpr.R(a), gpr.R(s), rot_dist);
      else if (a != s)
        MOV(gpr.R(a), gpr.R(s));
    }
    else if (lsb == 0 && inst.MB <= inst.ME && rot_dist + width <= 32)
    {
      // Destination is in least significant position
      // No mask inversion
      // Source field pre-rotation is contiguous
      gpr.BindToRegister(a, true);
      BFXIL(gpr.R(a), gpr.R(s), rot_dist, width);
    }
    else if (inst.SH == 0 && inst.MB <= inst.ME)
    {
      // No rotation
      // No mask inversion
      gpr.BindToRegister(a, true);
      ARM64Reg WA = gpr.GetReg();
      UBFX(WA, gpr.R(s), lsb, width);
      BFI(gpr.R(a), WA, lsb, width);
      gpr.Unlock(WA);
    }
    else if (inst.SH && inst.MB <= inst.ME)
    {
      // No mask inversion
      gpr.BindToRegister(a, true);
      if ((rot_dist + lsb) % 32 == 0)
      {
        BFI(gpr.R(a), gpr.R(s), lsb, width);
      }
      else
      {
        ARM64Reg WA = gpr.GetReg();
        ROR(WA, gpr.R(s), (rot_dist + lsb) % 32);
        BFI(gpr.R(a), WA, lsb, width);
        gpr.Unlock(WA);
      }
    }
    else
    {
      gpr.BindToRegister(a, true);
      const bool allocate_reg = a == s;
      ARM64Reg RA = gpr.R(a);
      ARM64Reg WA = gpr.GetReg();
      ARM64Reg WB = allocate_reg ? gpr.GetReg() : RA;

      MOVI2R(WA, mask);
      BIC(WB, RA, WA);
      AND(WA, WA, gpr.R(s), ArithOption(gpr.R(s), ShiftType::ROR, rot_dist));
      ORR(RA, WB, WA);

      gpr.Unlock(WA);
      if (allocate_reg)
        gpr.Unlock(WB);
    }

    if (inst.Rc)
      ComputeRC0(gpr.R(a));
  }
}
