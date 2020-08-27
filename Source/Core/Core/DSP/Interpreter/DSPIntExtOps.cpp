// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/Interpreter/DSPIntExtOps.h"

#include "Core/DSP/DSPMemoryMap.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/Interpreter/DSPIntUtil.h"

// not needed for game ucodes (it slows down interpreter/dspjit32 + easier to compare int VS
// dspjit64 without it)
//#define PRECISE_BACKLOG

// Extended opcodes do not exist on their own. These opcodes can only be
// attached to opcodes that allow extending (8 (or 7) lower bits of opcode not used by
// opcode). Extended opcodes do not modify program counter $pc register.

// Most of the suffixes increment or decrement one or more addressing registers
// (the first four, ARx). The increment/decrement is either 1, or the
// corresponding "index" register (the second four, IXx). The addressing
// registers will wrap in odd ways, dictated by the corresponding wrapping
// register, WR0-3.

namespace DSP
{
static void WriteToBackLog(int i, int idx, u16 value)
{
  writeBackLog[i] = value;
  writeBackLogIdx[i] = idx;
}

namespace Interpreter::Ext
{
static bool IsSameMemArea(u16 a, u16 b)
{
  // LM: tested on Wii
  return (a >> 10) == (b >> 10);
}

// DR $arR
// xxxx xxxx 0000 01rr
// Decrement addressing register $arR.
void dr(const UDSPInstruction opc)
{
  WriteToBackLog(0, opc & 0x3, dsp_decrement_addr_reg(opc & 0x3));
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void ir(const UDSPInstruction opc)
{
  WriteToBackLog(0, opc & 0x3, dsp_increment_addr_reg(opc & 0x3));
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void nr(const UDSPInstruction opc)
{
  u8 reg = opc & 0x3;

  WriteToBackLog(0, reg, dsp_increase_addr_reg(reg, (s16)g_dsp.r.ix[reg]));
}

// MV $axD.D, $acS.S
// xxxx xxxx 0001 ddss
// Move value of $acS.S to the $axD.D.
void mv(const UDSPInstruction opc)
{
  u8 sreg = (opc & 0x3) + DSP_REG_ACL0;
  u8 dreg = ((opc >> 2) & 0x3);

  switch (sreg)
  {
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    WriteToBackLog(0, dreg + DSP_REG_AXL0, g_dsp.r.ac[sreg - DSP_REG_ACL0].l);
    break;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    WriteToBackLog(0, dreg + DSP_REG_AXL0, dsp_op_read_reg_and_saturate(sreg - DSP_REG_ACM0));
    break;
  }
}

// S @$arD, $acS.S
// xxxx xxxx 001s s0dd
// Store value of $acS.S in the memory pointed by register $arD.
// Post increment register $arD.
void s(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;

  switch (sreg)
  {
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    dsp_dmem_write(g_dsp.r.ar[dreg], g_dsp.r.ac[sreg - DSP_REG_ACL0].l);
    break;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    dsp_dmem_write(g_dsp.r.ar[dreg], dsp_op_read_reg_and_saturate(sreg - DSP_REG_ACM0));
    break;
  }
  WriteToBackLog(0, dreg, dsp_increment_addr_reg(dreg));
}

// SN @$arD, $acS.S
// xxxx xxxx 001s s1dd
// Store value of register $acS.S in the memory pointed by register $arD.
// Add indexing register $ixD to register $arD.
void sn(const UDSPInstruction opc)
{
  u8 dreg = opc & 0x3;
  u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;

  switch (sreg)
  {
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    dsp_dmem_write(g_dsp.r.ar[dreg], g_dsp.r.ac[sreg - DSP_REG_ACL0].l);
    break;
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    dsp_dmem_write(g_dsp.r.ar[dreg], dsp_op_read_reg_and_saturate(sreg - DSP_REG_ACM0));
    break;
  }
  WriteToBackLog(0, dreg, dsp_increase_addr_reg(dreg, (s16)g_dsp.r.ix[dreg]));
}

// L $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Post increment register $arS.
void l(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x3;
  u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;

  if ((dreg >= DSP_REG_ACM0) && (g_dsp.r.sr & SR_40_MODE_BIT))
  {
    u16 val = dsp_dmem_read(g_dsp.r.ar[sreg]);
    WriteToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) ? 0xFFFF : 0x0000);
    WriteToBackLog(1, dreg, val);
    WriteToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
    WriteToBackLog(3, sreg, dsp_increment_addr_reg(sreg));
  }
  else
  {
    WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[sreg]));
    WriteToBackLog(1, sreg, dsp_increment_addr_reg(sreg));
  }
}

// LN $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS.
// Add indexing register $ixS to register $arS.
void ln(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x3;
  u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;

  if ((dreg >= DSP_REG_ACM0) && (g_dsp.r.sr & SR_40_MODE_BIT))
  {
    u16 val = dsp_dmem_read(g_dsp.r.ar[sreg]);
    WriteToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) ? 0xFFFF : 0x0000);
    WriteToBackLog(1, dreg, val);
    WriteToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
    WriteToBackLog(3, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));
  }
  else
  {
    WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[sreg]));
    WriteToBackLog(1, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));
  }
}

// LS $axD.D, $acS.m108
// xxxx xxxx 10dd 000s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Increment both $ar0 and $ar3.
void ls(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[3], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increment_addr_reg(DSP_REG_AR0));
}

// LSN $axD.D, $acS.m
// xxxx xxxx 10dd 010s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and increment $ar3.
void lsn(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[3], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increase_addr_reg(DSP_REG_AR0, (s16)g_dsp.r.ix[0]));
}

// LSM $axD.D, $acS.m
// xxxx xxxx 10dd 100s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix3 to addressing
// register $ar3 and increment $ar0.
void lsm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[3], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increment_addr_reg(DSP_REG_AR0));
}

// LSMN $axD.D, $acS.m
// xxxx xxxx 10dd 110s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and add corresponding indexing register $ix3 to addressing
// register $ar3.
void lsnm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[3], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[0]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increase_addr_reg(DSP_REG_AR0, (s16)g_dsp.r.ix[0]));
}

// SL $acS.m, $axD.D
// xxxx xxxx 10dd 001s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Increment both $ar0 and $ar3.
void sl(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[0], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increment_addr_reg(DSP_REG_AR0));
}

// SLN $acS.m, $axD.D
// xxxx xxxx 10dd 011s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and increment $ar3.
void sln(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[0], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increase_addr_reg(DSP_REG_AR0, (s16)g_dsp.r.ix[0]));
}

// SLM $acS.m, $axD.D
// xxxx xxxx 10dd 101s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix3 to addressing register $ar3
// and increment $ar0.
void slm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[0], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increment_addr_reg(DSP_REG_AR0));
}

// SLMN $acS.m, $axD.D
// xxxx xxxx 10dd 111s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and add corresponding indexing register $ix3 to addressing register $ar3.
void slnm(const UDSPInstruction opc)
{
  u8 sreg = opc & 0x1;
  u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

  dsp_dmem_write(g_dsp.r.ar[0], dsp_op_read_reg_and_saturate(sreg));

  WriteToBackLog(0, dreg, dsp_dmem_read(g_dsp.r.ar[3]));
  WriteToBackLog(1, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
  WriteToBackLog(2, DSP_REG_AR0, dsp_increase_addr_reg(DSP_REG_AR0, (s16)g_dsp.r.ix[0]));
}

// LD $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 00ss
// example for "nx'ld $AX0.L, $AX1.L, @$AR3"
// Loads the word pointed by AR0 to AX0.H, then loads the word pointed by AR3 to AX0.L.
// Increments AR0 and AR3.
// If AR0 and AR3 point into the same memory page (upper 6 bits of addr are the same -> games are
// not doing that!)
// then the value pointed by AR0 is loaded to BOTH AX0.H and AX0.L.
// If AR0 points into an invalid memory page (ie 0x2000), then AX0.H keeps its old value. (not
// implemented yet)
// If AR3 points into an invalid memory page, then AX0.L gets the same value as AX0.H. (not
// implemented yet)
void ld(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increment_addr_reg(sreg));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}

// LDAX $axR, @$arS
// xxxx xxxx 11sr 0011
void ldax(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  WriteToBackLog(0, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increment_addr_reg(sreg));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}

// LDN $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 01ss
void ldn(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}

// LDAXN $axR, @$arS
// xxxx xxxx 11sr 0111
void ldaxn(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  WriteToBackLog(0, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}

// LDM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 10ss
void ldm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increment_addr_reg(sreg));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
}

// LDAXM $axR, @$arS
// xxxx xxxx 11sr 1011
void ldaxm(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  WriteToBackLog(0, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increment_addr_reg(sreg));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
}

// LDNM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 11ss
void ldnm(const UDSPInstruction opc)
{
  u8 dreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;
  u8 sreg = opc & 0x3;

  WriteToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
}

// LDAXNM $axR, @$arS
// xxxx xxxx 11dr 1111
void ldaxnm(const UDSPInstruction opc)
{
  u8 sreg = (opc >> 5) & 0x1;
  u8 rreg = (opc >> 4) & 0x1;

  WriteToBackLog(0, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r.ar[sreg]));

  if (IsSameMemArea(g_dsp.r.ar[sreg], g_dsp.r.ar[3]))
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[sreg]));
  else
    WriteToBackLog(1, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r.ar[3]));

  WriteToBackLog(2, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r.ix[sreg]));

  WriteToBackLog(3, DSP_REG_AR3, dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r.ix[3]));
}

void nop(const UDSPInstruction opc)
{
}
}  // namespace Interpreter::Ext

// The ext ops are calculated in parallel with the actual op. That means that
// both the main op and the ext op see the same register state as input. The
// output is simple as long as the main and ext ops don't change the same
// register. If they do the output is the bitwise or of the result of both the
// main and ext ops.

// The ext op are writing their output into the backlog which is
// being applied to the real registers after the main op was executed
void ApplyWriteBackLog()
{
  // always make sure to have an extra entry at the end w/ -1 to avoid
  // infinitive loops
  for (int i = 0; writeBackLogIdx[i] != -1; i++)
  {
    u16 value = writeBackLog[i];
#ifdef PRECISE_BACKLOG
    value |= Interpreter::dsp_op_read_reg(writeBackLogIdx[i]);
#endif
    Interpreter::dsp_op_write_reg(writeBackLogIdx[i], value);

    // Clear back log
    writeBackLogIdx[i] = -1;
  }
}

// This function is being called in the main op after all input regs were read
// and before it writes into any regs. This way we can always use bitwise or to
// apply the ext command output, because if the main op didn't change the value
// then 0 | ext output = ext output and if it did then bitwise or is still the
// right thing to do
// Only needed for cases when mainop and extended are modifying the same ACC
// Games are not doing that + in motorola (similar DSP) dox this is forbidden to do.
void ZeroWriteBackLog()
{
#ifdef PRECISE_BACKLOG
  // always make sure to have an extra entry at the end w/ -1 to avoid
  // infinitive loops
  for (int i = 0; writeBackLogIdx[i] != -1; i++)
  {
    Interpreter::dsp_op_write_reg(writeBackLogIdx[i], 0);
  }
#endif
}

void ZeroWriteBackLogPreserveAcc(u8 acc)
{
#ifdef PRECISE_BACKLOG
  for (int i = 0; writeBackLogIdx[i] != -1; i++)
  {
    // acc0
    if ((acc == 0) &&
        ((writeBackLogIdx[i] == DSP_REG_ACL0) || (writeBackLogIdx[i] == DSP_REG_ACM0) ||
         (writeBackLogIdx[i] == DSP_REG_ACH0)))
      continue;

    // acc1
    if ((acc == 1) &&
        ((writeBackLogIdx[i] == DSP_REG_ACL1) || (writeBackLogIdx[i] == DSP_REG_ACM1) ||
         (writeBackLogIdx[i] == DSP_REG_ACH1)))
      continue;

    Interpreter::dsp_op_write_reg(writeBackLogIdx[i], 0);
  }
#endif
}
}  // namespace DSP
