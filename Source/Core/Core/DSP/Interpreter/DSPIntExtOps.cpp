// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Interpreter/DSPInterpreter.h"

// Extended opcodes do not exist on their own. These opcodes can only be
// attached to opcodes that allow extending (8 (or 7) lower bits of opcode not used by
// opcode). Extended opcodes do not modify program counter $pc register.

// Most of the suffixes increment or decrement one or more addressing registers
// (the first four, ARx). The increment/decrement is either 1, or the
// corresponding "index" register (the second four, IXx). The addressing
// registers will wrap in odd ways, dictated by the corresponding wrapping
// register, WR0-3.

namespace DSP::Interpreter
{
static bool IsSameMemArea(u16 a, u16 b)
{
  // LM: tested on Wii
  return (a >> 10) == (b >> 10);
}

// DR $arR
// xxxx xxxx 0000 01rr
// Decrement addressing register $arR.
void Interpreter::dr(const UDSPInstruction opc)
{
  WriteToBackLog(0, opc & 0x3, DecrementAddressRegister(opc & 0x3));
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void Interpreter::ir(const UDSPInstruction opc)
{
  WriteToBackLog(0, opc & 0x3, IncrementAddressRegister(opc & 0x3));
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void Interpreter::nr(const UDSPInstruction opc)
{
  const u8 reg = opc & 0x3;
  const auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, reg, IncreaseAddressRegister(reg, static_cast<s16>(state.r.ix[reg])));
}

// MV $axD.D, $acS.S
// xxxx xxxx 0001 ddss
// Move value of $acS.S to the $axD.D.
void Interpreter::mv(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x3) + DSP_REG_ACL0;
  const u8 dreg = ((opc >> 2) & 0x3);

  WriteToBackLog(0, dreg + DSP_REG_AXL0, OpReadRegister(sreg));
}

// S @$arD, $acS.S
// xxxx xxxx 001s s0dd
// Store value of $acS.S in the memory pointed by register $arD.
// Post increment register $arD.
void Interpreter::s(const UDSPInstruction opc)
{
  const u8 dreg = opc & 0x3;
  const u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));
  WriteToBackLog(0, dreg, IncrementAddressRegister(dreg));
}

// SN @$arD, $acS.S
// xxxx xxxx 001s s1dd
// Store value of register $acS.S in the memory pointed by register $arD.
// Add indexing register $ixD to register $arD.
void Interpreter::sn(const UDSPInstruction opc)
{
  const u8 dreg = opc & 0x3;
  const u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[dreg], OpReadRegister(sreg));
  WriteToBackLog(0, dreg, IncreaseAddressRegister(dreg, static_cast<s16>(state.r.ix[dreg])));
}

// L $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Post increment register $arS.
void Interpreter::l(const UDSPInstruction opc)
{
  const u8 sreg = opc & 0x3;
  const u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  if (dreg >= DSP_REG_ACM0 && IsSRFlagSet(SR_40_MODE_BIT))
  {
    const u16 val = state.ReadDMEM(state.r.ar[sreg]);
    WriteToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) != 0 ? 0xFFFF : 0x0000);
    WriteToBackLog(1, dreg, val);
    WriteToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
    WriteToBackLog(3, sreg, IncrementAddressRegister(sreg));
  }
  else
  {
    WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[sreg]));
    WriteToBackLog(1, sreg, IncrementAddressRegister(sreg));
  }
}

// LN $axD.D, @$arS
// xxxx xxxx 01dd d1ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Add indexing register $ixS to register $arS.
void Interpreter::ln(const UDSPInstruction opc)
{
  const u8 sreg = opc & 0x3;
  const u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  if (dreg >= DSP_REG_ACM0 && IsSRFlagSet(SR_40_MODE_BIT))
  {
    const u16 val = state.ReadDMEM(state.r.ar[sreg]);
    WriteToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) != 0 ? 0xFFFF : 0x0000);
    WriteToBackLog(1, dreg, val);
    WriteToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
    WriteToBackLog(3, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));
  }
  else
  {
    WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[sreg]));
    WriteToBackLog(1, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));
  }
}

// LS $axD.D, $acS.m
// xxxx xxxx 10dd 000s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Increment both $ar0 and $ar3.
void Interpreter::ls(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[3], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, IncrementAddressRegister(DSP_REG_AR0));
}

// LSN $axD.D, $acS.m
// xxxx xxxx 10dd 010s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and increment $ar3.
void Interpreter::lsn(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[3], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0,
                 IncreaseAddressRegister(DSP_REG_AR0, static_cast<s16>(state.r.ix[0])));
}

// LSM $axD.D, $acS.m
// xxxx xxxx 10dd 100s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix3 to addressing
// register $ar3 and increment $ar0.
void Interpreter::lsm(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[3], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
  WriteToBackLog(2, DSP_REG_AR0, IncrementAddressRegister(DSP_REG_AR0));
}

// LSMN $axD.D, $acS.m
// xxxx xxxx 10dd 110s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and add corresponding indexing register $ix3 to addressing
// register $ar3.
void Interpreter::lsnm(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[3], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
  WriteToBackLog(2, DSP_REG_AR0,
                 IncreaseAddressRegister(DSP_REG_AR0, static_cast<s16>(state.r.ix[0])));
}

// SL $acS.m, $axD.D
// xxxx xxxx 10dd 001s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Increment both $ar0 and $ar3.
void Interpreter::sl(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[0], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, IncrementAddressRegister(DSP_REG_AR0));
}

// SLN $acS.m, $axD.D
// xxxx xxxx 10dd 011s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and increment $ar3.
void Interpreter::sln(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[0], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0,
                 IncreaseAddressRegister(DSP_REG_AR0, static_cast<s16>(state.r.ix[0])));
}

// SLM $acS.m, $axD.D
// xxxx xxxx 10dd 101s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix3 to addressing register $ar3
// and increment $ar0.
void Interpreter::slm(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[0], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
  WriteToBackLog(2, DSP_REG_AR0, IncrementAddressRegister(DSP_REG_AR0));
}

// SLMN $acS.m, $axD.D
// xxxx xxxx 10dd 111s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and add corresponding indexing register $ix3 to addressing register $ar3.
void Interpreter::slnm(const UDSPInstruction opc)
{
  const u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
  const u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
  auto& state = m_dsp_core.DSPState();

  state.WriteDMEM(state.r.ar[0], OpReadRegister(sreg));

  WriteToBackLog(0, dreg, state.ReadDMEM(state.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
  WriteToBackLog(2, DSP_REG_AR0,
                 IncreaseAddressRegister(DSP_REG_AR0, static_cast<s16>(state.r.ix[0])));
}

// LD $ax0.D, $ax1.R, @$arS
// xxxx xxxx 11dr 00ss
// Load register $ax0.D (either $ax0.l or $ax0.h) with value from memory pointed by register $arS.
// Load register $ax1.R (either $ax1.l or $ax1.h) with value from memory pointed by register $ar3.
// Increment both $arS and $ar3.
// S cannot be 3, as that encodes LDAX.  Thus $arS and $ar3 are known to be distinct.
// If $ar0 and $ar3 point into the same memory page (upper 6 bits of addr are the same -> games are
// not doing that!) then the value pointed by $ar0 is loaded to BOTH $ax0.D and $ax1.R.
// If $ar0 points into an invalid memory page (ie 0x2000), then $ax0.D keeps its old value. (not
// implemented yet)
// If $ar3 points into an invalid memory page, then $ax1.R gets the same value as $ax0.D. (not
// implemented yet)
void Interpreter::ld(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  const u8 sreg = opc & 0x3;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncrementAddressRegister(sreg));

  WriteToBackLog(3, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
}

// LDAX $axR, @$arS
// xxxx xxxx 11sr 0011
// Load register $axR.h with value from memory pointed by register $arS.
// Load register $axR.l with value from memory pointed by register $ar3.
// Increment both $arS and $ar3.
void Interpreter::ldax(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, rreg + DSP_REG_AXH0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncrementAddressRegister(sreg));

  WriteToBackLog(3, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
}

// LDN $ax0.D, $ax1.R, @$arS
// xxxx xxxx 11dr 01ss
void Interpreter::ldn(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  const u8 sreg = opc & 0x3;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));

  WriteToBackLog(3, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
}

// LDAXN $axR, @$arS
// xxxx xxxx 11sr 0111
void Interpreter::ldaxn(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, rreg + DSP_REG_AXH0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));

  WriteToBackLog(3, DSP_REG_AR3, IncrementAddressRegister(DSP_REG_AR3));
}

// LDM $ax0.D, $ax1.R, @$arS
// xxxx xxxx 11dr 10ss
void Interpreter::ldm(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  const u8 sreg = opc & 0x3;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncrementAddressRegister(sreg));

  WriteToBackLog(3, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
}

// LDAXM $axR, @$arS
// xxxx xxxx 11sr 1011
void Interpreter::ldaxm(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, rreg + DSP_REG_AXH0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncrementAddressRegister(sreg));

  WriteToBackLog(3, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
}

// LDNM $ax0.D, $ax1.R, @$arS
// xxxx xxxx 11dr 11ss
void Interpreter::ldnm(const UDSPInstruction opc)
{
  const u8 dreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  const u8 sreg = opc & 0x3;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));

  WriteToBackLog(3, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
}

// LDAXNM $axR, @$arS
// xxxx xxxx 11dr 1111
void Interpreter::ldaxnm(const UDSPInstruction opc)
{
  const u8 sreg = (opc >> 5) & 0x1;
  const u8 rreg = (opc >> 4) & 0x1;
  auto& state = m_dsp_core.DSPState();

  WriteToBackLog(0, rreg + DSP_REG_AXH0, state.ReadDMEM(state.r.ar[sreg]));

  if (IsSameMemArea(state.r.ar[sreg], state.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, state.ReadDMEM(state.r.ar[3]));

  WriteToBackLog(2, sreg, IncreaseAddressRegister(sreg, static_cast<s16>(state.r.ix[sreg])));

  WriteToBackLog(3, DSP_REG_AR3,
                 IncreaseAddressRegister(DSP_REG_AR3, static_cast<s16>(state.r.ix[3])));
}

void Interpreter::nop_ext(const UDSPInstruction)
{
}
}  // namespace DSP::Interpreter
