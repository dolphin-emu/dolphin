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
#include "../DSPMemoryMap.h"
#include "../DSPEmitter.h"
#include "../DSPIntExtOps.h" // remove when getting rid of writebacklog
// See docs in the interpeter

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
void DSPEmitter::dr(const UDSPInstruction opc) {
	decrement_addr_reg(opc & 0x3);
}

// IR $arR
// xxxx xxxx 0000 10rr
// Increment addressing register $arR.
void DSPEmitter::ir(const UDSPInstruction opc) {
	increment_addr_reg(opc & 0x3);
}

// NR $arR
// xxxx xxxx 0000 11rr
// Add corresponding indexing register $ixR to addressing register $arR.
void DSPEmitter::nr(const UDSPInstruction opc) {
	u8 reg = opc & 0x3;	
	
	increase_addr_reg(reg);
}

// MV $axD.D, $acS.S
// xxxx xxxx 0001 ddss
// Move value of $acS.S to the $axD.D.
void DSPEmitter::mv(const UDSPInstruction opc)
{
 	u8 sreg = (opc & 0x3) + DSP_REG_ACL0;
	u8 dreg = ((opc >> 2) & 0x3);

#if 0 //more tests 
	if ((sreg >= DSP_REG_ACM0) && (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)) 
		writeToBackLog(0, dreg + DSP_REG_AXL0, ((u16)dsp_get_acc_h(sreg-DSP_REG_ACM0) & 0x0080) ? 0x8000 : 0x7fff);
	else
#endif	
		writeToBackLog(0, dreg + DSP_REG_AXL0, g_dsp.r[sreg]);
}
	
// S @$arD, $acS.S
// xxxx xxxx 001s s0dd
// Store value of $acS.S in the memory pointed by register $arD.
// Post increment register $arD.
void DSPEmitter::s(const UDSPInstruction opc)
{
	u8 dreg = opc & 0x3;
	u8 sreg = ((opc >> 3) & 0x3) + DSP_REG_ACL0;

	ext_dmem_write(dreg, sreg);
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

	ext_dmem_write(dreg, sreg);
	increase_addr_reg(dreg);
}

// L $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS. 
// Post increment register $arS.
void DSPEmitter::l(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x3;
	u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;
	
	if ((dreg >= DSP_REG_ACM0) && (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)) 
	{
		u16 val = ext_dmem_read(g_dsp.r[sreg]);
		writeToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) ? 0xFFFF : 0x0000);
		writeToBackLog(1, dreg,	val);
		writeToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
		increment_addr_reg(sreg);
	}
	else
	{
		writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[sreg]));
		increment_addr_reg(sreg);
	}
}

// LN $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS. 
// Add indexing register register $ixS to register $arS.
void DSPEmitter::ln(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x3;
	u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;

	if ((dreg >= DSP_REG_ACM0) && (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)) 
	{
		u16 val = ext_dmem_read(g_dsp.r[sreg]);
		writeToBackLog(0, dreg - DSP_REG_ACM0 + DSP_REG_ACH0, (val & 0x8000) ? 0xFFFF : 0x0000);
		writeToBackLog(1, dreg,	val);
		writeToBackLog(2, dreg - DSP_REG_ACM0 + DSP_REG_ACL0, 0);
		increase_addr_reg(sreg);
	}
	else
	{
		writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[sreg]));
		increase_addr_reg(sreg);
	}
}

// LS $axD.D, $acS.m
// xxxx xxxx 10dd 000s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Increment both $ar0 and $ar3.
void DSPEmitter::ls(const UDSPInstruction opc)
{
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

	ext_dmem_write(DSP_REG_AR3, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR0]));
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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;

	ext_dmem_write(DSP_REG_AR3, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR0]));
	increment_addr_reg(DSP_REG_AR3);
	increase_addr_reg(DSP_REG_AR0);
}

// LSM $axD.D, $acS.m
// xxxx xxxx 10dd 100s
// Load register $axD.D with value from memory pointed by register
// $ar0. Store value from register $acS.m to memory location pointed by
// register $ar3. Add corresponding indexing register $ix3 to addressing
// register $ar3 and increment $ar0.
void DSPEmitter::lsm(const UDSPInstruction opc)
{
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR3, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR0]));
	increase_addr_reg(DSP_REG_AR3);
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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR3, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR0]));
	increase_addr_reg(DSP_REG_AR3);
	increase_addr_reg(DSP_REG_AR0);
}

// SL $acS.m, $axD.D
// xxxx xxxx 10dd 001s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Increment both $ar0 and $ar3.
void DSPEmitter::sl(const UDSPInstruction opc)
{
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR0, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR3]));
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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR0, sreg);
	
	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR3]));
	increment_addr_reg(DSP_REG_AR3);
	increase_addr_reg(DSP_REG_AR0);
}

// SLM $acS.m, $axD.D
// xxxx xxxx 10dd 101s
// Store value from register $acS.m to memory location pointed by register
// $ar0. Load register $axD.D with value from memory pointed by register
// $ar3. Add corresponding indexing register $ix3 to addressing register $ar3
// and increment $ar0.
void DSPEmitter::slm(const UDSPInstruction opc)
{
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR0, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR3]));
	increase_addr_reg(DSP_REG_AR3);
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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
	
	ext_dmem_write(DSP_REG_AR0, sreg);

	writeToBackLog(0, dreg,	ext_dmem_read(g_dsp.r[DSP_REG_AR3]));
	increase_addr_reg(DSP_REG_AR3);
	increase_addr_reg(DSP_REG_AR0);
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

	if (sreg != DSP_REG_AR3) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increment_addr_reg(sreg);
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXH0, ext_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increment_addr_reg(dreg);
	}

	increment_addr_reg(DSP_REG_AR3);
}

// LDN $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 01ss
void DSPEmitter::ldn(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x1;
	u8 rreg = (opc >> 4) & 0x1;
	u8 sreg = opc & 0x3;
	
	if (sreg != DSP_REG_AR3) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increase_addr_reg(sreg);
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXH0, ext_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increase_addr_reg(dreg);
	}
	
	increment_addr_reg(DSP_REG_AR3);
}

// LDM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 10ss
void DSPEmitter::ldm(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x1;
	u8 rreg = (opc >> 4) & 0x1;
	u8 sreg = opc & 0x3;

	if (sreg != DSP_REG_AR3) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increment_addr_reg(sreg);
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXH0, ext_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increment_addr_reg(dreg);
	}

	increase_addr_reg(DSP_REG_AR3);
}

// LDNM $ax0.d, $ax1.r, @$arS
// xxxx xxxx 11dr 11ss
void DSPEmitter::ldnm(const UDSPInstruction opc)
{
	u8 dreg = (opc >> 5) & 0x1;
	u8 rreg = (opc >> 4) & 0x1;
	u8 sreg = opc & 0x3;

	if (sreg != DSP_REG_AR3) {
		writeToBackLog(0, (dreg << 1) + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[sreg]));

		if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[sreg]));
		else
			writeToBackLog(1, (rreg << 1) + DSP_REG_AXL1, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increase_addr_reg(sreg);
	} else {
		writeToBackLog(0, rreg + DSP_REG_AXH0, ext_dmem_read(g_dsp.r[dreg]));

		if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3]))	
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[dreg]));
		else
			writeToBackLog(1, rreg + DSP_REG_AXL0, ext_dmem_read(g_dsp.r[DSP_REG_AR3]));

		increase_addr_reg(dreg);
	}

	increase_addr_reg(DSP_REG_AR3);
}


void DSPEmitter::writeAxAcc(const UDSPInstruction opc) {
	
}


