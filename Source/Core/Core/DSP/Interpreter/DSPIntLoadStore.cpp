// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "Common/CommonTypes.h"

namespace DSP::Interpreter
{
// SRSH @M, $acS.h
// 0010 10ss mmmm mmmm
// Move value from register $acS.h to data memory pointed by address
// CR[0-7] | M. That is, the upper 8 bits of the address are the
// bottom 8 bits from CR, and the lower 8 bits are from the 8-bit immediate.
void Interpreter::srsh(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const auto reg = static_cast<u8>(((opc >> 8) & 0x1) + DSP_REG_ACH0);
  const auto addr = static_cast<u16>((state.r.cr << 8) | (opc & 0xFF));

  state.WriteDMEM(addr, OpReadRegister(reg));
}

// SRS @M, $(0x1C+S)
// 0010 11ss mmmm mmmm
// Move value from register $(0x1C+S) to data memory pointed by address
// CR[0-7] | M. That is, the upper 8 bits of the address are the
// bottom 8 bits from CR, and the lower 8 bits are from the 8-bit immediate.
void Interpreter::srs(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const auto reg = static_cast<u8>(((opc >> 8) & 0x3) + DSP_REG_ACL0);
  const auto addr = static_cast<u16>((state.r.cr << 8) | (opc & 0xFF));

  state.WriteDMEM(addr, OpReadRegister(reg));
}

// LRS $(0x18+D), @M
// 0010 0ddd mmmm mmmm
// Move value from data memory pointed by address CR[0-7] | M to register
// $(0x18+D).  That is, the upper 8 bits of the address are the bottom 8 bits
// from CR, and the lower 8 bits are from the 8-bit immediate.
void Interpreter::lrs(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const auto reg = static_cast<u8>(((opc >> 8) & 0x7) + 0x18);
  const auto addr = static_cast<u16>((state.r.cr << 8) | (opc & 0xFF));

  OpWriteRegister(reg, state.ReadDMEM(addr));
  ConditionalExtendAccum(reg);
}

// LR $D, @M
// 0000 0000 110d dddd
// mmmm mmmm mmmm mmmm
// Move value from data memory pointed by address M to register $D.
void Interpreter::lr(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 reg = opc & 0x1F;
  const u16 addr = state.FetchInstruction();
  const u16 val = state.ReadDMEM(addr);

  OpWriteRegister(reg, val);
  ConditionalExtendAccum(reg);
}

// SR @M, $S
// 0000 0000 111s ssss
// mmmm mmmm mmmm mmmm
// Store value from register $S to a memory pointed by address M.
void Interpreter::sr(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u8 reg = opc & 0x1F;
  const u16 addr = state.FetchInstruction();

  state.WriteDMEM(addr, OpReadRegister(reg));
}

// SI @M, #I
// 0001 0110 mmmm mmmm
// iiii iiii iiii iiii
// Store 16-bit immediate value I to a memory location pointed by address
// M (M is 8-bit value sign extended).
void Interpreter::si(const UDSPInstruction opc)
{
  auto& state = m_dsp_core.DSPState();
  const u16 addr = static_cast<u16>(static_cast<s8>(opc));
  const u16 imm = state.FetchInstruction();

  state.WriteDMEM(addr, imm);
}

// LRR $D, @$arS
// 0001 1000 0ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
void Interpreter::lrr(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x3;
  const u8 dreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  const u16 val = state.ReadDMEM(OpReadRegister(sreg));
  OpWriteRegister(dreg, val);
  ConditionalExtendAccum(dreg);
}

// LRRD $D, @$arS
// 0001 1000 1ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Decrement register $arS.
void Interpreter::lrrd(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x3;
  const u8 dreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  const u16 val = state.ReadDMEM(OpReadRegister(sreg));
  OpWriteRegister(dreg, val);
  ConditionalExtendAccum(dreg);
  state.r.ar[sreg] = DecrementAddressRegister(sreg);
}

// LRRI $D, @$arS
// 0001 1001 0ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Increment register $arS.
void Interpreter::lrri(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x3;
  const u8 dreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  const u16 val = state.ReadDMEM(OpReadRegister(sreg));
  OpWriteRegister(dreg, val);
  ConditionalExtendAccum(dreg);
  state.r.ar[sreg] = IncrementAddressRegister(sreg);
}

// LRRN $D, @$arS
// 0001 1001 1ssd dddd
// Move value from data memory pointed by addressing register $arS to register $D.
// Add corresponding indexing register $ixS to register $arS.
void Interpreter::lrrn(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x3;
  const u8 dreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  const u16 val = state.ReadDMEM(OpReadRegister(sreg));
  OpWriteRegister(dreg, val);
  ConditionalExtendAccum(dreg);
  state.r.ar[sreg] = IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg]));
}

// SRR @$arD, $S
// 0001 1010 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD.
void Interpreter::srr(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x3;
  const u8 sreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));
}

// SRRD @$arD, $S
// 0001 1010 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Decrement register $arD.
void Interpreter::srrd(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x3;
  const u8 sreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));

  state.r.ar[dreg] = DecrementAddressRegister(dreg);
}

// SRRI @$arD, $S
// 0001 1011 0dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Increment register $arD.
void Interpreter::srri(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x3;
  const u8 sreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));

  state.r.ar[dreg] = IncrementAddressRegister(dreg);
}

// SRRN @$arD, $S
// 0001 1011 1dds ssss
// Store value from source register $S to a memory location pointed by
// addressing register $arD. Add corresponding indexing register $ixD to register $arD.
void Interpreter::srrn(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x3;
  const u8 sreg = opc & 0x1f;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));

  state.r.ar[dreg] = IncreaseAddressRegister(dreg, static_cast<s16>(state.r.ix[dreg]));
}

// ILRR $acD.m, @$arS
// 0000 001d 0001 00ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m.
void Interpreter::ilrr(const UDSPInstruction opc)
{
  const u16 reg = opc & 0x3;
  const u16 dreg = DSP_REG_ACM0 + ((opc >> 8) & 1);
  auto& state = m_dsp_core.DSPState();

  state.r.ac[dreg - DSP_REG_ACM0].m = state.ReadIMEM(state.r.ar[reg]);
  ConditionalExtendAccum(dreg);
}

// ILRRD $acD.m, @$arS
// 0000 001d 0001 01ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Decrement addressing register $arS.
void Interpreter::ilrrd(const UDSPInstruction opc)
{
  const u16 reg = opc & 0x3;
  const u16 dreg = DSP_REG_ACM0 + ((opc >> 8) & 1);
  auto& state = m_dsp_core.DSPState();

  state.r.ac[dreg - DSP_REG_ACM0].m = state.ReadIMEM(state.r.ar[reg]);
  ConditionalExtendAccum(dreg);
  state.r.ar[reg] = DecrementAddressRegister(reg);
}

// ILRRI $acD.m, @$S
// 0000 001d 0001 10ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Increment addressing register $arS.
void Interpreter::ilrri(const UDSPInstruction opc)
{
  const u16 reg = opc & 0x3;
  const u16 dreg = DSP_REG_ACM0 + ((opc >> 8) & 1);
  auto& state = m_dsp_core.DSPState();

  state.r.ac[dreg - DSP_REG_ACM0].m = state.ReadIMEM(state.r.ar[reg]);
  ConditionalExtendAccum(dreg);
  state.r.ar[reg] = IncrementAddressRegister(reg);
}

// ILRRN $acD.m, @$arS
// 0000 001d 0001 11ss
// Move value from instruction memory pointed by addressing register
// $arS to mid accumulator register $acD.m. Add corresponding indexing
// register $ixS to addressing register $arS.
void Interpreter::ilrrn(const UDSPInstruction opc)
{
  const u16 reg = opc & 0x3;
  const u16 dreg = DSP_REG_ACM0 + ((opc >> 8) & 1);
  auto& state = m_dsp_core.DSPState();

  state.r.ac[dreg - DSP_REG_ACM0].m = state.ReadIMEM(state.r.ar[reg]);
  ConditionalExtendAccum(dreg);
  state.r.ar[reg] = IncreaseAddressRegister(reg, static_cast<s16>(state.r.ix[reg]));
}
}  // namespace DSP::Interpreter
