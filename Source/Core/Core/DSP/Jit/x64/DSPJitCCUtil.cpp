// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Jit/x64/DSPEmitter.h"

#include "Core/DSP/DSPCore.h"

using namespace Gen;

namespace DSP::JIT::x64
{
// In: val: s64 _Value
// Clobbers scratch
void DSPEmitter::Update_SR_Register(Gen::X64Reg val, Gen::X64Reg scratch)
{
  ASSERT(val != scratch);

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

// Updates SR based on a 64-bit value computed by result = val1 + val2 or result = val1 - val2
// Clobbers scratch
void DSPEmitter::UpdateSR64AddSub(Gen::X64Reg val1, Gen::X64Reg val2, Gen::X64Reg result,
                                  Gen::X64Reg scratch, bool subtract)
{
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  // g_dsp.r[DSP_REG_SR] &= ~SR_CMP_MASK;
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));

  CMP(64, R(val1), R(result));
  // x86 ZF set if val1 == result
  // x86 CF set if val1 < result
  // Note that x86 uses a different definition of carry than the DSP

  // 0x01
  // g_dsp.r[DSP_REG_SR] |= SR_CARRY;
  // isCarryAdd = (val1 > result) => skip setting if (val <= result) => jump if ZF or CF => use JBE
  // isCarrySubtract = (val1 >= result) => skip setting if (val < result) => jump if CF => use JB
  FixupBranch noCarry = J_CC(subtract ? CC_B : CC_BE);
  OR(16, sr_reg, Imm16(SR_CARRY));
  SetJumpTarget(noCarry);

  // 0x02 and 0x80
  // g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW;
  // g_dsp.r[DSP_REG_SR] |= SR_OVERFLOW_STICKY;
  // Overflow (add) = ((val1 ^ res) & (val2 ^ res)) < 0
  // Overflow (sub) = ((val1 ^ res) & (-val2 ^ res)) < 0
  MOV(64, R(scratch), R(val1));
  XOR(64, R(scratch), R(result));

  if (subtract)
    NEG(64, R(val2));
  XOR(64, R(result), R(val2));

  TEST(64, R(scratch), R(result));  // Test scratch & value
  FixupBranch noOverflow = J_CC(CC_GE);
  OR(16, sr_reg, Imm16(SR_OVERFLOW | SR_OVERFLOW_STICKY));
  SetJumpTarget(noOverflow);

  // Restore result and val2 -- TODO: does this really matter?
  XOR(64, R(result), R(val2));
  if (subtract)
    NEG(64, R(val2));

  m_gpr.PutReg(DSP_REG_SR);
  Update_SR_Register(result, scratch);
}

// In: RAX: s16 _Value (middle)
void DSPEmitter::Update_SR_Register16(X64Reg val)
{
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  AND(16, sr_reg, Imm16(~SR_CMP_MASK));

  //	// 0x04
  //	if (_Value == 0) g_dsp.r[DSP_REG_SR] |= SR_ARITH_ZERO;
  TEST(16, R(val), R(val));
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

// In: RAX: s16 _Value (middle)
// In: RDX: s64 _FullValue
// Clobbers scratch
void DSPEmitter::Update_SR_Register16_OverS32(Gen::X64Reg val, Gen::X64Reg full_val,
                                              Gen::X64Reg scratch)
{
  Update_SR_Register16(val);

  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);

  //	// 0x10
  //	if (_FullValue != (s32)_FullValue) g_dsp.r[DSP_REG_SR] |= SR_OVER_S32;
  MOVSX(64, 32, scratch, R(full_val));
  CMP(64, R(scratch), R(full_val));
  FixupBranch noOverS32 = J_CC(CC_E);
  OR(16, sr_reg, Imm16(SR_OVER_S32));
  SetJumpTarget(noOverS32);

  m_gpr.PutReg(DSP_REG_SR);
}

}  // namespace DSP::JIT::x64
