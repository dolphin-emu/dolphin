// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

namespace DSP::Interpreter
{
// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
void mrr(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1f;
  u8 dreg = (opc >> 5) & 0x1f;

  if (sreg >= DSP_REG_ACM0)
    dsp_op_write_reg(dreg, dsp_op_read_reg_and_saturate(sreg - DSP_REG_ACM0));
  else
    dsp_op_write_reg(dreg, dsp_op_read_reg(sreg));

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
void lri(const UDSPInstruction opc)
{
  u8 reg = opc & 0x1F;
  u16 imm = dsp_fetch_code();
  dsp_op_write_reg(reg, imm);
  dsp_conditional_extend_accum(reg);
}

// LRIS $(0x18+D), #I
// 0000 1ddd iiii iiii
// Load immediate value I (8-bit sign extended) to accumulator register.
void lris(const UDSPInstruction opc)
{
  u8 reg = ((opc >> 8) & 0x7) + DSP_REG_AXL0;
  u16 imm = (s8)opc;
  dsp_op_write_reg(reg, imm);
  dsp_conditional_extend_accum(reg);
}

//----

// NX
// 1000 -000 xxxx xxxx
// No operation, but can be extended with extended opcode.
// This opcode is supposed to do nothing - it's used if you want to use
// an opcode extension but not do anything. At least according to duddie.
void nx(const UDSPInstruction opc)
{
  ZeroWriteBackLog();
}

//----

// DAR $arD
// 0000 0000 0000 01dd
// Decrement address register $arD.
void dar(const UDSPInstruction opc)
{
  g_dsp.r.ar[opc & 0x3] = dsp_decrement_addr_reg(opc & 0x3);
}

// IAR $arD
// 0000 0000 0000 10dd
// Increment address register $arD.
void iar(const UDSPInstruction opc)
{
  g_dsp.r.ar[opc & 0x3] = dsp_increment_addr_reg(opc & 0x3);
}

// SUBARN $arD
// 0000 0000 0000 11dd
// Subtract indexing register $ixD from an addressing register $arD.
// used only in IPL-NTSC ucode
void subarn(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  g_dsp.r.ar[dreg] = dsp_decrease_addr_reg(dreg, (s16)g_dsp.r.ix[dreg]);
}

// ADDARN $arD, $ixS
// 0000 0000 0001 ssdd
// Adds indexing register $ixS to an addressing register $arD.
// It is critical for the Zelda ucode that this one wraps correctly.
void addarn(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  u8 sreg = (opc >> 2) & 0x3;
  g_dsp.r.ar[dreg] = dsp_increase_addr_reg(dreg, (s16)g_dsp.r.ix[sreg]);
}

//----

// SBCLR #I
// 0001 0011 aaaa aiii
// bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void sbclr(const UDSPInstruction opc)
{
  u8 bit = (opc & 0x7) + 6;
  g_dsp.r.sr &= ~(1 << bit);
}

// SBSET #I
// 0001 0010 aaaa aiii
// Set bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void sbset(const UDSPInstruction opc)
{
  u8 bit = (opc & 0x7) + 6;
  g_dsp.r.sr |= (1 << bit);
}

// This is a bunch of flag setters, flipping bits in SR.
void srbith(const UDSPInstruction opc)
{
  ZeroWriteBackLog();
  switch ((opc >> 8) & 0xf)
  {
  case 0xa:  // M2
    g_dsp.r.sr &= ~SR_MUL_MODIFY;
    break;
  case 0xb:  // M0
    g_dsp.r.sr |= SR_MUL_MODIFY;
    break;
  case 0xc:  // CLR15
    g_dsp.r.sr &= ~SR_MUL_UNSIGNED;
    break;
  case 0xd:  // SET15
    g_dsp.r.sr |= SR_MUL_UNSIGNED;
    break;
  case 0xe:  // SET16 (CLR40)
    g_dsp.r.sr &= ~SR_40_MODE_BIT;
    break;
  case 0xf:  // SET40
    g_dsp.r.sr |= SR_40_MODE_BIT;
    break;
  default:
    break;
  }
}
}  // namespace DSP::Interpreter
