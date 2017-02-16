// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Jit/DSPEmitter.h"

using namespace Gen;

/* It is safe to directly write to the address registers as they are
   neither read not written by any extendable opcode. The same is true
   for memory accesses.
   It probably even is safe to write to all registers except for
   SR, ACx.x, AXx.x and PROD, which may be modified by the main op.

   This code uses EBX to keep the values of the registers written by
   the extended op so the main op can still access the old values.
   storeIndex and storeIndex2 control where the lower and upper 16bits
   of EBX are written to. Additionally, the upper 16bits can contain the
   original SR so we can do sign extension in 40bit mode. There is only
   the 'ld family of opcodes writing to two registers at the same time,
   and those always are AXx.x, thus no need to leave space for SR for
   sign extension.
 */

namespace DSP
{
namespace JIT
{
namespace x86
{
// DR $arR
// xxxx xxxx 0000 01rr
// Decrement addressing register $arR.
void DSPEmitter::dr(const UDSPInstruction opc)
{
  decrement_addr_reg(opc & 0x3);
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void DSPEmitter::ir(const UDSPInstruction opc)
{
  increment_addr_reg(opc & 0x3);
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void DSPEmitter::nr(const UDSPInstruction opc)
{
  u8 reg = opc & 0x3;

  increase_addr_reg(reg, reg);
}

// MV $axD.D, $acS.S
// xxxx xxxx 0001 ddss
// Move value of $acS.S to the $axD.D.
void DSPEmitter::mv(const UDSPInstruction opc)
{
  u8 sreg = (opc & 0x3) + DSP_REG_ACL0;
  u8 dreg = ((opc >> 2) & 0x3);
  dsp_op_read_reg(sreg, RBX, RegisterExtension::Zero);
  m_store_index = dreg + DSP_REG_AXL0;
}

// S @$arD, $acS.S
// xxxx xxxx 001s s0dd
// Store value of $acS.S in the memory pointed by register $arD.
// Post increment register $arD.
void DSPEmitter::s(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;
  //	u16 addr = g_dsp.r[dest];
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1, RegisterExtension::Zero);
  //	u16 val = g_dsp.r[src];
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  increment_addr_reg(dreg);
}

// SN @$arD, $acS.S
// xxxx xxxx 001s s1dd
// Store value of register $acS.S in the memory pointed by register $arD.
// Add indexing register $ixD to register $arD.
void DSPEmitter::sn(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  increase_addr_reg(dreg, dreg);
}

// L $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Post increment register $arS.
void DSPEmitter::l(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x3;
  u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;  // AX?.?, AC?.[LM]

  pushExtValueFromMem(dreg, sreg);

  if (dreg >= DSP_REG_ACM0)
  {
    // save SR too, so we can decide later.
    // even if only for one bit, can only
    // store (up to) two registers in EBX,
    // so store all of SR
    dsp_op_read_reg(DSP_REG_SR, RAX);
    SHL(32, R(EAX), Imm8(16));
    OR(32, R(EBX), R(EAX));
  }

  increment_addr_reg(sreg);
}

// LN $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Add indexing register $ixS to register $arS.
void DSPEmitter::ln(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x3;
  u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;

  pushExtValueFromMem(dreg, sreg);

  if (dreg >= DSP_REG_ACM0)
  {
    // save SR too, so we can decide later.
    // even if only for one bit, can only
    // store (up to) two registers in EBX,
    // so store all of SR
    dsp_op_read_reg(DSP_REG_SR, RAX);
    SHL(32, R(EAX), Imm8(16));
    OR(32, R(EBX), R(EAX));
  }

  increase_addr_reg(sreg, sreg);
}

// LS $axD.D, $acS.m
// xxxx xxxx 10dd 000s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Increment both $ar0 and $ar3.
void DSPEmitter::ls(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR3, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR0);

  increment_addr_reg(DSP_REG_AR3);
  increment_addr_reg(DSP_REG_AR0);
}

// LSN $axD.D, $acS.m
// xxxx xxxx 10dd 010s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and increment $ar3.
void DSPEmitter::lsn(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR3, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR0);

  increment_addr_reg(DSP_REG_AR3);
  increase_addr_reg(DSP_REG_AR0, DSP_REG_AR0);
}

// LSM $axD.D, $acS.m
// xxxx xxxx 10dd 100s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix3 to addressing
// register $ar3 and increment $ar0.
void DSPEmitter::lsm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR3, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR0);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
  increment_addr_reg(DSP_REG_AR0);
}

// LSMN $axD.D, $acS.m
// xxxx xxxx 10dd 110s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and add corresponding indexing register $ix3 to addressing
// register $ar3.
void DSPEmitter::lsnm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR3, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR0);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
  increase_addr_reg(DSP_REG_AR0, DSP_REG_AR0);
}

// SL $acS.m, $axD.D
// xxxx xxxx 10dd 001s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Increment both $ar0 and $ar3.
void DSPEmitter::sl(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR0, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR3);

  increment_addr_reg(DSP_REG_AR3);
  increment_addr_reg(DSP_REG_AR0);
}

// SLN $acS.m, $axD.D
// xxxx xxxx 10dd 011s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and increment $ar3.
void DSPEmitter::sln(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR0, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR3);

  increment_addr_reg(DSP_REG_AR3);
  increase_addr_reg(DSP_REG_AR0, DSP_REG_AR0);
}

// SLM $acS.m, $axD.D
// xxxx xxxx 10dd 101s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix3 to addressing register $ar3
// and increment $ar0.
void DSPEmitter::slm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR0, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR3);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
  increment_addr_reg(DSP_REG_AR0);
}

// SLMN $acS.m, $axD.D
// xxxx xxxx 10dd 111s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and add corresponding indexing register $ix3 to addressing register $ar3.
void DSPEmitter::slnm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  dsp_op_read_reg(DSP_REG_AR0, RAX, RegisterExtension::Zero);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg + DSP_REG_ACM0, tmp1, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  pushExtValueFromMem(dreg, DSP_REG_AR3);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
  increase_addr_reg(DSP_REG_AR0, DSP_REG_AR0);
}

// LD $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 00ss
// example for "nx'ld $AX0.L, $AX1.L, @$AR3"
// Loads the word pointed by AR0 to AX0.H, then loads the word pointed by AR3
// to AX0.L.  Increments AR0 and AR3.  If AR0 and AR3 point into the same
// memory page (upper 6 bits of addr are the same -> games are not doing that!)
// then the value pointed by AR0 is loaded to BOTH AX0.H and AX0.L.  If AR0
// points into an invalid memory page (ie 0x2000), then AX0.H keeps its old
// value. (not implemented yet) If AR3 points into an invalid memory page, then
// AX0.L gets the same value as AX0.H. (not implemented yet)
void DSPEmitter::ld(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

  // 	if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  X64Reg tmp = m_gpr.GetFreeXReg();
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);
  SetJumpTarget(not_equal);  // else
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increment_addr_reg(sreg);

  increment_addr_reg(DSP_REG_AR3);
}

// LDAX $axR, @$arS
// xxxx xxxx 11sr 0011
void DSPEmitter::ldax(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  pushExtValueFromMem(rreg + DSP_REG_AXH0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);  // else
  SetJumpTarget(not_equal);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increment_addr_reg(sreg);

  increment_addr_reg(DSP_REG_AR3);
}

// LDN $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 01ss
void DSPEmitter::ldn(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);
  SetJumpTarget(not_equal);  // else
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increase_addr_reg(sreg, sreg);

  increment_addr_reg(DSP_REG_AR3);
}

// LDAXN $axR, @$arS
// xxxx xxxx 11sr 0111
void DSPEmitter::ldaxn(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  pushExtValueFromMem(rreg + DSP_REG_AXH0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);  // else
  SetJumpTarget(not_equal);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increase_addr_reg(sreg, sreg);

  increment_addr_reg(DSP_REG_AR3);
}

// LDM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 10ss
void DSPEmitter::ldm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);
  SetJumpTarget(not_equal);  // else
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increment_addr_reg(sreg);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
}

// LDAXM $axR, @$arS
// xxxx xxxx 11sr 1011
void DSPEmitter::ldaxm(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  pushExtValueFromMem(rreg + DSP_REG_AXH0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);  // else
  SetJumpTarget(not_equal);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increment_addr_reg(sreg);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
}

// LDNM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 11ss
void DSPEmitter::ldnm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);
  SetJumpTarget(not_equal);  // else
  pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increase_addr_reg(sreg, sreg);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
}

// LDAXNM $axR, @$arS
// xxxx xxxx 11sr 1111
void DSPEmitter::ldaxnm(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  pushExtValueFromMem(rreg + DSP_REG_AXH0, sreg);

  X64Reg tmp = m_gpr.GetFreeXReg();
  // if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
  dsp_op_read_reg(sreg, RCX);
  dsp_op_read_reg(DSP_REG_AR3, tmp);
  XOR(16, R(ECX), R(tmp));
  m_gpr.PutXReg(tmp);
  DSPJitRegCache c(m_gpr);
  TEST(16, R(ECX), Imm16(0xfc00));
  FixupBranch not_equal = J_CC(CC_NE, true);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, sreg);
  m_gpr.FlushRegs(c);
  FixupBranch after = J(true);  // else
  SetJumpTarget(not_equal);
  pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
  m_gpr.FlushRegs(c);
  SetJumpTarget(after);

  increase_addr_reg(sreg, sreg);

  increase_addr_reg(DSP_REG_AR3, DSP_REG_AR3);
}

// Push value from address in g_dsp.r[sreg] into EBX and stores the
// destinationindex in storeIndex
void DSPEmitter::pushExtValueFromMem(u16 dreg, u16 sreg)
{
  //	u16 addr = g_dsp.r[addr];

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1, RegisterExtension::Zero);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  MOVZX(32, 16, EBX, R(EAX));

  m_store_index = dreg;
}

void DSPEmitter::pushExtValueFromMem2(u16 dreg, u16 sreg)
{
  //	u16 addr = g_dsp.r[addr];

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1, RegisterExtension::Zero);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  SHL(32, R(EAX), Imm8(16));
  OR(32, R(EBX), R(EAX));

  m_store_index2 = dreg;
}

void DSPEmitter::popExtValueToReg()
{
  // in practice, we rarely ever have a non-NX main op
  // with an extended op, so the OR here is either
  // not run (storeIndex == -1) or ends up OR'ing
  // EBX with 0 (becoming the MOV we have here)
  // nakee wants to keep it clean, so lets do that.
  // [nakeee] the or case never happens in real
  // [nakeee] it's just how the hardware works so we added it
  if (m_store_index != -1)
  {
    dsp_op_write_reg(m_store_index, RBX);
    if (m_store_index >= DSP_REG_ACM0 && m_store_index2 == -1)
    {
      TEST(32, R(EBX), Imm32(SR_40_MODE_BIT << 16));
      FixupBranch not_40bit = J_CC(CC_Z, true);
      DSPJitRegCache c(m_gpr);
      // if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
      //{
      // Sign extend into whole accum.
      // u16 val = g_dsp.r[reg];
      MOVSX(32, 16, EAX, R(EBX));
      SHR(32, R(EAX), Imm8(16));
      // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
      // g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
      set_acc_h(m_store_index - DSP_REG_ACM0, R(RAX));
      set_acc_l(m_store_index - DSP_REG_ACM0, Imm16(0));
      //}
      m_gpr.FlushRegs(c);
      SetJumpTarget(not_40bit);
    }
  }

  m_store_index = -1;

  if (m_store_index2 != -1)
  {
    SHR(32, R(EBX), Imm8(16));
    dsp_op_write_reg(m_store_index2, RBX);
  }

  m_store_index2 = -1;
}

}  // namespace x86
}  // namespace JIT
}  // namespace DSP
