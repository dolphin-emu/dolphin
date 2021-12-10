// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Jit/x64/DSPEmitter.h"

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"

using namespace Gen;

namespace DSP::JIT::x64
{
// SRSH @M, $acS.h
// 0010 1sss mmmm mmmm
// Move value from register $acS.h to data memory pointed by address
// CR[0-7] | M. That is, the upper 8 bits of the address are the
// bottom 8 bits from CR, and the lower 8 bits are from the 8-bit immediate.
void DSPEmitter::srsh(const UDSPInstruction opc)
{
  u8 reg = ((opc >> 8) & 0x1) + DSP_REG_ACH0;
  // u16 addr = (g_dsp.r.cr << 8) | (opc & 0xFF);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  dsp_op_read_reg(DSP_REG_CR, RAX, RegisterExtension::Zero);
  SHL(16, R(EAX), Imm8(8));
  OR(16, R(EAX), Imm16(opc & 0xFF));
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);
}

// SRS @M, $(0x1C+S)
// 0010 1sss mmmm mmmm
// Move value from register $(0x1C+S) to data memory pointed by address
// CR[0-7] | M. That is, the upper 8 bits of the address are the
// bottom 8 bits from CR, and the lower 8 bits are from the 8-bit immediate.
void DSPEmitter::srs(const UDSPInstruction opc)
{
  u8 reg = ((opc >> 8) & 0x3) + DSP_REG_ACL0;
  // u16 addr = (g_dsp.r.cr << 8) | (opc & 0xFF);

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  dsp_op_read_reg(DSP_REG_CR, RAX, RegisterExtension::Zero);
  SHL(16, R(EAX), Imm8(8));
  OR(16, R(EAX), Imm16(opc & 0xFF));
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);
}

// LRS $(0x18+D), @M
// 0010 0ddd mmmm mmmm
// Move value from data memory pointed by address CR[0-7] | M to register
// $(0x18+D).  That is, the upper 8 bits of the address are the bottom 8 bits
// from CR, and the lower 8 bits are from the 8-bit immediate.
void DSPEmitter::lrs(const UDSPInstruction opc)
{
  u8 reg = ((opc >> 8) & 0x7) + 0x18;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  // u16 addr = (g_dsp.r[DSP_REG_CR] << 8) | (opc & 0xFF);
  dsp_op_read_reg(DSP_REG_CR, tmp1, RegisterExtension::Zero);
  SHL(16, R(tmp1), Imm8(8));
  OR(16, R(tmp1), Imm16(opc & 0xFF));
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  dsp_op_write_reg(reg, RAX);
  dsp_conditional_extend_accum(reg);
}

// LR $D, @M
// 0000 0000 110d dddd
// mmmm mmmm mmmm mmmm
// Move value from data memory pointed by address M to register $D.
void DSPEmitter::lr(const UDSPInstruction opc)
{
  const int reg = opc & 0x1F;
  const u16 address = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);
  dmem_read_imm(address);
  dsp_op_write_reg(reg, EAX);
  dsp_conditional_extend_accum(reg);
}

// SR @M, $S
// 0000 0000 111s ssss
// mmmm mmmm mmmm mmmm
// Store value from register $S to a memory pointed by address M.
void DSPEmitter::sr(const UDSPInstruction opc)
{
  const u8 reg = opc & 0x1F;
  const u16 address = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);

  const X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1);
  dmem_write_imm(address, tmp1);

  m_gpr.PutXReg(tmp1);
}

// SI @M, #I
// 0001 0110 mmmm mmmm
// iiii iiii iiii iiii
// Store 16-bit immediate value I to a memory location pointed by address
// M (M is 8-bit value sign extended).
void DSPEmitter::si(const UDSPInstruction opc)
{
  const u16 address = static_cast<s8>(opc);
  const u16 imm = m_dsp_core.DSPState().ReadIMEM(m_compile_pc + 1);

  const X64Reg tmp1 = m_gpr.GetFreeXReg();

  MOV(32, R(tmp1), Imm32((u32)imm));
  dmem_write_imm(address, tmp1);

  m_gpr.PutXReg(tmp1);
}

// LRR $D, @$arS
// 0001 1000 0ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
void DSPEmitter::lrr(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x3;
  u8 dreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  dsp_op_write_reg(dreg, EAX);
  dsp_conditional_extend_accum(dreg);
}

// LRRD $D, @$arS
// 0001 1000 1ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Decrement register $arS.
void DSPEmitter::lrrd(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x3;
  u8 dreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  dsp_op_write_reg(dreg, EAX);
  dsp_conditional_extend_accum(dreg);
  decrement_addr_reg(sreg);
}

// LRRI $D, @$arS
// 0001 1001 0ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Increment register $arS.
void DSPEmitter::lrri(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x3;
  u8 dreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  dsp_op_write_reg(dreg, EAX);
  dsp_conditional_extend_accum(dreg);
  increment_addr_reg(sreg);
}

// LRRN $D, @$arS
// 0001 1001 1ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Add indexing register $ixS to register $arS.
void DSPEmitter::lrrn(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x3;
  u8 dreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dmem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  dsp_op_write_reg(dreg, EAX);
  dsp_conditional_extend_accum(dreg);
  increase_addr_reg(sreg, sreg);
}

// SRR @$arD, $S
// 0001 1010 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD.
void DSPEmitter::srr(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x3;
  u8 sreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);
}

// SRRD @$arD, $S
// 0001 1010 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Decrement register $arD.
void DSPEmitter::srrd(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x3;
  u8 sreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  decrement_addr_reg(dreg);
}

// SRRI @$arD, $S
// 0001 1011 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Increment register $arD.
void DSPEmitter::srri(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x3;
  u8 sreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  increment_addr_reg(dreg);
}

// SRRN @$arD, $S
// 0001 1011 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Add corresponding indexing register $ixD to register $arD.
void DSPEmitter::srrn(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x3;
  u8 sreg = opc & 0x1f;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(sreg, tmp1);
  dsp_op_read_reg(dreg, RAX, RegisterExtension::Zero);
  dmem_write(tmp1);

  m_gpr.PutXReg(tmp1);

  increase_addr_reg(dreg, dreg);
}

// ILRR $acD.m, @$arS
// 0000 001d 0001 00ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m.
void DSPEmitter::ilrr(const UDSPInstruction opc)
{
  u16 reg = opc & 0x3;
  u16 dreg = (opc >> 8) & 1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  imem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  set_acc_m(dreg, R(RAX));
  dsp_conditional_extend_accum(dreg + DSP_REG_ACM0);
}

// ILRRD $acD.m, @$arS
// 0000 001d 0001 01ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Decrement addressing register $arS.
void DSPEmitter::ilrrd(const UDSPInstruction opc)
{
  u16 reg = opc & 0x3;
  u16 dreg = (opc >> 8) & 1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  imem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  set_acc_m(dreg, R(RAX));
  dsp_conditional_extend_accum(dreg + DSP_REG_ACM0);
  decrement_addr_reg(reg);
}

// ILRRI $acD.m, @$S
// 0000 001d 0001 10ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Increment addressing register $arS.
void DSPEmitter::ilrri(const UDSPInstruction opc)
{
  u16 reg = opc & 0x3;
  u16 dreg = (opc >> 8) & 1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  imem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  set_acc_m(dreg, R(RAX));
  dsp_conditional_extend_accum(dreg + DSP_REG_ACM0);
  increment_addr_reg(reg);
}

// ILRRN $acD.m, @$arS
// 0000 001d 0001 11ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Add corresponding indexing
// register $ixS to addressing register $arS.
void DSPEmitter::ilrrn(const UDSPInstruction opc)
{
  u16 reg = opc & 0x3;
  u16 dreg = (opc >> 8) & 1;

  X64Reg tmp1 = m_gpr.GetFreeXReg();

  dsp_op_read_reg(reg, tmp1, RegisterExtension::Zero);
  imem_read(tmp1);

  m_gpr.PutXReg(tmp1);

  set_acc_m(dreg, R(RAX));
  dsp_conditional_extend_accum(dreg + DSP_REG_ACM0);
  increase_addr_reg(reg, reg);
}

}  // namespace DSP::JIT::x64
