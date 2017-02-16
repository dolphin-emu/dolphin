// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/Jit/DSPEmitter.h"

using namespace Gen;

namespace DSP
{
namespace JIT
{
namespace x86
{
// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
void DSPEmitter::mrr(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1f;
  u8 dreg = (opc >> 5) & 0x1f;

  dsp_op_read_reg(sreg, EDX);
  dsp_op_write_reg(dreg, EDX);
  dsp_conditional_extend_accum(dreg);
}

// LRI $D, #I
// 0000 0000 100d dddd
// iiii iiii iiii iiii
// Load immediate value I to register $D.
//
// DSPSpy discovery: This, and possibly other instructions that load a
// register, has a different behaviour in S40 mode if loaded to AC0.M: The
// value gets sign extended to the whole accumulator! This does not happen in
// S16 mode.
void DSPEmitter::lri(const UDSPInstruction opc)
{
  u8 reg = opc & 0x1F;
  u16 imm = dsp_imem_read(m_compile_pc + 1);
  dsp_op_write_reg_imm(reg, imm);
  dsp_conditional_extend_accum_imm(reg, imm);
}

// LRIS $(0x18+D), #I
// 0000 1ddd iiii iiii
// Load immediate value I (8-bit sign extended) to accumulator register.
void DSPEmitter::lris(const UDSPInstruction opc)
{
  u8 reg = ((opc >> 8) & 0x7) + DSP_REG_AXL0;
  u16 imm = (s8)opc;
  dsp_op_write_reg_imm(reg, imm);
  dsp_conditional_extend_accum_imm(reg, imm);
}

//----

// NX
// 1000 -000 xxxx xxxx
// No operation, but can be extended with extended opcode.
// This opcode is supposed to do nothing - it's used if you want to use
// an opcode extension but not do anything. At least according to duddie.
void DSPEmitter::nx(const UDSPInstruction opc)
{
}

//----

// DAR $arD
// 0000 0000 0000 01dd
// Decrement address register $arD.
void DSPEmitter::dar(const UDSPInstruction opc)
{
  //	g_dsp.r[opc & 0x3] = dsp_decrement_addr_reg(opc & 0x3);
  decrement_addr_reg(opc & 0x3);
}

// IAR $arD
// 0000 0000 0000 10dd
// Increment address register $arD.
void DSPEmitter::iar(const UDSPInstruction opc)
{
  //	g_dsp.r[opc & 0x3] = dsp_increment_addr_reg(opc & 0x3);
  increment_addr_reg(opc & 0x3);
}

// SUBARN $arD
// 0000 0000 0000 11dd
// Subtract indexing register $ixD from an addressing register $arD.
// used only in IPL-NTSC ucode
void DSPEmitter::subarn(const UDSPInstruction opc)
{
  //	u8 dreg = opc & 0x3;
  //	g_dsp.r[dreg] = dsp_decrease_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + dreg]);
  decrease_addr_reg(opc & 0x3);
}

// ADDARN $arD, $ixS
// 0000 0000 0001 ssdd
// Adds indexing register $ixS to an addressing register $arD.
// It is critical for the Zelda ucode that this one wraps correctly.
void DSPEmitter::addarn(const UDSPInstruction opc)
{
  //	u8 dreg = opc & 0x3;
  //	u8 sreg = (opc >> 2) & 0x3;
  //	g_dsp.r[dreg] = dsp_increase_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]);

  // From looking around it is always called with the matching index register
  increase_addr_reg(opc & 0x3, (opc >> 2) & 0x3);
}

//----

void DSPEmitter::setCompileSR(u16 bit)
{
  //	g_dsp.r[DSP_REG_SR] |= bit
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  OR(16, sr_reg, Imm16(bit));
  m_gpr.PutReg(DSP_REG_SR);

  m_compile_status_register |= bit;
}

void DSPEmitter::clrCompileSR(u16 bit)
{
  //	g_dsp.r[DSP_REG_SR] &= bit
  const OpArg sr_reg = m_gpr.GetReg(DSP_REG_SR);
  AND(16, sr_reg, Imm16(~bit));
  m_gpr.PutReg(DSP_REG_SR);

  m_compile_status_register &= ~bit;
}
// SBCLR #I
// 0001 0011 aaaa aiii
// bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void DSPEmitter::sbclr(const UDSPInstruction opc)
{
  u8 bit = (opc & 0x7) + 6;

  clrCompileSR(1 << bit);
}

// SBSET #I
// 0001 0010 aaaa aiii
// Set bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void DSPEmitter::sbset(const UDSPInstruction opc)
{
  u8 bit = (opc & 0x7) + 6;

  setCompileSR(1 << bit);
}

// 1000 1bbb xxxx xxxx, bbb >= 010
// This is a bunch of flag setters, flipping bits in SR. So far so good,
// but it's harder to know exactly what effect they have.
void DSPEmitter::srbith(const UDSPInstruction opc)
{
  switch ((opc >> 8) & 0xf)
  {
  // M0/M2 change the multiplier mode (it can multiply by 2 for free).
  case 0xa:  // M2
    clrCompileSR(SR_MUL_MODIFY);
    break;
  case 0xb:  // M0
    setCompileSR(SR_MUL_MODIFY);
    break;

  // If set, treat multiplicands as unsigned.
  // If clear, treat them as signed.
  case 0xc:  // CLR15
    clrCompileSR(SR_MUL_UNSIGNED);
    break;
  case 0xd:  // SET15
    setCompileSR(SR_MUL_UNSIGNED);
    break;

  // Automatic 40-bit sign extension when loading ACx.M.
  // SET40 changes something very important: see the LRI instruction above.
  case 0xe:  // SET16 (CLR40)
    clrCompileSR(SR_40_MODE_BIT);
    break;

  case 0xf:  // SET40
    setCompileSR(SR_40_MODE_BIT);
    break;

  default:
    break;
  }
}

}  // namespace x86
}  // namespace JIT
}  // namespace DSP
