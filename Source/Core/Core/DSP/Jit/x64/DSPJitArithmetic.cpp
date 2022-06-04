// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Jit/x64/DSPEmitter.h"

#include "Common/CommonTypes.h"
#include "Core/DSP/DSPCore.h"

using namespace Gen;

namespace DSP::JIT::x64
{
// CLR $acR
// 1000 r001 xxxx xxxx
// Clears accumulator $acR
//
// flags out: --10 0100
void DSPEmitter::clr(const UDSPInstruction opc)
{
  u8 reg = (opc >> 11) & 0x1;
  //	dsp_set_long_acc(reg, 0);
  XOR(32, R(EAX), R(EAX));
  set_long_acc(reg);
  //	Update_SR_Register64(0);
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// CLRL $acR.l
// 1111 110r xxxx xxxx
// Clears (and rounds!) $acR.l - low 16 bits of accumulator $acR.
//
// flags out: --xx xx00
void DSPEmitter::clrl(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  //	s64 acc = dsp_round_long_acc(dsp_get_long_acc(reg));
  get_long_acc(reg);
  round_long_acc();
  //	dsp_set_long_acc(reg, acc);
  set_long_acc(reg);
  //	Update_SR_Register64(acc);
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

//----

// ANDCF $acD.m, #I
// 0000 001d 1100 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logic AND of
// accumulator mid part $acD.m with immediate value I is equal to I.
//
// flags out: -x-- ----
void DSPEmitter::andcf(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    const u8 reg = (opc >> 8) & 0x1;
    // const u16 imm = m_dsp_core.DSPState().FetchInstruction();
    const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
    // const u16 val = GetAccMid(reg);
    X64Reg val = RAX;
    get_acc_m(reg, val);
    // UpdateSRLogicZero((val & imm) == imm);
    // if ((val & imm) == imm)
    //   g_dsp.r.sr |= SR_LOGIC_ZERO;
    // else
    //   g_dsp.r.sr &= ~SR_LOGIC_ZERO;
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    AND(16, R(val), Imm16(imm));
    CMP(16, R(val), Imm16(imm));
    FixupBranch notLogicZero = J_CC(CC_NE);
    OR(16, sr_reg, Imm16(SR_LOGIC_ZERO));
    FixupBranch exit = J();
    SetJumpTarget(notLogicZero);
    AND(16, sr_reg, Imm16(~SR_LOGIC_ZERO));
    SetJumpTarget(exit);
    m_gpr.PutReg(DSP_REG_SR);
  }
}

// ANDF $acD.m, #I
// 0000 001d 1010 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logical AND
// operation of accumulator mid part $acD.m with immediate value I is equal
// to immediate value 0.
//
// flags out: -x-- ----
void DSPEmitter::andf(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    const u8 reg = (opc >> 8) & 0x1;
    // const u16 imm = m_dsp_core.DSPState().FetchInstruction();
    const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
    // const u16 val = GetAccMid(reg);
    X64Reg val = RAX;
    get_acc_m(reg, val);
    // UpdateSRLogicZero((val & imm) == 0);
    // if ((val & imm) == 0)
    //   g_dsp.r.sr |= SR_LOGIC_ZERO;
    // else
    //   g_dsp.r.sr &= ~SR_LOGIC_ZERO;
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    TEST(16, R(val), Imm16(imm));
    FixupBranch notLogicZero = J_CC(CC_NE);
    OR(16, sr_reg, Imm16(SR_LOGIC_ZERO));
    FixupBranch exit = J();
    SetJumpTarget(notLogicZero);
    AND(16, sr_reg, Imm16(~SR_LOGIC_ZERO));
    SetJumpTarget(exit);
    m_gpr.PutReg(DSP_REG_SR);
  }
}

//----

// TST
// 1011 r001 xxxx xxxx
// Test accumulator $acR.
//
// flags out: --xx xx00
void DSPEmitter::tst(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 reg = (opc >> 11) & 0x1;
    //		s64 acc = dsp_get_long_acc(reg);
    get_long_acc(reg);
    //		Update_SR_Register64(acc);
    Update_SR_Register64();
  }
}

// TSTAXH $axR.h
// 1000 011r xxxx xxxx
// Test high part of secondary accumulator $axR.h.
//
// flags out: --x0 xx00
void DSPEmitter::tstaxh(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 reg = (opc >> 8) & 0x1;
    //		s16 val = dsp_get_ax_h(reg);
    get_ax_h(reg, EAX);
    //		Update_SR_Register16(val);
    Update_SR_Register16(EAX);
  }
}

//----

// CMP
// 1000 0010 xxxx xxxx
// Compares accumulator $ac0 with accumulator $ac1.
//
// flags out: x-xx xxxx
void DSPEmitter::cmp(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    // const s64 acc0 = GetLongAcc(0);
    X64Reg acc0 = RAX;
    get_long_acc(0, acc0);
    // const s64 acc1 = GetLongAcc(1);
    X64Reg acc1 = RDX;
    get_long_acc(1, acc1);
    // s64 res = dsp_convert_long_acc(acc0 - acc1);
    X64Reg res = RCX;
    MOV(64, R(res), R(acc0));
    SUB(64, R(res), R(acc1));
    dsp_convert_long_acc(RCX);

    // UpdateSR64Sub(acc0, acc1, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc0, acc1, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPAXH $acS, $axR.h
// 110r s001 xxxx xxxx
// Compares accumulator $acS with high part of secondary accumulator $axR.h.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpaxh(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 rreg = ((opc >> 12) & 0x1);
    u8 sreg = (opc >> 11) & 0x1;

    // const s64 acc = GetLongAcc(sreg);
    X64Reg acc = RAX;
    get_long_acc(sreg, acc);
    // s64 ax = GetAXHigh(rreg);
    X64Reg ax = RDX;
    get_ax_h(rreg, ax);
    // ax <<= 16;
    SHL(64, R(ax), Imm8(16));
    // const s64 res = dsp_convert_long_acc(acc - ax);
    X64Reg res = RCX;
    MOV(64, R(res), R(acc));
    SUB(64, R(res), R(ax));
    dsp_convert_long_acc(res);
    // UpdateSR64Sub(acc, ax, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, ax, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPI $acD, #I
// 0000 001d 1000 0000
// iiii iiii iiii iiii
// Compares accumulator with immediate. Comparison is executed
// by subtracting the immediate (16-bit sign extended) from mid accumulator
// $acD.hm and computing flags based on whole accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpi(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    const u8 reg = (opc >> 8) & 0x1;
    // const s64 val = GetLongAcc(reg);
    X64Reg val = RAX;
    get_long_acc(reg, val);
    // Immediate is considered to be at M level in the 40-bit accumulator.
    // s64 imm = static_cast<s16>(state.FetchInstruction());
    // imm <<= 16;
    X64Reg imm_reg = RDX;
    s64 imm = static_cast<s16>(m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1));
    imm <<= 16;
    MOV(64, R(imm_reg), Imm64(imm));
    // const s64 res = dsp_convert_long_acc(val - imm);
    X64Reg res = RCX;
    MOV(64, R(res), R(val));
    SUB(64, R(res), R(imm_reg));
    dsp_convert_long_acc(res);
    // UpdateSR64Sub(val, imm, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(val, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPIS $acD, #I
// 0000 011d iiii iiii
// Compares accumulator with short immediate. Comparison is executed
// by subtracting the short immediate (8-bit sign extended) from mid accumulator
// $acD.hm and computing flags based on whole accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpis(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 areg = (opc >> 8) & 0x1;
    // const s64 acc = GetLongAcc(areg);
    X64Reg acc = RAX;
    get_long_acc(areg, acc);
    // s64 imm = static_cast<s8>(opc);
    // imm <<= 16;
    X64Reg imm_reg = RDX;
    s64 imm = static_cast<s8>(opc);
    imm <<= 16;
    MOV(64, R(imm_reg), Imm64(imm));
    // const s64 res = dsp_convert_long_acc(acc - imm);
    X64Reg res = RCX;
    MOV(64, R(res), R(acc));
    SUB(64, R(res), R(imm_reg));
    dsp_convert_long_acc(res);
    // UpdateSR64Sub(acc, imm, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

//----

// XORR $acD.m, $axS.h
// 0011 00sd 0xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with
// high part of secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::xorr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] ^ g_dsp.r.axh[sreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_ax_h(sreg, RDX);
  XOR(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ANDR $acD.m, $axS.h
// 0011 01sd 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::andr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] & g_dsp.r.axh[sreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_ax_h(sreg, RDX);
  AND(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ORR $acD.m, $axS.h
// 0011 10sd 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with high part of
// secondary accumulator $axS.h.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::orr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] | g_dsp.r.axh[sreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_ax_h(sreg, RDX);
  OR(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ANDC $acD.m, $ac(1-D).m
// 0011 110d 0xxx xxxx
// Logic AND middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::andc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] & g_dsp.r.acm[1 - dreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_acc_m(1 - dreg, RDX);
  AND(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ORC $acD.m, $ac(1-D).m
// 0011 111d 0xxx xxxx
// Logic OR middle part of accumulator $acD.m with middle part of
// accumulator $ac(1-D).m.
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::orc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] | g_dsp.r.acm[1 - dreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_acc_m(1 - dreg, RDX);
  OR(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// XORC $acD.m
// 0011 000d 1xxx xxxx
// Logic XOR (exclusive or) middle part of accumulator $acD.m with $ac(1-D).m
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::xorc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] ^ g_dsp.r.acm[1 - dreg];
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  get_acc_m(1 - dreg, RDX);
  XOR(64, R(accm), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// NOT $acD.m
// 0011 001d 1xxx xxxx
// Invert all bits in dest reg, aka xor with 0xffff
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::notc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	u16 accm = g_dsp.r.acm[dreg] ^ 0xffff;
  X64Reg accm = RAX;
  get_acc_m(dreg, accm);
  NOT(16, R(accm));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg, R(accm));
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(dreg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// XORI $acD.m, #I
// 0000 001d 0010 0000
// iiii iiii iiii iiii
// Logic exclusive or (XOR) of accumulator mid part $acD.m with
// immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::xori(const UDSPInstruction opc)
{
  const u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] ^= imm;
  X64Reg accm = RAX;
  get_acc_m(reg, accm);
  XOR(16, R(accm), Imm16(imm));
  set_acc_m(reg, R(accm));
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(reg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ANDI $acD.m, #I
// 0000 001d 0100 0000
// iiii iiii iiii iiii
// Logic AND of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::andi(const UDSPInstruction opc)
{
  const u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] &= imm;
  X64Reg accm = RAX;
  get_acc_m(reg, accm);
  AND(16, R(accm), Imm16(imm));
  set_acc_m(reg, R(accm));
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(reg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

// ORI $acD.m, #I
// 0000 001d 0110 0000
// iiii iiii iiii iiii
// Logic OR of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::ori(const UDSPInstruction opc)
{
  const u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] |= imm;
  X64Reg accm = RAX;
  get_acc_m(reg, accm);
  OR(16, R(accm), Imm16(imm));
  set_acc_m(reg, R(accm));
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    X64Reg acc_full = RCX;
    get_long_acc(reg, acc_full);
    Update_SR_Register16_OverS32(accm, acc_full, RDX);
  }
}

//----

// ADDR $acD.M, $axS.L
// 0100 0ssd xxxx xxxx
// Adds register $axS.L to accumulator $acD.M register.
//
// flags out: x-xx xxxx
void DSPEmitter::addr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // s64 ax = ...;
  X64Reg ax = RDX;
  dsp_op_read_reg(sreg, ax, RegisterExtension::Sign);
  // ax <<= 16;
  SHL(64, R(ax), Imm8(16));
  // const s64 res = acc + ax;
  X64Reg res = RCX;
  LEA(64, res, MRegSum(acc, ax));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, ax, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, ax, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADDAX $acD, $axS
// 0100 10sd xxxx xxxx
// Adds secondary accumulator $axS to accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::addax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 ax = GetLongACX(sreg);
  X64Reg ax = RDX;
  get_long_acx(sreg, ax);
  // const s64 res = acc + ax;
  X64Reg res = RCX;
  LEA(64, res, MRegSum(acc, ax));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, ax, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, ax, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADD $acD, $ac(1-D)
// 0100 110d xxxx xxxx
// Adds accumulator $ac(1-D) to accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::add(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  // const s64 acc0 = GetLongAcc(dreg);
  X64Reg acc0 = RAX;
  get_long_acc(dreg, acc0);
  // const s64 acc1 = GetLongAcc(1 - dreg);
  X64Reg acc1 = RDX;
  get_long_acc(1 - dreg, acc1);
  // const s64 res = acc0 + acc1;
  X64Reg res = RCX;
  LEA(64, res, MRegSum(acc0, acc1));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc0, acc1, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc0, acc1, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADDP $acD
// 0100 111d xxxx xxxx
// Adds product register to accumulator register.
//
// flags out: x-xx xxxx
void DSPEmitter::addp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 prod = GetLongProduct();
  X64Reg prod = RDX;
  get_long_prod(prod);
  // const s64 res = acc + prod;
  X64Reg res = RCX;
  LEA(64, res, MRegSum(acc, prod));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, prod, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, prod, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADDAXL $acD, $axS.l
// 0111 00sd xxxx xxxx
// Adds secondary accumulator $axS.l to accumulator register $acD.
// should be unsigned values!!
//
// flags out: x-xx xxxx
void DSPEmitter::addaxl(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 9) & 0x1;
  u8 dreg = (opc >> 8) & 0x1;

  // const u64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const u16 acx = static_cast<u16>(GetAXLow(sreg));
  X64Reg acx = RDX;
  get_ax_l(sreg, acx);
  MOVZX(64, 16, acx, R(acx));
  // const u64 res = acc + acx;
  X64Reg res = RCX;
  LEA(64, res, MRegSum(acc, acx));
  // SetLongAcc(dreg, static_cast<s64>(res));
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, acx, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, acx, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADDI $acD, #I
// 0000 001d 0000 0000
// iiii iiii iiii iiii
// Adds immediate (16-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void DSPEmitter::addi(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;
  // const s64 acc = GetLongAcc(areg);
  X64Reg acc = RAX;
  get_long_acc(areg, acc);
  // s64 imm = static_cast<s16>(state.FetchInstruction());
  // imm <<= 16;
  s64 imm = static_cast<s16>(m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1));
  imm <<= 16;
  // const s64 res = acc + imm;
  X64Reg res = RCX;
  // Can safely use LEA as we are using a 16-bit sign-extended immediate shifted left by 16, which
  // fits in a signed 32-bit immediate
  LEA(64, res, MDisp(acc, static_cast<s32>(imm)));
  // SetLongAcc(areg, res);
  set_long_acc(areg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, imm, GetLongAcc(areg));
    get_long_acc(areg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(imm_reg), Imm64(imm));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ADDIS $acD, #I
// 0000 010d iiii iiii
// Adds short immediate (8-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void DSPEmitter::addis(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // s64 imm = static_cast<s8>(opc);
  // imm <<= 16;
  s64 imm = static_cast<s8>(opc);
  imm <<= 16;
  // const s64 res = acc + imm;
  X64Reg res = RCX;
  LEA(64, res, MDisp(acc, static_cast<s32>(imm)));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, imm, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(imm_reg), Imm64(imm));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// INCM $acsD
// 0111 010d xxxx xxxx
// Increment 24-bit mid-accumulator $acsD.
//
// flags out: x-xx xxxx
void DSPEmitter::incm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  s64 subtract = 0x10000;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 res = acc + sub;
  X64Reg res = RCX;
  LEA(64, res, MDisp(acc, static_cast<s32>(subtract)));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, sub, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(imm_reg), Imm64(subtract));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// INC $acD
// 0111 011d xxxx xxxx
// Increment accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::inc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 res = acc + 1;
  X64Reg res = RCX;
  LEA(64, res, MDisp(acc, 1));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Add(acc, 1, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(imm_reg), Imm64(1));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Add(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

//----

// SUBR $acD.M, $axS.L
// 0101 0ssd xxxx xxxx
// Subtracts register $axS.L from accumulator $acD.M register.
//
// flags out: x-xx xxxx
void DSPEmitter::subr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // s64 ax = ...;
  X64Reg ax = RDX;
  dsp_op_read_reg(sreg, ax, RegisterExtension::Sign);
  // ax <<= 16;
  SHL(64, R(ax), Imm8(16));
  // const s64 res = acc - ax;
  X64Reg res = RCX;
  MOV(64, R(res), R(acc));
  SUB(64, R(res), R(ax));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc, ax, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, ax, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// SUBAX $acD, $axS
// 0101 10sd xxxx xxxx
// Subtracts secondary accumulator $axS from accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::subax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 acx = GetLongACX(sreg);
  X64Reg acx = RDX;
  get_long_acx(sreg, acx);
  // const s64 res = acc - acx;
  X64Reg res = RCX;
  MOV(64, R(res), R(acc));
  SUB(64, R(res), R(acx));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc, acx, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, acx, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// SUB $acD, $ac(1-D)
// 0101 110d xxxx xxxx
// Subtracts accumulator $ac(1-D) from accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::sub(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  // const s64 acc1 = GetLongAcc(dreg);
  X64Reg acc1 = RAX;
  get_long_acc(dreg, acc1);
  // const s64 acc2 = GetLongAcc(1 - dreg);
  X64Reg acc2 = RDX;
  get_long_acc(1 - dreg, acc2);
  // const s64 res = acc1 - acc2;
  X64Reg res = RCX;
  MOV(64, R(res), R(acc1));
  SUB(64, R(res), R(acc2));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc1, acc2, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc1, acc2, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// SUBP $acD
// 0101 111d xxxx xxxx
// Subtracts product register from accumulator register.
//
// flags out: x-xx xxxx
void DSPEmitter::subp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 prod = GetLongProduct();
  X64Reg prod = RDX;
  get_long_prod(prod);
  // const s64 res = acc - prod;
  X64Reg res = RCX;
  MOV(64, R(res), R(acc));
  SUB(64, R(res), R(prod));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc, prod, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, prod, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// DECM $acsD
// 0111 100d xxxx xxxx
// Decrement 24-bit mid-accumulator $acsD.
//
// flags out: x-xx xxxx
void DSPEmitter::decm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;
  s64 subtract = 0x10000;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 res = acc - sub;
  X64Reg res = RCX;
  LEA(64, res, MDisp(acc, -subtract));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc, sub, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(imm_reg), Imm64(subtract));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// DEC $acD
// 0111 101d xxxx xxxx
// Decrement accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::dec(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 res = acc - 1;
  X64Reg res = RCX;
  LEA(64, res, MDisp(acc, -1));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(acc, 1, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    MOV(64, R(RDX), Imm64(1));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(acc, imm_reg, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

//----

// NEG $acD
// 0111 110d xxxx xxxx
// Negate accumulator $acD.
//
// flags out: x-xx xxxx
//
// The carry flag is set only if $acD was zero.
// The overflow flag is set only if $acD was 0x8000000000 (the minimum value),
// as -INT_MIN is INT_MIN in two's complement. In both of these cases,
// the value of $acD after the operation is the same as it was before.
void DSPEmitter::neg(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  // const s64 acc = GetLongAcc(dreg);
  X64Reg acc = RAX;
  get_long_acc(dreg, acc);
  // const s64 res = 0 - acc;
  X64Reg res = RCX;
  MOV(64, R(res), R(acc));
  NEG(64, R(res));
  // SetLongAcc(dreg, res);
  set_long_acc(dreg, res);
  if (FlagsNeeded())
  {
    // UpdateSR64Sub(0, acc, GetLongAcc(dreg));
    get_long_acc(dreg, res);
    X64Reg imm_reg = RDX;
    XOR(64, R(imm_reg), R(imm_reg));
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    UpdateSR64Sub(imm_reg, acc, res, tmp1);
    m_gpr.PutXReg(tmp1);
  }
}

// ABS  $acD
// 1010 d001 xxxx xxxx
// absolute value of $acD
//
// flags out: --xx xx00
void DSPEmitter::abs(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 11) & 0x1;

  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg);
  //	if (acc < 0) acc = 0 - acc;
  TEST(64, R(RAX), R(RAX));
  FixupBranch GreaterThanOrEqual = J_CC(CC_GE);
  NEG(64, R(RAX));
  set_long_acc(dreg);
  SetJumpTarget(GreaterThanOrEqual);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}
//----

// MOVR $acD, $axS.R
// 0110 0srd xxxx xxxx
// Moves register $axS.R (sign extended) to middle accumulator $acD.hm.
// Sets $acD.l to 0.
// TODO: Check what happens to acD.h.
//
// flags out: --xx xx00
void DSPEmitter::movr(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;
  u8 sreg = ((opc >> 9) & 0x3) + DSP_REG_AXL0;

  //	s64 acc = (s16)g_dsp.r[sreg];
  dsp_op_read_reg(sreg, RAX, RegisterExtension::Sign);
  //	acc <<= 16;
  SHL(64, R(RAX), Imm8(16));
  //	acc &= ~0xffff;
  //	dsp_set_long_acc(areg, acc);
  set_long_acc(areg);
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// MOVAX $acD, $axS
// 0110 10sd xxxx xxxx
// Moves secondary accumulator $axS to accumulator $acD.
//
// flags out: --xx xx00
void DSPEmitter::movax(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  //	s64 acx = dsp_get_long_acx(sreg);
  get_long_acx(sreg);
  //	dsp_set_long_acc(dreg, acx);
  set_long_acc(dreg);
  //	Update_SR_Register64(acx);
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// MOV $acD, $ac(1-D)
// 0110 110d xxxx xxxx
// Moves accumulator $ac(1-D) to accumulator $acD.
//
// flags out: --x0 xx00
void DSPEmitter::mov(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	u64 acc = dsp_get_long_acc(1 - dreg);
  get_long_acc(1 - dreg);
  //	dsp_set_long_acc(dreg, acc);
  set_long_acc(dreg);
  //	Update_SR_Register64(acc);
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

//----

// LSL16 $acR
// 1111 000r xxxx xxxx
// Logically shifts left accumulator $acR by 16.
//
// flags out: --xx xx00
void DSPEmitter::lsl16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;
  //	s64 acc = dsp_get_long_acc(areg);
  get_long_acc(areg);
  //	acc <<= 16;
  SHL(64, R(RAX), Imm8(16));
  //	dsp_set_long_acc(areg, acc);
  set_long_acc(areg);
  //	Update_SR_Register64(dsp_get_long_acc(areg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// LSR16 $acR
// 1111 010r xxxx xxxx
// Logically shifts right accumulator $acR by 16.
//
// flags out: --xx xx00
void DSPEmitter::lsr16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;

  //	u64 acc = dsp_get_long_acc(areg);
  get_long_acc(areg);
  //	acc &= 0x000000FFFFFFFFFFULL; 	// Lop off the extraneous sign extension our 64-bit fake accum
  // causes
  //	acc >>= 16;
  SHR(64, R(RAX), Imm8(16));
  AND(32, R(EAX), Imm32(0xffffff));
  //	dsp_set_long_acc(areg, (s64)acc);
  set_long_acc(areg);
  //	Update_SR_Register64(dsp_get_long_acc(areg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// ASR16 $acR
// 1001 r001 xxxx xxxx
// Arithmetically shifts right accumulator $acR by 16.
//
// flags out: --xx xx00
void DSPEmitter::asr16(const UDSPInstruction opc)
{
  u8 areg = (opc >> 11) & 0x1;

  //	s64 acc = dsp_get_long_acc(areg);
  get_long_acc(areg);
  //	acc >>= 16;
  SAR(64, R(RAX), Imm8(16));
  //	dsp_set_long_acc(areg, acc);
  set_long_acc(areg);
  //	Update_SR_Register64(dsp_get_long_acc(areg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// LSL $acR, #I
// 0001 010r 00ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
//
// flags out: --xx xx00
void DSPEmitter::lsl(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift = opc & 0x3f;
  //	u64 acc = dsp_get_long_acc(rreg);
  get_long_acc(rreg);

  //	acc <<= shift;
  SHL(64, R(RAX), Imm8((u8)shift));

  //	dsp_set_long_acc(rreg, acc);
  set_long_acc(rreg);
  //	Update_SR_Register64(dsp_get_long_acc(rreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// LSR $acR, #I
// 0001 010r 01ii iiii
// Logically shifts right accumulator $acR by number specified by value
// calculated by negating sign extended bits 0-6.
//
// flags out: --xx xx00
void DSPEmitter::lsr(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift;
  //	u64 acc = dsp_get_long_acc(rreg);
  get_long_acc(rreg);

  if ((opc & 0x3f) == 0)
    shift = 0;
  else
    shift = 0x40 - (opc & 0x3f);

  if (shift)
  {
    //	acc &= 0x000000FFFFFFFFFFULL; 	// Lop off the extraneous sign extension our 64-bit fake
    // accum causes
    SHL(64, R(RAX), Imm8(24));
    //	acc >>= shift;
    SHR(64, R(RAX), Imm8(shift + 24));
  }

  //	dsp_set_long_acc(rreg, (s64)acc);
  set_long_acc(rreg);
  //	Update_SR_Register64(dsp_get_long_acc(rreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// ASL $acR, #I
// 0001 010r 10ii iiii
// Logically shifts left accumulator $acR by number specified by value I.
//
// flags out: --xx xx00
void DSPEmitter::asl(const UDSPInstruction opc)
{
  u8 rreg = (opc >> 8) & 0x01;
  u16 shift = opc & 0x3f;
  //	u64 acc = dsp_get_long_acc(rreg);
  get_long_acc(rreg);
  //	acc <<= shift;
  SHL(64, R(RAX), Imm8((u8)shift));
  //	dsp_set_long_acc(rreg, acc);
  set_long_acc(rreg);
  //	Update_SR_Register64(dsp_get_long_acc(rreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// ASR $acR, #I
// 0001 010r 11ii iiii
// Arithmetically shifts right accumulator $acR by number specified by
// value calculated by negating sign extended bits 0-6.
//
// flags out: --xx xx00
void DSPEmitter::asr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;
  u16 shift;

  if ((opc & 0x3f) == 0)
    shift = 0;
  else
    shift = 0x40 - (opc & 0x3f);

  // arithmetic shift
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg);
  //	acc >>= shift;
  SAR(64, R(RAX), Imm8((u8)shift));

  //	dsp_set_long_acc(dreg, acc);
  set_long_acc(dreg);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
  }
}

// LSRN  (fixed parameters)
// 0000 0010 1100 1010
// Logically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M
// (if value negative, becomes left shift).
//
// flags out: --xx xx00
void DSPEmitter::lsrn(const UDSPInstruction opc)
{
  //	s16 shift;
  //	u16 accm = (u16)dsp_get_acc_m(1);
  get_acc_m(1);
  //	u64 acc = dsp_get_long_acc(0);
  get_long_acc(0, RDX);
  //	acc &= 0x000000FFFFFFFFFFULL;
  SHL(64, R(RDX), Imm8(24));
  SHR(64, R(RDX), Imm8(24));

  //	if ((accm & 0x3f) == 0)
  //		shift = 0;
  //	else if (accm & 0x40)
  //		shift = -0x40 + (accm & 0x3f);
  //	else
  //		shift = accm & 0x3f;

  //	if (shift > 0)
  //	{
  //		acc >>= shift;
  //	}
  //	else if (shift < 0)
  //	{
  //		acc <<= -shift;
  //	}

  TEST(64, R(RDX), R(RDX));  // is this actually worth the branch cost?
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));  // is this actually worth the branch cost?
  FixupBranch noShift = J_CC(CC_Z);
  // CL gets automatically masked with 0x3f on IA32/AMD64
  // MOVZX(64, 16, RCX, R(RAX));
  MOV(64, R(RCX), R(RAX));
  // AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  // ADD(16, R(RCX), Imm16(0x40));
  SHL(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SHR(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(0, (s64)acc);
  set_long_acc(0, RDX);
  SetJumpTarget(zero);
  //	Update_SR_Register64(dsp_get_long_acc(0));
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

// ASRN  (fixed parameters)
// 0000 0010 1100 1011
// Arithmetically shifts right accumulator $ACC0 by lower 7-bit (signed) value in $AC1.M
// (if value negative, becomes left shift).
//
// flags out: --xx xx00
void DSPEmitter::asrn(const UDSPInstruction opc)
{
  //	s16 shift;
  //	u16 accm = (u16)dsp_get_acc_m(1);
  get_acc_m(1);
  //	s64 acc = dsp_get_long_acc(0);
  get_long_acc(0, RDX);

  //	if ((accm & 0x3f) == 0)
  //		shift = 0;
  //	else if (accm & 0x40)
  //		shift = -0x40 + (accm & 0x3f);
  //	else
  //		shift = accm & 0x3f;

  //	if (shift > 0)
  //	{
  //		acc >>= shift;
  //	}
  //	else if (shift < 0)
  //	{
  //		acc <<= -shift;
  //	}

  TEST(64, R(RDX), R(RDX));
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));
  FixupBranch noShift = J_CC(CC_Z);
  MOVZX(64, 16, RCX, R(RAX));
  AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  ADD(16, R(RCX), Imm16(0x40));
  SHL(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SAR(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(0, acc);
  //	Update_SR_Register64(dsp_get_long_acc(0));
  set_long_acc(0, RDX);
  SetJumpTarget(zero);
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

// LSRNRX $acD, $axS.h
// 0011 01sd 1xxx xxxx
// Logically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AX[S].H
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::lsrnrx(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  //	s16 shift;
  //	u16 axh = g_dsp.r.axh[sreg];
  get_ax_h(sreg);
  //	u64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, RDX);
  //	acc &= 0x000000FFFFFFFFFFULL;
  SHL(64, R(RDX), Imm8(24));
  SHR(64, R(RDX), Imm8(24));

  //	if ((axh & 0x3f) == 0)
  //		shift = 0;
  //	else if (axh & 0x40)
  //		shift = -0x40 + (axh & 0x3f);
  //	else
  //		shift = axh & 0x3f;

  //	if (shift > 0)
  //	{
  //		acc <<= shift;
  //	}
  //	else if (shift < 0)
  //	{
  //		acc >>= -shift;
  //	}

  TEST(64, R(RDX), R(RDX));
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));
  FixupBranch noShift = J_CC(CC_Z);
  MOVZX(64, 16, RCX, R(RAX));
  AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  ADD(16, R(RCX), Imm16(0x40));
  SHR(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SHL(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(dreg, (s64)acc);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  set_long_acc(dreg, RDX);
  SetJumpTarget(zero);
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

// ASRNRX $acD, $axS.h
// 0011 10sd 1xxx xxxx
// Arithmetically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AX[S].H
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::asrnrx(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  u8 sreg = (opc >> 9) & 0x1;

  //	s16 shift;
  //	u16 axh = g_dsp.r.axh[sreg];
  get_ax_h(sreg);
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, RDX);

  //	if ((axh & 0x3f) == 0)
  //		shift = 0;
  //	else if (axh & 0x40)
  //		shift = -0x40 + (axh & 0x3f);
  //	else
  //		shift = axh & 0x3f;

  //	if (shift > 0) {
  //		acc <<= shift;
  //	} else if (shift < 0) {
  //		acc >>= -shift;
  //	}

  TEST(64, R(RDX), R(RDX));
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));
  FixupBranch noShift = J_CC(CC_Z);
  MOVZX(64, 16, RCX, R(RAX));
  AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  ADD(16, R(RCX), Imm16(0x40));
  SAR(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SHL(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(dreg, acc);
  set_long_acc(dreg, RDX);
  SetJumpTarget(zero);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

// LSRNR  $acD
// 0011 110d 1xxx xxxx
// Logically shifts left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AC[1-D].M
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::lsrnr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  //	s16 shift;
  //	u16 accm = (u16)dsp_get_acc_m(1 - dreg);
  get_acc_m(1 - dreg);
  //	u64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, RDX);
  //	acc &= 0x000000FFFFFFFFFFULL;
  SHL(64, R(RDX), Imm8(24));
  SHR(64, R(RDX), Imm8(24));

  //	if ((accm & 0x3f) == 0)
  //		shift = 0;
  //	else if (accm & 0x40)
  //		shift = -0x40 + (accm & 0x3f);
  //	else
  //		shift = accm & 0x3f;

  //	if (shift > 0)
  //		acc <<= shift;
  //	else if (shift < 0)
  //		acc >>= -shift;

  TEST(64, R(RDX), R(RDX));
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));
  FixupBranch noShift = J_CC(CC_Z);
  MOVZX(64, 16, RCX, R(RAX));
  AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  ADD(16, R(RCX), Imm16(0x40));
  SHR(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SHL(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(dreg, (s64)acc);
  set_long_acc(dreg, RDX);
  SetJumpTarget(zero);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

// ASRNR  $acD
// 0011 111d 1xxx xxxx
// Arithmetically shift left/right accumulator $ACC[D] by lower 7-bit (signed) value in $AC[1-D].M
// x = extension (7 bits!!)
//
// flags out: --xx xx00
void DSPEmitter::asrnr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  //	s16 shift;
  //	u16 accm = (u16)dsp_get_acc_m(1 - dreg);
  get_acc_m(1 - dreg);
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, RDX);

  //	if ((accm & 0x3f) == 0)
  //		shift = 0;
  //	else if (accm & 0x40)
  //		shift = -0x40 + (accm & 0x3f);
  //	else
  //		shift = accm & 0x3f;

  //	if (shift > 0)
  //		acc <<= shift;
  //	else if (shift < 0)
  //		acc >>= -shift;

  TEST(64, R(RDX), R(RDX));
  FixupBranch zero = J_CC(CC_E);
  TEST(16, R(RAX), Imm16(0x3f));
  FixupBranch noShift = J_CC(CC_Z);
  MOVZX(64, 16, RCX, R(RAX));
  AND(16, R(RCX), Imm16(0x3f));
  TEST(16, R(RAX), Imm16(0x40));
  FixupBranch shiftLeft = J_CC(CC_Z);
  NEG(16, R(RCX));
  ADD(16, R(RCX), Imm16(0x40));
  SAR(64, R(RDX), R(RCX));
  FixupBranch exit = J();
  SetJumpTarget(shiftLeft);
  SHL(64, R(RDX), R(RCX));
  SetJumpTarget(noShift);
  SetJumpTarget(exit);

  //	dsp_set_long_acc(dreg, acc);
  set_long_acc(dreg, RDX);
  SetJumpTarget(zero);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64(RDX, RCX);
  }
}

}  // namespace DSP::JIT::x64
