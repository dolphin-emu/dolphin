// Copyright (C) 2010 Dolphin Project.

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
#include "x64Emitter.h"
#include "ABI.h"

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
	pushExtValueFromReg(dreg + DSP_REG_AXL0, sreg);
	//	MOV(16, M(&g_dsp.r[dreg + DSP_REG_AXL0]), M(&g_dsp.r[sreg]));
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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[dreg]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,dreg*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	//	u16 val = g_dsp.r[src];
	dmem_write();
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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[dreg]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,dreg*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();
	increase_addr_reg(dreg);
}

// L $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS. 
// Post increment register $arS.
void DSPEmitter::l(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x3;
	u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0; //AX?.?, AC?.[LM]

	pushExtValueFromMem(dreg, sreg);

	if (dreg  >= DSP_REG_ACM0) {
		//save SR too, so we can decide later.
		//even if only for one bit, can only
		//store (up to) two registers in EBX,
		//so store all of SR
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_SR]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(EAX), MDisp(R11,DSP_REG_SR*2));
#endif
		SHL(32, R(EAX), Imm8(16));
		OR(32, R(EBX), R(EAX));
	}

	increment_addr_reg(sreg);
}

// LN $axD.D, @$arS
// xxxx xxxx 01dd d0ss
// Load $axD.D/$acD.D with value from memory pointed by register $arS. 
// Add indexing register register $ixS to register $arS.
void DSPEmitter::ln(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x3;
	u8 dreg = ((opc >> 3) & 0x7) + DSP_REG_AXL0;

	pushExtValueFromMem(dreg, sreg);

	if (dreg  >= DSP_REG_ACM0) {
		//save SR too, so we can decide later.
		//even if only for one bit, can only
		//store (up to) two registers in EBX,
		//so store all of SR
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_SR]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(EAX), MDisp(R11,DSP_REG_SR*2));
#endif
		SHL(32, R(EAX), Imm8(16));
		OR(32, R(EBX), R(EAX));
	}

	increase_addr_reg(sreg);
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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR3]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR3*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR3]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR3*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR0);
	
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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR3]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR3*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR0);

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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR3]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR3*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR0);

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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR0]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR0*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

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
	u8 sreg = (opc & 0x1) + DSP_REG_ACM0;
	u8 dreg = ((opc >> 4) & 0x3) + DSP_REG_AXL0;
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR0]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR0*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR3);

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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR0]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR0*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR3);

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
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[DSP_REG_AR0]));
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, EAX, MDisp(R11,DSP_REG_AR0*2));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_write();

	pushExtValueFromMem(dreg, DSP_REG_AR3);

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
		pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

		// 	if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[sreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,sreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
		FixupBranch after = J();
		SetJumpTarget(not_equal); // else
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
		SetJumpTarget(after);

		increment_addr_reg(sreg);

	} else {
		pushExtValueFromMem(rreg + DSP_REG_AXH0, dreg);

		//if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[dreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,dreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, dreg);
		FixupBranch after = J(); // else
		SetJumpTarget(not_equal);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
		SetJumpTarget(after);

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
		pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

		// 	if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[sreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,sreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
		FixupBranch after = J();
		SetJumpTarget(not_equal); // else
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
		SetJumpTarget(after);

		increase_addr_reg(sreg);
	} else {
		pushExtValueFromMem(rreg + DSP_REG_AXH0, dreg);

		//if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[dreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,dreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, dreg);
		FixupBranch after = J(); // else
		SetJumpTarget(not_equal);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
		SetJumpTarget(after);

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
		pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

		// 	if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[sreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,sreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
		FixupBranch after = J();
		SetJumpTarget(not_equal); // else
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
		SetJumpTarget(after);

		increment_addr_reg(sreg);
	} else {
		pushExtValueFromMem(rreg + DSP_REG_AXH0, dreg);

		//if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[dreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,dreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, dreg);
		FixupBranch after = J(); // else
		SetJumpTarget(not_equal);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
		SetJumpTarget(after);

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
		pushExtValueFromMem((dreg << 1) + DSP_REG_AXL0, sreg);

		// 	if (IsSameMemArea(g_dsp.r[sreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[sreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,sreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, sreg);
		FixupBranch after = J();
		SetJumpTarget(not_equal); // else
		pushExtValueFromMem2((rreg << 1) + DSP_REG_AXL1, DSP_REG_AR3);
		SetJumpTarget(after);

		increase_addr_reg(sreg);
	} else {
		pushExtValueFromMem(rreg + DSP_REG_AXH0, dreg);

		//if (IsSameMemArea(g_dsp.r[dreg], g_dsp.r[DSP_REG_AR3])) {
#ifdef _M_IX86 // All32
		MOV(16, R(ESI), M(&g_dsp.r[dreg]));
		MOV(16, R(EDI), M(&g_dsp.r[DSP_REG_AR3]));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, R(ESI), MDisp(R11,dreg*2));
		MOV(16, R(EDI), MDisp(R11,DSP_REG_AR3*2));
#endif
		SHR(16, R(ESI), Imm8(10));
		SHR(16, R(EDI), Imm8(10));
		CMP(16, R(ESI), R(EDI));
		FixupBranch not_equal = J_CC(CC_NE);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, dreg);
		FixupBranch after = J(); // else
		SetJumpTarget(not_equal);
		pushExtValueFromMem2(rreg + DSP_REG_AXL0, DSP_REG_AR3);
		SetJumpTarget(after);

		increase_addr_reg(dreg);
	}

	increase_addr_reg(DSP_REG_AR3);
}


// Push value from g_dsp.r[sreg] into EBX and stores the destinationindex in
// storeIndex
void DSPEmitter::pushExtValueFromReg(u16 dreg, u16 sreg) {
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EBX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(RBX), ImmPtr(&g_dsp.r));
	MOVZX(32, 16, EBX, MDisp(RBX,sreg*2));
#endif
	storeIndex = dreg;
}

void DSPEmitter::pushExtValueFromMem(u16 dreg, u16 sreg) {
	//	u16 addr = g_dsp.r[addr];
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_read();
	MOVZX(32, 16, EBX, R(EAX));

	storeIndex = dreg;
}

void DSPEmitter::pushExtValueFromMem2(u16 dreg, u16 sreg) {
	//	u16 addr = g_dsp.r[addr];
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp.r[sreg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(64, 16, ECX, MDisp(R11,sreg*2));
#endif
	dmem_read();
	SHL(32,R(EAX),Imm8(16));
	OR(32, R(EBX), R(EAX));

	storeIndex2 = dreg;
}

void DSPEmitter::popExtValueToReg() {
	// in practise, we rarely ever have a non-NX main op
	// with an extended op, so the OR here is either
	// not run (storeIndex == -1) or ends up OR'ing
	// EBX with 0 (becoming the MOV we have here)
	// nakee wants to keep it clean, so lets do that.
	// [nakeee] the or case never happens in real
	// [nakeee] it's just how the hardware works so we added it
	if (storeIndex != -1) {
#ifdef _M_IX86 // All32
		MOV(16, M(&g_dsp.r[storeIndex]), R(EBX));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, MDisp(R11,storeIndex*2), R(EBX));
#endif
		if (storeIndex  >= DSP_REG_ACM0 && storeIndex2 == -1) {
			TEST(32, R(EBX), Imm32(SR_40_MODE_BIT << 16));
			FixupBranch not_40bit = J_CC(CC_Z);
			//if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
			//{
			// Sign extend into whole accum.
			//u16 val = g_dsp.r[reg];
			MOVSX(32, 16, EAX, R(EBX));
			SHR(32,R(EAX),Imm8(16));
			//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
			//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
#ifdef _M_IX86 // All32
			MOV(16,M(&g_dsp.r[storeIndex - DSP_REG_ACM0 + DSP_REG_ACH0]),
			    R(EAX));
			MOV(16,M(&g_dsp.r[storeIndex - DSP_REG_ACM0 + DSP_REG_ACL0]),
			    Imm16(0));
#else
			MOV(16, MDisp(R11,(storeIndex - DSP_REG_ACM0 + DSP_REG_ACH0)*2), R(EAX));
			MOV(16, MDisp(R11,(storeIndex - DSP_REG_ACM0 + DSP_REG_ACL0)*2), Imm16(0));
#endif
			//}
			SetJumpTarget(not_40bit);
		}
	}

	storeIndex = -1;

	if (storeIndex2 != -1) {
		SHR(32,R(EBX),Imm8(16));
#ifdef _M_IX86 // All32
		MOV(16, M(&g_dsp.r[storeIndex2]), R(EBX));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp.r));
		MOV(16, MDisp(R11,storeIndex2*2), R(EBX));
#endif
	}
	storeIndex2 = -1;
}

// This function is being called in the main op after all input regs were read
// and before it writes into any regs. This way we can always use bitwise or to
// apply the ext command output, because if the main op didn't change the value
// then 0 | ext output = ext output and if it did then bitwise or is still the
// right thing to do
//this is only needed as long as we do fallback for ext ops
void DSPEmitter::zeroWriteBackLog(const UDSPInstruction opc)
{
	const DSPOPCTemplate *tinst = GetOpTemplate(opc);

	// Call extended
	if (!tinst->extended)
	    return;

	if ((opc >> 12) == 0x3) {
		if (! extOpTable[opc & 0x7F]->jitFunc)
			ABI_CallFunction((void*)::zeroWriteBackLog);
	} else {
		if (! extOpTable[opc & 0xFF]->jitFunc)
			ABI_CallFunction((void*)::zeroWriteBackLog);
	}
	return;
}
