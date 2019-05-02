// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/Jit/x64/DSPEmitter.h"

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
// 0000 001r 1100 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logic AND of
// accumulator mid part $acD.m with immediate value I is equal I.
//
// flags out: -x-- ----
void DSPEmitter::andcf(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 reg = (opc >> 8) & 0x1;
    //		u16 imm = dsp_fetch_code();
    u16 imm = dsp_imem_read(m_compile_pc + 1);
    //		u16 val = dsp_get_acc_m(reg);
    get_acc_m(reg);
    //		Update_SR_LZ(((val & imm) == imm) ? true : false);
    //		if ((val & imm) == imm)
    //			g_dsp.r.sr |= SR_LOGIC_ZERO;
    //		else
    //			g_dsp.r.sr &= ~SR_LOGIC_ZERO;
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    AND(16, R(RAX), Imm16(imm));
    CMP(16, R(RAX), Imm16(imm));
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
// 0000 001r 1010 0000
// iiii iiii iiii iiii
// Set logic zero (LZ) flag in status register $sr if result of logical AND
// operation of accumulator mid part $acD.m with immediate value I is equal
// immediate value 0.
//
// flags out: -x-- ----
void DSPEmitter::andf(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 reg = (opc >> 8) & 0x1;
    //		u16 imm = dsp_fetch_code();
    u16 imm = dsp_imem_read(m_compile_pc + 1);
    //		u16 val = dsp_get_acc_m(reg);
    get_acc_m(reg);
    //		Update_SR_LZ(((val & imm) == 0) ? true : false);
    //		if ((val & imm) == 0)
    //			g_dsp.r.sr |= SR_LOGIC_ZERO;
    //		else
    //			g_dsp.r.sr &= ~SR_LOGIC_ZERO;
    const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
    TEST(16, R(RAX), Imm16(imm));
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
// Test accumulator %acR.
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
    get_ax_h(reg);
    //		Update_SR_Register16(val);
    Update_SR_Register16();
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
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    //		s64 acc0 = dsp_get_long_acc(0);
    get_long_acc(0, tmp1);
    MOV(64, R(RAX), R(tmp1));
    //		s64 acc1 = dsp_get_long_acc(1);
    get_long_acc(1, RDX);
    //		s64 res = dsp_convert_long_acc(acc0 - acc1);
    SUB(64, R(RAX), R(RDX));
    //		Update_SR_Register64(res, isCarry2(acc0, res), isOverflow(acc0, -acc1, res)); // CF ->
    // influence on ABS/0xa100
    NEG(64, R(RDX));
    Update_SR_Register64_Carry(EAX, tmp1, true);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPAR $acS axR.h
// 110r s001 xxxx xxxx
// Compares accumulator $acS with accumulator axR.h.
// Not described by Duddie's doc - at least not as a separate instruction.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpar(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 rreg = ((opc >> 12) & 0x1);
    u8 sreg = (opc >> 11) & 0x1;

    X64Reg tmp1 = m_gpr.GetFreeXReg();
    //		s64 sr = dsp_get_long_acc(sreg);
    get_long_acc(sreg, tmp1);
    MOV(64, R(RAX), R(tmp1));
    //		s64 rr = (s16)g_dsp.r.axh[rreg];
    get_ax_h(rreg, RDX);
    //		rr <<= 16;
    SHL(64, R(RDX), Imm8(16));
    //		s64 res = dsp_convert_long_acc(sr - rr);
    SUB(64, R(RAX), R(RDX));
    //		Update_SR_Register64(res, isCarry2(sr, res), isOverflow(sr, -rr, res));
    NEG(64, R(RDX));
    Update_SR_Register64_Carry(EAX, tmp1, true);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPI $amD, #I
// 0000 001r 1000 0000
// iiii iiii iiii iiii
// Compares mid accumulator $acD.hm ($amD) with sign extended immediate value I.
// Although flags are being set regarding whole accumulator register.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpi(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 reg = (opc >> 8) & 0x1;
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    //		s64 val = dsp_get_long_acc(reg);
    get_long_acc(reg, tmp1);
    MOV(64, R(RAX), R(tmp1));
    //		s64 imm = (s64)(s16)dsp_fetch_code() << 16; // Immediate is considered to be at M level in
    // the 40-bit accumulator.
    u16 imm = dsp_imem_read(m_compile_pc + 1);
    MOV(64, R(RDX), Imm64((s64)(s16)imm << 16));
    //		s64 res = dsp_convert_long_acc(val - imm);
    SUB(64, R(RAX), R(RDX));
    //		Update_SR_Register64(res, isCarry2(val, res), isOverflow(val, -imm, res));
    NEG(64, R(RDX));
    Update_SR_Register64_Carry(EAX, tmp1, true);
    m_gpr.PutXReg(tmp1);
  }
}

// CMPIS $acD, #I
// 0000 011d iiii iiii
// Compares accumulator with short immediate. Comparison is executed
// by subtracting short immediate (8bit sign extended) from mid accumulator
// $acD.hm and computing flags based on whole accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::cmpis(const UDSPInstruction opc)
{
  if (FlagsNeeded())
  {
    u8 areg = (opc >> 8) & 0x1;
    //		s64 acc = dsp_get_long_acc(areg);
    X64Reg tmp1 = m_gpr.GetFreeXReg();
    get_long_acc(areg, tmp1);
    MOV(64, R(RAX), R(tmp1));
    //		s64 val = (s8)opc;
    //		val <<= 16;
    MOV(64, R(RDX), Imm64((s64)(s8)opc << 16));
    //		s64 res = dsp_convert_long_acc(acc - val);
    SUB(64, R(RAX), R(RDX));
    //		Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -val, res));
    NEG(64, R(RDX));
    Update_SR_Register64_Carry(EAX, tmp1, true);
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
  get_acc_m(dreg, RAX);
  get_ax_h(sreg, RDX);
  XOR(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  get_ax_h(sreg, RDX);
  AND(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  get_ax_h(sreg, RDX);
  OR(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  get_acc_m(1 - dreg, RDX);
  AND(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  get_acc_m(1 - dreg, RDX);
  OR(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  get_acc_m(1 - dreg, RDX);
  XOR(64, R(RAX), R(RDX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
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
  get_acc_m(dreg, RAX);
  NOT(16, R(AX));
  //	g_dsp.r.acm[dreg] = accm;
  set_acc_m(dreg);
  //	Update_SR_Register16((s16)accm, false, false, isOverS32(dsp_get_long_acc(dreg)));
  if (FlagsNeeded())
  {
    get_long_acc(dreg, RCX);
    Update_SR_Register16_OverS32();
  }
}

// XORI $acD.m, #I
// 0000 001r 0010 0000
// iiii iiii iiii iiii
// Logic exclusive or (XOR) of accumulator mid part $acD.m with
// immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::xori(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  u16 imm = dsp_imem_read(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] ^= imm;
  get_acc_m(reg, RAX);
  XOR(16, R(RAX), Imm16(imm));
  set_acc_m(reg);
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    get_long_acc(reg, RCX);
    Update_SR_Register16_OverS32();
  }
}

// ANDI $acD.m, #I
// 0000 001r 0100 0000
// iiii iiii iiii iiii
// Logic AND of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::andi(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  u16 imm = dsp_imem_read(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] &= imm;
  get_acc_m(reg, RAX);
  AND(16, R(RAX), Imm16(imm));
  set_acc_m(reg);
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    get_long_acc(reg, RCX);
    Update_SR_Register16_OverS32();
  }
}

// ORI $acD.m, #I
// 0000 001r 0110 0000
// iiii iiii iiii iiii
// Logic OR of accumulator mid part $acD.m with immediate value I.
//
// flags out: --xx xx00
void DSPEmitter::ori(const UDSPInstruction opc)
{
  u8 reg = (opc >> 8) & 0x1;
  //	u16 imm = dsp_fetch_code();
  u16 imm = dsp_imem_read(m_compile_pc + 1);
  //	g_dsp.r.acm[reg] |= imm;
  get_acc_m(reg, RAX);
  OR(16, R(RAX), Imm16(imm));
  set_acc_m(reg);
  //	Update_SR_Register16((s16)g_dsp.r.acm[reg], false, false, isOverS32(dsp_get_long_acc(reg)));
  if (FlagsNeeded())
  {
    get_long_acc(reg, RCX);
    Update_SR_Register16_OverS32();
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

  //	s64 acc = dsp_get_long_acc(dreg);
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 ax = (s16)g_dsp.r[sreg];
  dsp_op_read_reg(sreg, RDX, RegisterExtension::Sign);
  //	ax <<= 16;
  SHL(64, R(RDX), Imm8(16));
  //	s64 res = acc + ax;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, ax, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
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

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 ax  = dsp_get_long_acx(sreg);
  get_long_acx(sreg, RDX);
  //	s64 res = acc + ax;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, ax, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// ADD $acD, $ac(1-D)
// 0100 110d xxxx xxxx
// Adds accumulator $ac(1-D) to accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::add(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc0 = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 acc1 = dsp_get_long_acc(1 - dreg);
  get_long_acc(1 - dreg, RDX);
  //	s64 res = acc0 + acc1;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc0, res), isOverflow(acc0, acc1, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// ADDP $acD
// 0100 111d xxxx xxxx
// Adds product register to accumulator register.
//
// flags out: x-xx xxxx
void DSPEmitter::addp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 prod = dsp_get_long_prod();
  get_long_prod(RDX);
  //	s64 res = acc + prod;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, prod, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
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

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	u64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	u16 acx = (u16)dsp_get_ax_l(sreg);
  get_ax_l(sreg, RDX);
  MOVZX(64, 16, RDX, R(RDX));
  //	u64 res = acc + acx;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, (s64)res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64((s64)res, isCarry(acc, res), isOverflow((s64)acc, (s64)acx, (s64)res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// ADDI $amR, #I
// 0000 001r 0000 0000
// iiii iiii iiii iiii
// Adds immediate (16-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void DSPEmitter::addi(const UDSPInstruction opc)
{
  u8 areg = (opc >> 8) & 0x1;
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(areg);
  get_long_acc(areg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 imm = (s16)dsp_fetch_code();
  s16 imm = dsp_imem_read(m_compile_pc + 1);
  // imm <<= 16;
  MOV(16, R(RDX), Imm16(imm));
  MOVSX(64, 16, RDX, R(RDX));
  SHL(64, R(RDX), Imm8(16));
  //	s64 res = acc + imm;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(areg, res);
  //	res = dsp_get_long_acc(areg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, imm, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(areg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(areg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// ADDIS $acD, #I
// 0000 010d iiii iiii
// Adds short immediate (8-bit sign extended) to mid accumulator $acD.hm.
//
// flags out: x-xx xxxx
void DSPEmitter::addis(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 imm = (s8)(u8)opc;
  //	imm <<= 16;
  MOV(8, R(RDX), Imm8((u8)opc));
  MOVSX(64, 8, RDX, R(RDX));
  SHL(64, R(RDX), Imm8(16));
  //	s64 res = acc + imm;
  ADD(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, imm, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
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
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  //	s64 res = acc + sub;
  LEA(64, RAX, MDisp(tmp1, subtract));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, subtract, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RDX), Imm32((u32)subtract));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg);
  }
  m_gpr.PutXReg(tmp1);
}

// INC $acD
// 0111 011d xxxx xxxx
// Increment accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::inc(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  //	s64 res = acc + 1;
  LEA(64, RAX, MDisp(tmp1, 1));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry(acc, res), isOverflow(acc, 1, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RDX), Imm64(1));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1);
  }
  else
  {
    set_long_acc(dreg);
  }
  m_gpr.PutXReg(tmp1);
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

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 ax = (s16)g_dsp.r[sreg];
  dsp_op_read_reg(sreg, RDX, RegisterExtension::Sign);
  //	ax <<= 16;
  SHL(64, R(RDX), Imm8(16));
  //	s64 res = acc - ax;
  SUB(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -ax, res));
  if (FlagsNeeded())
  {
    NEG(64, R(RDX));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
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

  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 acx = dsp_get_long_acx(sreg);
  get_long_acx(sreg, RDX);
  //	s64 res = acc - acx;
  SUB(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -acx, res));
  if (FlagsNeeded())
  {
    NEG(64, R(RDX));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// SUB $acD, $ac(1-D)
// 0101 110d xxxx xxxx
// Subtracts accumulator $ac(1-D) from accumulator register $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::sub(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc1 = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 acc2 = dsp_get_long_acc(1 - dreg);
  get_long_acc(1 - dreg, RDX);
  //	s64 res = acc1 - acc2;
  SUB(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc1, res), isOverflow(acc1, -acc2, res));
  if (FlagsNeeded())
  {
    NEG(64, R(RDX));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// SUBP $acD
// 0101 111d xxxx xxxx
// Subtracts product register from accumulator register.
//
// flags out: x-xx xxxx
void DSPEmitter::subp(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  MOV(64, R(RAX), R(tmp1));
  //	s64 prod = dsp_get_long_prod();
  get_long_prod(RDX);
  //	s64 res = acc - prod;
  SUB(64, R(RAX), R(RDX));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -prod, res));
  if (FlagsNeeded())
  {
    NEG(64, R(RDX));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
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
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  //	s64 res = acc - sub;
  LEA(64, RAX, MDisp(tmp1, -subtract));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -subtract, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RDX), Imm64(-subtract));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg, RAX);
  }
  m_gpr.PutXReg(tmp1);
}

// DEC $acD
// 0111 101d xxxx xxxx
// Decrement accumulator $acD.
//
// flags out: x-xx xxxx
void DSPEmitter::dec(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x01;
  X64Reg tmp1 = m_gpr.GetFreeXReg();
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg, tmp1);
  //	s64 res = acc - 1;
  LEA(64, RAX, MDisp(tmp1, -1));
  //	dsp_set_long_acc(dreg, res);
  //	res = dsp_get_long_acc(dreg);
  //	Update_SR_Register64(res, isCarry2(acc, res), isOverflow(acc, -1, res));
  if (FlagsNeeded())
  {
    MOV(64, R(RDX), Imm64(-1));
    MOV(64, R(RCX), R(RAX));
    set_long_acc(dreg, RCX);
    Update_SR_Register64_Carry(EAX, tmp1, true);
  }
  else
  {
    set_long_acc(dreg);
  }
  m_gpr.PutXReg(tmp1);
}

//----

// NEG $acD
// 0111 110d xxxx xxxx
// Negate accumulator $acD.
//
// flags out: --xx xx00
void DSPEmitter::neg(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 8) & 0x1;
  //	s64 acc = dsp_get_long_acc(dreg);
  get_long_acc(dreg);
  //	acc = 0 - acc;
  NEG(64, R(RAX));
  //	dsp_set_long_acc(dreg, acc);
  set_long_acc(dreg);
  //	Update_SR_Register64(dsp_get_long_acc(dreg));
  if (FlagsNeeded())
  {
    Update_SR_Register64();
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
// Moves secondary accumulator $axS to accumulator $axD.
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
// Moves accumulator $ax(1-D) to accumulator $axD.
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
