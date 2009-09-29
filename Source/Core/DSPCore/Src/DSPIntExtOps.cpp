// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#include "DSPIntUtil.h"
#include "DSPMemoryMap.h"
#include "DSPIntExtOps.h"

// Extended opcodes do not exist on their own. These opcodes can only be
// attached to opcodes that allow extending (8 lower bits of opcode not used by
// opcode). Extended opcodes do not modify program counter $pc register.

// Most of the suffixes increment or decrement one or more addressing registers
// (the first four, ARx). The increment/decrement is either 1, or the corresponding 
// "index" register (the second four, IXx). The addressing registers will wrap
// in odd ways, dictated by the corresponding wrapping register, WP0-3.

// The following should be applied as a decrement (and is applied by dsp_decrement_addr_reg):
// ar[i] = (ar[i] & wp[i]) == 0 ? ar[i] | wp[i] : ar[i] - 1;
// I have not found the corresponding algorithms for increments yet.
// It's gotta be fairly simple though. See R3123, R3125 in Google Code.
// (May have something to do with (ar[i] ^ wp[i]) == 0)


namespace DSPInterpreter
{

namespace Ext 
{

inline bool IsSameMemArea(u16 a, u16 b)
{
//LM: tested on WII
	if ((a>>10)==(b>>10))
		return true;
	else
		return false;
}

// DR $arR
// xxxx xxxx 0000 01rr
// Decrement addressing register $arR.
void dr(const UDSPInstruction& opc) {
	writeToBackLog(0, opc.hex & 0x3, dsp_decrement_addr_reg(opc.hex & 0x3));
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void ir(const UDSPInstruction& opc) {
	writeToBackLog(0, opc.hex & 0x3, dsp_increment_addr_reg(opc.hex & 0x3));
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void nr(const UDSPInstruction& opc) {
	u8 reg = opc.hex & 0x3;	
	
	writeToBackLog(0, reg, dsp_increase_addr_reg(reg, (s16)g_dsp.r[DSP_REG_IX0 + reg]));
}

// MV $axD, $acS.l
// xxxx xxxx 0001 ddss
// Move value of $acS.l to the $axD.l.
void mv(const UDSPInstruction& opc)
{
 	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 2) & 0x3);
	
	writeToBackLog(0, dreg + DSP_REG_AXL0, g_dsp.r[sreg + DSP_REG_ACC0]);
}
	
// S @$D, $acD.l
// xxxx xxxx 001s s0dd
// Store value of $(acS.l) in the memory pointed by register $D.
// Post increment register $D.
void s(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = ((opc.hex >> 3) & 0x3) + DSP_REG_ACC0;

	dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);
	writeToBackLog(0, dreg,	dsp_increment_addr_reg(dreg));
}

// SN @$D, $acD.l
// xxxx xxxx 001s s1dd
// Store value of register $acS in the memory pointed by register $D.
// Add indexing register $ixD to register $D.
void sn(const UDSPInstruction& opc)
{
	u8 dreg = opc.hex & 0x3;
	u8 sreg = ((opc.hex >> 3) & 0x3) + DSP_REG_ACC0;

	dsp_dmem_write(g_dsp.r[dreg], g_dsp.r[sreg]);

	writeToBackLog(0, dreg,	dsp_increase_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + dreg]));
}

// L axD.l, @$S
// xxxx xxxx 01dd d0ss
// Load $axD with value from memory pointed by register $S. 
// Post increment register $S.
void l(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + DSP_REG_AXL0;
	
	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg, dsp_increment_addr_reg(sreg));
}

// LN axD.l, @$S
// xxxx xxxx 01dd d0ss
// Load $axD with value from memory pointed by register $S. 
// Add indexing register register $ixS to register $S.
void ln(const UDSPInstruction& opc)
{
	u8 sreg = opc.hex & 0x3;
	u8 dreg = ((opc.hex >> 3) & 0x7) + DSP_REG_AXL0;

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg, dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
}

// LS $axD.l, $acS.m
// xxxx xxxx 10dd 000s
// Load register $axD.l with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Increment both $ar0 and $ar3.
void ls(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 sreg = 0x03;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;

	dsp_dmem_write(g_dsp.r[sreg], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[0x00]));
	writeToBackLog(1, sreg,	dsp_increment_addr_reg(sreg));
	writeToBackLog(2, 0x00,	dsp_increment_addr_reg(0x00)); 
}


// LSN $acD.l, $acS.m
// xxxx xxxx 10dd 010s
// Load register $acD.l with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and increment $ar3.
void lsn(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 sreg = 0x03;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;

	dsp_dmem_write(g_dsp.r[sreg], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[0x00]));
	writeToBackLog(1, sreg,	dsp_increment_addr_reg(sreg));
	writeToBackLog(2, 0x00,dsp_increase_addr_reg(0x00, (s16)g_dsp.r[DSP_REG_IX0]));
}

// LSM $acD.l, $acS.m
// xxxx xxxx 10dd 100s
// Load register $acD.l with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix3 to addressing
// register $ar3 and increment $ar0.
void lsm(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 sreg = 0x03;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	
	dsp_dmem_write(g_dsp.r[sreg], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[0x00]));
	writeToBackLog(1, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	writeToBackLog(2, 0x00,	dsp_increment_addr_reg(0x00));
}

// LSMN $acD.l, $acS.m
// xxxx xxxx 10dd 110s
// Load register $acD.l with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix0 to addressing
// register $ar0 and add corresponding indexing register $ix3 to addressing
// register $ar3.
void lsnm(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 sreg = 0x03;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	
	dsp_dmem_write(g_dsp.r[sreg], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[0x00]));
	writeToBackLog(1, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	writeToBackLog(2, 0x00, dsp_increase_addr_reg(0x00, (s16)g_dsp.r[DSP_REG_IX0]));
}

// SL $acS.m, $acD.l
// xxxx xxxx 10dd 001s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $acD.l with value from memory pointed by register
// $ar3. Increment both $ar0 and $ar3.
void sl(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	const u8 sreg = 0x03;
	
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg,	dsp_increment_addr_reg(sreg));
	writeToBackLog(2, 0x00,	dsp_increment_addr_reg(0x00)); 
}

// SLN $acS.m, $acD.l
// xxxx xxxx 10dd 011s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $acD.l with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and increment $ar3.
void sln(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	const u8 sreg = 0x03;
	
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);
	
	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg,	dsp_increment_addr_reg(sreg));
	writeToBackLog(2, 0x00, dsp_increase_addr_reg(0x00, (s16)g_dsp.r[DSP_REG_IX0]));
}

// SLM $acS.m, $acD.l
// xxxx xxxx 10dd 101s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $acD.l with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix3 to addressing register $ar3
// and increment $ar0.
void slm(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	const u8 sreg = 0x03;
	
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	writeToBackLog(2, 0x00,	dsp_increment_addr_reg(0x00));
}

// SLMN $acS.m, $acD.l
// xxxx xxxx 10dd 111s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $acD.l with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix0 to addressing register $ar0
// and add corresponding indexing register $ix3 to addressing register $ar3.
void slnm(const UDSPInstruction& opc)
{
	u8 areg = (opc.hex & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc.hex >> 4) & 0x3) + DSP_REG_AXL0;
	const u8 sreg = 0x03;
	
	dsp_dmem_write(g_dsp.r[0x00], g_dsp.r[areg]);

	writeToBackLog(0, dreg,	dsp_dmem_read(g_dsp.r[sreg]));
	writeToBackLog(1, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	writeToBackLog(2, 0x00, dsp_increase_addr_reg(0x00, (s16)g_dsp.r[DSP_REG_IX0]));
}

// Not in duddie's doc
// LD $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 00ss
void ld(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x1;
	u8 rreg = (opc.hex >> 4) & 0x1;
	u8 sreg = opc.hex & 0x3;

	if (sreg != 0x03) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg],g_dsp.r[0x3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, sreg,	dsp_increment_addr_reg(sreg));
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg],g_dsp.r[0x3]))	
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, dreg,	dsp_increment_addr_reg(dreg));
	}

	writeToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}

// Not in duddie's doc
// LDN $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 01ss
void ldn(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x1;
	u8 rreg = (opc.hex >> 4) & 0x1;
	u8 sreg = opc.hex & 0x3;
	
	if (sreg != 0x03) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg],g_dsp.r[0x3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg],g_dsp.r[0x3]))	
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, dreg,	dsp_increase_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + dreg]));
	}
	
	writeToBackLog(3, DSP_REG_AR3, dsp_increment_addr_reg(DSP_REG_AR3));
}


// Not in duddie's doc
// LDM $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 10ss
void ldm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x1;
	u8 rreg = (opc.hex >> 4) & 0x1;
	u8 sreg = opc.hex & 0x3;

	if (sreg != 0x03) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg],g_dsp.r[0x3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, sreg,	dsp_increment_addr_reg(sreg));
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg],g_dsp.r[0x3]))	
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, dreg,	dsp_increment_addr_reg(dreg));
	}

	writeToBackLog(3, DSP_REG_AR3,
				   dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r[DSP_REG_IX0 + DSP_REG_AR3]));
}

// Not in duddie's doc
// LDNM $ax0.d $ax1.r @$arS
// xxxx xxxx 11dr 11ss
void ldnm(const UDSPInstruction& opc)
{
	u8 dreg = (opc.hex >> 5) & 0x1;
	u8 rreg = (opc.hex >> 4) & 0x1;
	u8 sreg = opc.hex & 0x3;

	if (sreg != 0x03) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg],g_dsp.r[0x3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, sreg,	dsp_increase_addr_reg(sreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]));
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXL0, dsp_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg],g_dsp.r[0x3]))	
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXH0, dsp_dmem_read(g_dsp.r[0x3]));

		writeToBackLog(2, dreg,	dsp_increase_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + dreg]));
	}

	writeToBackLog(3, DSP_REG_AR3,
				   dsp_increase_addr_reg(DSP_REG_AR3, (s16)g_dsp.r[DSP_REG_IX0 + DSP_REG_AR3]));
}


void nop(const UDSPInstruction& opc)
{
}

} // end namespace ext
} // end namespace DSPInterpeter

void applyWriteBackLog()
{
	//always make sure to have an extra entry at the end w/ -1 to avoid
	//infinitive loops
	for (int i=0;writeBackLogIdx[i]!=-1;i++) {
		dsp_op_write_reg(writeBackLogIdx[i], g_dsp.r[writeBackLogIdx[i]] | writeBackLog[i]);
		// Clear back log
		writeBackLogIdx[i] = -1;
	}
}

void zeroWriteBackLog() 
{
	//always make sure to have an extra entry at the end w/ -1 to avoid
	//infinitive loops
	for (int i=0;writeBackLogIdx[i]!=-1;i++) 
		dsp_op_write_reg(writeBackLogIdx[i], 0);
}

