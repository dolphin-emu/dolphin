// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Jit/DSPEmitter.h"

using namespace Gen;

namespace DSP
{
namespace JIT
{
namespace x86
{
// In: val: s64 _Value
// Clobbers scratch
void DSPEmitter::Update_SR_Register(Gen::X64Reg val, Gen::X64Reg scratch)
{
  _assert_(val != scratch);

  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  //	// 0x04
  //	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
  TEST(64, R(val), R(val));
  FixupBranch notZero = J_CC(CC_NZ);
  OR(16, sr_reg, Imm16(SR_ARITH_ZERO | SR_TOP2BITS));
  FixupBranch end = J();
  SetJumpTarget(notZero);

  //	// 0x08
  //	if (_Value < 0) g_dsp.r[DSP_REG_SR] |= SR_SIGN;
  FixupBranch greaterThanEqual = J_CC(CC_GE);
  OR(16, sr_reg, Imm16(SR_SIGN));
  SetJumpTarget(greaterThanEqual);

  //	// 0x10
  //	if (_Value != (s32)_Value) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
  MOVSX(64, 32, scratch, R(val));
  CMP(64, R(scratch), R(val));
  FixupBranch noOverS32 = J_CC(CC_E);
  OR(16, sr_reg, Imm16(SR_OVER_S32));
  SetJumpTarget(noOverS32);

  //	// 0x20 - Checks if top bits of m are equal
  //	if (((_Value & 0xc0000000) == 0) || ((_Value & 0xc0000000) == 0xc0000000))
  MOV(32, R(scratch), Imm32(0xc0000000));
  AND(32, R(val), R(scratch));
  FixupBranch zeroC = J_CC(CC_Z);
  CMP(32, R(val), R(scratch));
  FixupBranch cC = J_CC(CC_NE);
  SetJumpTarget(zeroC);
  //		g_dsp.r[DSP_REG_SR] |= SR_TOP2BITS;
  OR(16, sr_reg, Imm16(SR_TOP2BITS));
  SetJumpTarget(cC);
  SetJumpTarget(end);
  m_gpr.PutReg(DSP_REG_SR);
}

// In: val: s64 _Value
// Clobbers scratch
void DSPEmitter::Update_SR_Register64(Gen::X64Reg val, Gen::X64Reg scratch)
{
  //	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));
  m_gpr.PutReg(DSP_REG_SR);
  Update_SR_Register(val, scratch);
}

// In: (val): s64 _Value
// In: (carry_ovfl): 1 = carry, 2 = overflow
// Clobbers RDX
void DSPEmitter::Update_SR_Register64_Carry(X64Reg val, X64Reg carry_ovfl, bool carry_eq)
{
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  //	g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));

  CMP(64, R(carry_ovfl), R(val));

  // 0x01
  //	g_dsp.r[DSP_REG_SR] |= SR_CARRY;
  // Carry = (acc>res)
  // Carry2 = (acc>=res)
  FixupBranch noCarry = J_CC(carry_eq ? CC_B : CC_BE);
  OR(16, sr_reg, Imm16(SR_CARRY));
  SetJumpTarget(noCarry);

  // 0x02 and 0x80
  //	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
  //	g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
  // Overflow = ((acc ^ res) & (ax ^ res)) < 0
  XOR(64, R(carry_ovfl), R(val));
  XOR(64, R(RDX), R(val));
  TEST(64, R(carry_ovfl), R(RDX));
  FixupBranch noOverflow = J_CC(CC_GE);
  OR(16, sr_reg, Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
  SetJumpTarget(noOverflow);

  m_gpr.PutReg(DSP_REG_SR);
  if (carry_eq)
  {
    Update_SR_Register();
  }
  else
  {
    Update_SR_Register(val);
  }
}

// In: RAX: s64 _Value
void DSPEmitter::Update_SR_Register16(X64Reg val)
{
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));

  //	// 0x04
  //	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
  TEST(64, R(val), R(val));
  FixupBranch notZero = J_CC(CC_NZ);
  OR(16, sr_reg, Imm16(SR_ARITH_ZERO | SR_TOP2BITS));
  FixupBranch end = J();
  SetJumpTarget(notZero);

  //	// 0x08
  //	if (_Value < 0) g_dsp.r[DSP_REG_SR] |= SR_SIGN;
  FixupBranch greaterThanEqual = J_CC(CC_GE);
  OR(16, sr_reg, Imm16(SR_SIGN));
  SetJumpTarget(greaterThanEqual);

  //	// 0x20 - Checks if top bits of m are equal
  //	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
  SHR(16, R(val), Imm8(14));
  TEST(16, R(val), R(val));
  FixupBranch isZero = J_CC(CC_Z);
  CMP(16, R(val), Imm16(3));
  FixupBranch notThree = J_CC(CC_NE);
  SetJumpTarget(isZero);
  //		g_dsp.r[DSP_REG_SR] |= SR_TOP2BITS;
  OR(16, sr_reg, Imm16(SR_TOP2BITS));
  SetJumpTarget(notThree);
  SetJumpTarget(end);
  m_gpr.PutReg(DSP_REG_SR);
}

// In: RAX: s64 _Value
// Clobbers RCX
void DSPEmitter::Update_SR_Register16_OverS32(Gen::X64Reg val)
{
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));

  //	// 0x10
  //	if (_Value != (s32)_Value) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
  MOVSX(64, 32, RCX, R(val));
  CMP(64, R(RCX), R(val));
  FixupBranch noOverS32 = J_CC(CC_E);
  OR(16, sr_reg, Imm16(SR_OVER_S32));
  SetJumpTarget(noOverS32);

  m_gpr.PutReg(DSP_REG_SR);
  //	// 0x20 - Checks if top bits of m are equal
  //	if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
  // AND(32, R(val), Imm32(0xc0000000));
  Update_SR_Register16(val);
}

}  // namespace x86
}  // namespace JIT
}  // namespace DSP
