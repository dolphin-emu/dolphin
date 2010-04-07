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

#ifndef _DSP_JIT_UTIL_H
#define _DSP_JIT_UTIL_H

#include "../DSPMemoryMap.h"
#include "../DSPEmitter.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

// HORRIBLE UGLINESS, someone please fix.
// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
void DSPEmitter::increment_addr_reg(int reg)
{
	ABI_PushAllCalleeSavedRegsAndAdjustStack();

	//	u16 tmb = g_dsp.r[DSP_REG_WR0 + reg];
	MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	//	tmb = tmb | (tmb >> 8);
	MOV(16, R(EBX), R(EAX));
	SHR(16, R(EBX), Imm8(8));
	OR(16, R(EAX), R(EBX));

	//	tmb = tmb | (tmb >> 4);
	MOV(16, R(EBX), R(EAX));
	SHR(16, R(EBX), Imm8(4));
	OR(16, R(EAX), R(EBX));

	//	tmb = tmb | (tmb >> 2);	
	MOV(16, R(EBX), R(EAX));
	SHR(16, R(EBX), Imm8(2));
	OR(16, R(EAX), R(EBX));

	//	tmb = tmb | (tmb >> 1);	
	MOV(16, R(EBX), R(EAX));
	SHR(16, R(EBX), Imm8(1));
	OR(16, R(EAX), R(EBX));
	
	//	s16 tmp = g_dsp.r[reg];
	MOV(16, R(EBX), M(&g_dsp.r[reg]));

	// 	if ((tmp & tmb) == tmb)
	AND(16, R(EAX), R(EBX));
	CMP(16, R(EAX), R(EBX));
	FixupBranch not_equal = J_CC(CC_NZ);

	//	tmp ^= g_dsp.r[DSP_REG_WR0 + reg];
	XOR(16, R(EBX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	FixupBranch end = J();
	SetJumpTarget(not_equal);
	//		tmp++;
	ADD(16, R(EBX), Imm8(1));

	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(EBX));

	ABI_PopAllCalleeSavedRegsAndAdjustStack();
	
}

// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
void DSPEmitter::decrement_addr_reg(int reg)
{
	ABI_PushAllCalleeSavedRegsAndAdjustStack();

	//	s16 tmp = g_dsp.r[reg];
	MOV(16, R(EAX), M(&g_dsp.r[reg]));

	//	if ((tmp & g_dsp.r[DSP_REG_WR0 + reg]) == 0)
	MOV(16, R(EBX), R(EAX));
	AND(16, R(EBX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
	CMP(16, R(EBX), Imm8(0));
	FixupBranch not_equal = J_CC(CC_NZ);

	//	tmp |= g_dsp.r[DSP_REG_WR0 + reg];
	OR(16, R(EAX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	FixupBranch end = J();
	SetJumpTarget(not_equal);
	//		tmp--;
	SUB(16, R(EAX), Imm8(1));

	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(EAX));

	ABI_PopAllCalleeSavedRegsAndAdjustStack();
}

// Increase addr register according to the correspond ix register
void DSPEmitter::increase_addr_reg(int reg)
{
	
	/*
	MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_IX0 + reg]));
	
	CMP(16, R(EAX), Imm16(0));
	FixupBranch end = J_CC(CC_Z);
	FixupBranch negValue = J_CC(CC_L);
		
	ABI_CallFunctionC((void *)increment_addr_reg, reg);   

	FixupBranch posValue = J();

	SetJumpTarget(negValue);
	ABI_CallFunctionC((void *)decrement_addr_reg, reg);   

	SetJumpTarget(posValue);
	SetJumpTarget(end);
	*/

	// TODO: DO RIGHT!
	s16 value = (s16)g_dsp.r[DSP_REG_IX0 + reg];

	if (value > 0) {
		for (int i = 0; i < value; i++) {
			increment_addr_reg(reg);
		}
	} else if (value < 0) {
		for (int i = 0; i < (int)(-value); i++) {
			decrement_addr_reg(reg);
		}
	} 
}

// Decrease addr register according to the correspond ix register
void DSPEmitter::decrease_addr_reg(int reg)
{
	// TODO: DO RIGHT!

	s16 value = (s16)g_dsp.r[DSP_REG_IX0 + reg];
	
	if (value > 0) {
		for (int i = 0; i < value; i++) {
			decrement_addr_reg(reg);
		}
	} else if (value < 0) {
		for (int i = 0; i < (int)(-value); i++) {
			increment_addr_reg(reg);
		}
	} 
}

void DSPEmitter::ext_dmem_write(u32 dest, u32 src)
{

	u16 addr = g_dsp.r[dest];
	u16 val = g_dsp.r[src];
	switch (addr >> 12) {
	case 0x0: // 0xxx DRAM
		g_dsp.dram[addr & DSP_DRAM_MASK] = val;
		break;
		
	case 0x1: // 1xxx COEF
		ERROR_LOG(DSPLLE, "Illegal write to COEF (pc = %02x)", g_dsp.pc);
		break;
		
	case 0xf: // Fxxx HW regs
		// Can ext write to ifx?
		//		gdsp_ifx_write(addr, val);
		ERROR_LOG(DSPLLE, "Illegal write to ifx (pc = %02x)", g_dsp.pc);
		break;
		
	default:  // Unmapped/non-existing memory
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Write to UNKNOWN (%04x) memory", g_dsp.pc, addr);
		break;
	}
}

u16 DSPEmitter::ext_dmem_read(u16 addr)
{
	switch (addr >> 12) {
	case 0x0:  // 0xxx DRAM
		return g_dsp.dram[addr & DSP_DRAM_MASK];
		
	case 0x1:  // 1xxx COEF
		NOTICE_LOG(DSPLLE, "%04x : Coef Read @ %04x", g_dsp.pc, addr);
		return g_dsp.coef[addr & DSP_COEF_MASK];
		
	case 0xf:  // Fxxx HW regs
		// Can ext read from ifx?
		ERROR_LOG(DSPLLE, "Illegal read from ifx (pc = %02x)", g_dsp.pc);
		//		return gdsp_ifx_read(addr);
        
	default:   // Unmapped/non-existing memory
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Read from UNKNOWN (%04x) memory", g_dsp.pc, addr);
		return 0;
	}
}

#endif
