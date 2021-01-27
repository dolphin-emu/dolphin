// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/PowerPC.h"

using namespace Arm64Gen;

void JitArm64::fp_arith(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;
  u32 op5 = inst.SUBOP5;

  bool single = inst.OPCD == 59;
  bool packed = inst.OPCD == 4;

  bool use_c = op5 >= 25;  // fmul and all kind of fmaddXX
  bool use_b = op5 != 25;  // fmul uses no B

  const auto inputs_are_singles_func = [&] {
    return fpr.IsSingle(a, !packed) && (!use_b || fpr.IsSingle(b, !packed)) &&
           (!use_c || fpr.IsSingle(c, !packed));
  };
  const bool inputs_are_singles = inputs_are_singles_func();

  ARM64Reg VA{}, VB{}, VC{}, VD{};

  if (packed)
  {
    const RegType type = inputs_are_singles ? RegType::Single : RegType::Register;
    const u8 size = inputs_are_singles ? 32 : 64;
    const auto reg_encoder = inputs_are_singles ? EncodeRegToDouble : EncodeRegToQuad;

    VA = reg_encoder(fpr.R(a, type));
    if (use_b)
      VB = reg_encoder(fpr.R(b, type));
    if (use_c)
      VC = reg_encoder(fpr.R(c, type));
    VD = reg_encoder(fpr.RW(d, type));

    switch (op5)
    {
    case 18:
      m_float_emit.FDIV(size, VD, VA, VB);
      break;
    case 20:
      m_float_emit.FSUB(size, VD, VA, VB);
      break;
    case 21:
      m_float_emit.FADD(size, VD, VA, VB);
      break;
    case 25:
      m_float_emit.FMUL(size, VD, VA, VC);
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "fp_arith");
      break;
    }
  }
  else
  {
    const RegType type =
        (inputs_are_singles && single) ? RegType::LowerPairSingle : RegType::LowerPair;
    const RegType type_out =
        single ? (inputs_are_singles ? RegType::DuplicatedSingle : RegType::Duplicated) :
                 RegType::LowerPair;
    const auto reg_encoder = (inputs_are_singles && single) ? EncodeRegToSingle : EncodeRegToDouble;

    VA = reg_encoder(fpr.R(a, type));
    if (use_b)
      VB = reg_encoder(fpr.R(b, type));
    if (use_c)
      VC = reg_encoder(fpr.R(c, type));
    VD = reg_encoder(fpr.RW(d, type_out));

    switch (op5)
    {
    case 18:
      m_float_emit.FDIV(VD, VA, VB);
      break;
    case 20:
      m_float_emit.FSUB(VD, VA, VB);
      break;
    case 21:
      m_float_emit.FADD(VD, VA, VB);
      break;
    case 25:
      m_float_emit.FMUL(VD, VA, VC);
      break;
    case 28:
      m_float_emit.FNMSUB(VD, VA, VC, VB);
      break;  // fmsub: "D = A*C - B" vs "Vd = (-Va) + Vn*Vm"
    case 29:
      m_float_emit.FMADD(VD, VA, VC, VB);
      break;  // fmadd: "D = A*C + B" vs "Vd = Va + Vn*Vm"
    case 30:
      m_float_emit.FMSUB(VD, VA, VC, VB);
      break;  // fnmsub: "D = -(A*C - B)" vs "Vd = Va + (-Vn)*Vm"
    case 31:
      m_float_emit.FNMADD(VD, VA, VC, VB);
      break;  // fnmadd: "D = -(A*C + B)" vs "Vd = (-Va) + (-Vn)*Vm"
    default:
      ASSERT_MSG(DYNA_REC, 0, "fp_arith");
      break;
    }
  }

  if (single || packed)
  {
    ASSERT_MSG(DYNA_REC, inputs_are_singles == inputs_are_singles_func(),
               "Register allocation turned singles into doubles in the middle of fp_arith");

    fpr.FixSinglePrecision(d);
  }
}

void JitArm64::fp_logic(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  const u32 b = inst.FB;
  const u32 d = inst.FD;
  const u32 op10 = inst.SUBOP10;

  bool packed = inst.OPCD == 4;

  // MR with source === dest => no-op
  if (op10 == 72 && b == d)
    return;

  const bool single = fpr.IsSingle(b, !packed);
  const u8 size = single ? 32 : 64;

  if (packed)
  {
    const RegType type = single ? RegType::Single : RegType::Register;
    const auto reg_encoder = single ? EncodeRegToDouble : EncodeRegToQuad;

    const ARM64Reg VB = reg_encoder(fpr.R(b, type));
    const ARM64Reg VD = reg_encoder(fpr.RW(d, type));

    switch (op10)
    {
    case 40:
      m_float_emit.FNEG(size, VD, VB);
      break;
    case 72:
      m_float_emit.ORR(VD, VB, VB);
      break;
    case 136:
      m_float_emit.FABS(size, VD, VB);
      m_float_emit.FNEG(size, VD, VD);
      break;
    case 264:
      m_float_emit.FABS(size, VD, VB);
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "fp_logic");
      break;
    }
  }
  else
  {
    const RegType type = single ? RegType::LowerPairSingle : RegType::LowerPair;
    const auto reg_encoder = single ? EncodeRegToSingle : EncodeRegToDouble;

    const ARM64Reg VB = fpr.R(b, type);
    const ARM64Reg VD = fpr.RW(d, type);

    switch (op10)
    {
    case 40:
      m_float_emit.FNEG(reg_encoder(VD), reg_encoder(VB));
      break;
    case 72:
      m_float_emit.INS(size, VD, 0, VB, 0);
      break;
    case 136:
      m_float_emit.FABS(reg_encoder(VD), reg_encoder(VB));
      m_float_emit.FNEG(reg_encoder(VD), reg_encoder(VD));
      break;
    case 264:
      m_float_emit.FABS(reg_encoder(VD), reg_encoder(VB));
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "fp_logic");
      break;
    }
  }

  ASSERT_MSG(DYNA_REC, single == fpr.IsSingle(b, !packed),
             "Register allocation turned singles into doubles in the middle of fp_logic");
}

void JitArm64::fselx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const u32 c = inst.FC;
  const u32 d = inst.FD;

  const bool a_single = fpr.IsSingle(a, true);
  if (a_single)
  {
    const ARM64Reg VA = fpr.R(a, RegType::LowerPairSingle);
    m_float_emit.FCMPE(EncodeRegToSingle(VA));
  }
  else
  {
    const ARM64Reg VA = fpr.R(a, RegType::LowerPair);
    m_float_emit.FCMPE(EncodeRegToDouble(VA));
  }

  ASSERT_MSG(DYNA_REC, a_single == fpr.IsSingle(a, true),
             "Register allocation turned singles into doubles in the middle of fselx");

  const bool b_and_c_singles = fpr.IsSingle(b, true) && fpr.IsSingle(c, true);
  const RegType type = b_and_c_singles ? RegType::LowerPairSingle : RegType::LowerPair;
  const auto reg_encoder = b_and_c_singles ? EncodeRegToSingle : EncodeRegToDouble;

  const ARM64Reg VB = fpr.R(b, type);
  const ARM64Reg VC = fpr.R(c, type);
  const ARM64Reg VD = fpr.RW(d, type);

  m_float_emit.FCSEL(reg_encoder(VD), reg_encoder(VC), reg_encoder(VB), CC_GE);

  ASSERT_MSG(DYNA_REC, b_and_c_singles == (fpr.IsSingle(b, true) && fpr.IsSingle(c, true)),
             "Register allocation turned singles into doubles in the middle of fselx");
}

void JitArm64::frspx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  const u32 b = inst.FB;
  const u32 d = inst.FD;

  const bool single = fpr.IsSingle(b, true);
  if (single)
  {
    // Source is already in single precision, so no need to do anything but to copy to PSR1.
    const ARM64Reg VB = fpr.R(b, RegType::LowerPairSingle);
    const ARM64Reg VD = fpr.RW(d, RegType::DuplicatedSingle);

    if (b != d)
      m_float_emit.FMOV(EncodeRegToSingle(VD), EncodeRegToSingle(VB));
  }
  else
  {
    const ARM64Reg VB = fpr.R(b, RegType::LowerPair);
    const ARM64Reg VD = fpr.RW(d, RegType::DuplicatedSingle);

    m_float_emit.FCVT(32, 64, EncodeRegToDouble(VD), EncodeRegToDouble(VB));
  }

  ASSERT_MSG(DYNA_REC, b == d || single == fpr.IsSingle(b, true),
             "Register allocation turned singles into doubles in the middle of frspx");
}

void JitArm64::fcmpX(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(SConfig::GetInstance().bFPRF && js.op->wantsFPRF);

  const u32 a = inst.FA;
  const u32 b = inst.FB;
  const int crf = inst.CRFD;

  const bool singles = fpr.IsSingle(a, true) && fpr.IsSingle(b, true);
  const RegType type = singles ? RegType::LowerPairSingle : RegType::LowerPair;
  const auto reg_encoder = singles ? EncodeRegToSingle : EncodeRegToDouble;

  const ARM64Reg VA = reg_encoder(fpr.R(a, type));
  const ARM64Reg VB = reg_encoder(fpr.R(b, type));

  gpr.BindCRToRegister(crf, false);
  const ARM64Reg XA = gpr.CR(crf);

  FixupBranch pNaN, pLesser, pGreater;
  FixupBranch continue1, continue2, continue3;
  ORR(XA, ZR, 32, 0, true);

  m_float_emit.FCMP(VA, VB);

  if (a != b)
  {
    // if B > A goto Greater's jump target
    pGreater = B(CC_GT);
    // if B < A, goto Lesser's jump target
    pLesser = B(CC_MI);
  }

  pNaN = B(CC_VS);

  // A == B
  ORR(XA, XA, 64 - 63, 0, true);
  continue1 = B();

  SetJumpTarget(pNaN);

  MOVI2R(XA, PowerPC::ConditionRegister::PPCToInternal(PowerPC::CR_SO));

  if (a != b)
  {
    continue2 = B();

    SetJumpTarget(pGreater);
    ORR(XA, XA, 0, 0, true);

    continue3 = B();

    SetJumpTarget(pLesser);
    ORR(XA, XA, 64 - 62, 1, true);
    ORR(XA, XA, 0, 0, true);

    SetJumpTarget(continue2);
    SetJumpTarget(continue3);
  }
  SetJumpTarget(continue1);

  ASSERT_MSG(DYNA_REC, singles == (fpr.IsSingle(a, true) && fpr.IsSingle(b, true)),
             "Register allocation turned singles into doubles in the middle of fcmpX");
}

void JitArm64::fctiwzx(UGeckoInstruction inst)
{
  INSTRUCTION_START
  JITDISABLE(bJITFloatingPointOff);
  FALLBACK_IF(inst.Rc);

  const u32 b = inst.FB;
  const u32 d = inst.FD;

  const bool single = fpr.IsSingle(b, true);

  const ARM64Reg VB = fpr.R(b, single ? RegType::LowerPairSingle : RegType::LowerPair);
  const ARM64Reg VD = fpr.RW(d, RegType::LowerPair);

  const ARM64Reg V0 = fpr.GetReg();

  // Generate 0xFFF8000000000000ULL
  m_float_emit.MOVI(64, EncodeRegToDouble(V0), 0xFFFF000000000000ULL);
  m_float_emit.BIC(16, EncodeRegToDouble(V0), 0x7);

  if (single)
  {
    m_float_emit.FCVTS(EncodeRegToSingle(VD), EncodeRegToSingle(VB), RoundingMode::Z);
  }
  else
  {
    const ARM64Reg V1 = gpr.GetReg();

    m_float_emit.FCVTS(V1, EncodeRegToDouble(VB), RoundingMode::Z);
    m_float_emit.FMOV(EncodeRegToSingle(VD), V1);

    gpr.Unlock(V1);
  }
  m_float_emit.ORR(EncodeRegToDouble(VD), EncodeRegToDouble(VD), EncodeRegToDouble(V0));
  fpr.Unlock(V0);

  ASSERT_MSG(DYNA_REC, b == d || single == fpr.IsSingle(b, true),
             "Register allocation turned singles into doubles in the middle of fctiwzx");
}
