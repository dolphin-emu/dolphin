// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"

namespace DSP::Interpreter
{
// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
void Interpreter::mrr(const UDSPInstruction opc)
{
  const u8 sreg = opc & 0x1f;
  const u8 dreg = (opc >> 5) & 0x1f;

  OpWriteRegister(dreg, OpReadRegister(sreg));

  ConditionalExtendAccum(dreg);
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
void Interpreter::lri(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 reg = opc & 0x1F;
  const u16 imm = state.FetchInstruction();

  OpWriteRegister(reg, imm);
  ConditionalExtendAccum(reg);
}

// LRIS $(0x18+D), #I
// 0000 1ddd iiii iiii
// Load immediate value I (8-bit sign extended) to accumulator register.
void Interpreter::lris(const UDSPInstruction opc)
{
  const u8 reg = ((opc >> 8) & 0x7) + DSP_REG_AXL0;
  const u16 imm = static_cast<u16>(static_cast<s8>(opc));

  OpWriteRegister(reg, imm);
  ConditionalExtendAccum(reg);
}

//----

// NX
// 1000 -000 xxxx xxxx
// No operation, but can be extended with extended opcode.
// This opcode is supposed to do nothing - it's used if you want to use
// an opcode extension but not do anything. At least according to duddie.
void Interpreter::nx(const UDSPInstruction)
{
  ZeroWriteBackLog();
}

//----

// DAR $arD
// 0000 0000 0000 01dd
// Decrement address register $arD.
void Interpreter::dar(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 index = opc & 3;

  state.r.ar[index] = DecrementAddressRegister(index);
}

// IAR $arD
// 0000 0000 0000 10dd
// Increment address register $arD.
void Interpreter::iar(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 index = opc & 3;

  state.r.ar[index] = IncrementAddressRegister(index);
}

// SUBARN $arD
// 0000 0000 0000 11dd
// Subtract indexing register $ixD from an addressing register $arD.
// used only in IPL-NTSC ucode
void Interpreter::subarn(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 dreg = opc & 0x3;

  state.r.ar[dreg] = DecreaseAddressRegister(dreg, static_cast<s16>(state.r.ix[dreg]));
}

// ADDARN $arD, $ixS
// 0000 0000 0001 ssdd
// Adds indexing register $ixS to an addressing register $arD.
// It is critical for the Zelda ucode that this one wraps correctly.
void Interpreter::addarn(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 dreg = opc & 0x3;
  const u8 sreg = (opc >> 2) & 0x3;

  state.r.ar[dreg] = IncreaseAddressRegister(dreg, static_cast<s16>(state.r.ix[sreg]));
}

//----

// SBCLR #I
// 0001 0010 aaaa aiii
// Clear bit of status register $sr. Bit number is calculated by adding 6 to immediate value I;
// thus, bits 6 through 13 (LZ through AM) can be cleared with this instruction.
void Interpreter::sbclr(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 bit = (opc & 0x7) + 6;

  state.r.sr &= ~(1U << bit);
}

// SBSET #I
// 0001 0011 aaaa aiii
// Set bit of status register $sr. Bit number is calculated by adding 6 to immediate value I;
// thus, bits 6 through 13 (LZ through AM) can be set with this instruction.
void Interpreter::sbset(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 bit = (opc & 0x7) + 6;

  state.r.sr |= (1U << bit);
}

// This is a bunch of flag setters, flipping bits in SR.
void Interpreter::srbith(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();

  ZeroWriteBackLog();
  switch ((opc >> 8) & 0x7)
  {
  case 2:  // M2
    state.r.sr &= ~SR_MUL_MODIFY;
    break;
  case 3:  // M0
    state.r.sr |= SR_MUL_MODIFY;
    break;
  case 4:  // CLR15
    state.r.sr &= ~SR_MUL_UNSIGNED;
    break;
  case 5:  // SET15
    state.r.sr |= SR_MUL_UNSIGNED;
    break;
  case 6:  // SET16 (CLR40)
    state.r.sr &= ~SR_40_MODE_BIT;
    break;
  case 7:  // SET40
    state.r.sr |= SR_40_MODE_BIT;
    break;
  default:
    break;
  }
}
}  // namespace DSP::Interpreter
