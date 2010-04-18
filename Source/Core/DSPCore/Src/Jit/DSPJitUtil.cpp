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
#include "../DSPHWInterface.h"
#include "../DSPEmitter.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

// Performs the hashing required by increment/increase/decrease_addr_reg
void DSPEmitter::ToMask(X64Reg value_reg, X64Reg temp_reg)
{
	MOV(16, R(temp_reg), R(value_reg));
	SHR(16, R(temp_reg), Imm8(8));
	OR(16, R(value_reg), R(temp_reg));
	MOV(16, R(temp_reg), R(value_reg));
	SHR(16, R(temp_reg), Imm8(4));
	OR(16, R(value_reg), R(temp_reg));
	MOV(16, R(temp_reg), R(value_reg));
	SHR(16, R(temp_reg), Imm8(2));
	OR(16, R(value_reg), R(temp_reg));
	MOV(16, R(temp_reg), R(value_reg));
	SHR(16, R(temp_reg), Imm8(1));
	OR(16, R(value_reg), R(temp_reg));
}

// HORRIBLE UGLINESS, someone please fix.
// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
void DSPEmitter::increment_addr_reg(int reg)
{
	//	s16 tmp = g_dsp.r[reg];
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	//ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	// 	if ((tmp & tmb) == tmb)
	MOV(16, R(ESI), R(EAX));
	AND(16, R(ESI), R(EDI));
	CMP(16, R(ESI), R(EDI));
	FixupBranch not_equal = J_CC(CC_NE);

	// tmp ^= wr_reg
	XOR(16, R(EAX), R(EDX));

	FixupBranch end = J()
	SetJumpTarget(not_equal);

	//	else tmp++
	ADD(16, R(EAX), Imm16(1));
	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
}


// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
void DSPEmitter::decrement_addr_reg(int reg)
{
	//	s16 tmp = g_dsp.r[reg];
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));

	//	if ((tmp & g_dsp.r[DSP_REG_WR0 + reg]) == 0)
	TEST(16, R(EAX), R(EDX));
	FixupBranch not_equal = J_CC(CC_NZ);

	//	tmp |= g_dsp.r[DSP_REG_WR0 + reg];
	OR(16, R(EAX), R(EDX));

	FixupBranch end = J();
	SetJumpTarget(not_equal);
	//		tmp--;
	SUB(16, R(EAX), Imm16(1));

	SetJumpTarget(end);
	
	//	g_dsp.r[reg] = tmp;	
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
}

// Increase addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::increase_addr_reg(int reg)
{	
	MOV(16, R(ECX), M(&g_dsp.r[DSP_REG_IX0 + reg]));
	//IX0 == 0, bail out
	TEST(16, R(ECX), R(ECX));
	FixupBranch end = J_CC(CC_Z);

	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
	//IX0 > 0
	FixupBranch neg = J_CC(CC_L);

	//ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	//dsp_increment
	JumpTarget loop_pos = GetCodePtr();
	// if ((tmp & tmb) == tmb)
	MOV(16, R(ESI), R(EAX));
	AND(16, R(ESI), R(EDI));
	CMP(16, R(ESI), R(EDI));
	FixupBranch pos_neq = J_CC(CC_NE);

	// tmp ^= wr_reg
	XOR(16, R(EAX), R(EDX));
	FixupBranch pos_eq = J();
	SetJumpTarget(pos_neq);
	//	else tmp++
	ADD(16, R(EAX), Imm16(1));
	SetJumpTarget(pos_eq);

	SUB(16, R(ECX), Imm16(1)); // value--
	CMP(16, R(ECX), Imm16(0)); // value > 0
	J_CC(CC_G, loop_pos); 
	FixupBranch end_pos = J();

	//else, IX0 < 0
	SetJumpTarget(neg);
	JumpTarget loop_neg = GetCodePtr();
	//dsp_decrement
	//			if ((tmp & wr_reg) == 0)
	TEST(16, R(EAX), R(EDX));

	FixupBranch neg_nz = J_CC(CC_NZ);
	//	tmp |= wr_reg;
	OR(16, R(EAX), R(EDX));
	FixupBranch neg_z = J();
	SetJumpTarget(neg_nz);
	// else tmp--
	SUB(16, R(EAX), Imm16(1));
	SetJumpTarget(neg_z);

	ADD(16, R(ECX), Imm16(1)); // value++
	CMP(16, R(ECX), Imm16(0)); // value < 0
	J_CC(CC_L, loop_neg);

	SetJumpTarget(end_pos);

	//	g_dsp.r[reg] = tmp;
	MOV(16, M(&g_dsp.r[reg]), R(EAX));

	SetJumpTarget(end);
}

// Decrease addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::decrease_addr_reg(int reg)
{
	MOV(16, R(ECX), M(&g_dsp.r[DSP_REG_IX0 + reg]));
	//IX0 == 0, bail out
	TEST(16, R(ECX), R(ECX));
	FixupBranch end = J_CC(CC_Z);

	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
	//IX0 > 0
	FixupBranch neg = J_CC(CC_L);
	JumpTarget loop_pos = GetCodePtr();

	//dsp_decrement
	//			if ((tmp & wr_reg) == 0)
	TEST(16, R(EAX), R(EDX));

	FixupBranch neg_nz = J_CC(CC_NZ);
	//	tmp |= wr_reg;
	OR(16, R(EAX), R(EDX));
	FixupBranch neg_z = J();
	SetJumpTarget(neg_nz);
	// else tmp--
	SUB(16, R(EAX), Imm16(1));
	SetJumpTarget(neg_z);

	SUB(16, R(ECX), Imm16(1)); // value--
	CMP(16, R(ECX), Imm16(0)); // value > 0
	J_CC(CC_G, loop_pos); 
	FixupBranch end_pos = J();

	//else, IX0 < 0
	SetJumpTarget(neg);

	//ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	JumpTarget loop_neg = GetCodePtr();
	//dsp_increment
	// if ((tmp & tmb) == tmb)
	MOV(16, R(ESI), R(EAX));
	AND(16, R(ESI), R(EDI));
	CMP(16, R(ESI), R(EDI));
	FixupBranch pos_neq = J_CC(CC_NE);

	// tmp ^= wr_reg
	XOR(16, R(EAX), R(EDX));
	FixupBranch pos_eq = J();

	SetJumpTarget(pos_neq);
	//	else tmp++
	ADD(16, R(EAX), Imm16(1));
	SetJumpTarget(pos_eq);

	ADD(16, R(ECX), Imm16(1)); // value++
	CMP(16, R(ECX), Imm16(0)); // value < 0
	J_CC(CC_L, loop_neg);

	SetJumpTarget(end_pos);

	//	g_dsp.r[reg] = tmp;
	MOV(16, M(&g_dsp.r[reg]), R(EAX));

	SetJumpTarget(end);
}

void DSPEmitter::ext_dmem_write(u32 dest, u32 src)
{
	//	u16 addr = g_dsp.r[dest];
	MOVZX(32, 16, EAX, M(&g_dsp.r[dest]));

	//	u16 val = g_dsp.r[src];
	MOVZX(32, 16, ECX, M(&g_dsp.r[src]));

	//	u16 saddr = addr >> 12; 
	MOVZX(32, 16, ESI, R(EAX));
	SHR(16, R(ESI), Imm16(12));

	//	if (saddr == 0)
	CMP(16, R(ESI), Imm16(0));
	FixupBranch ifx = J_CC(CC_NZ);

	//  g_dsp.dram[addr & DSP_DRAM_MASK] = val;
	AND(16, R(EAX), Imm16(DSP_DRAM_MASK));
#ifdef _M_X64
	MOV(64, R(R11), Imm64((u64)g_dsp.dram)); 
	ADD(64, R(EAX), R(R11));
#else
	ADD(32, R(EAX), Imm32((u32)g_dsp.dram));
#endif
	MOV(16, MDisp(EAX,0), R(ECX));
	
	FixupBranch end = J();
	//	else if (saddr == 0xf)
	SetJumpTarget(ifx);
	// Does it mean gdsp_ifx_write needs u32 rather than u16?
	ABI_CallFunctionRR((void *)gdsp_ifx_write, EAX, ECX);	
	SetJumpTarget(end);
}

// EAX should have the return value
void DSPEmitter::ext_dmem_read(u16 addr)
{
	MOVZX(32, 16, ECX, M(&addr));

	//	u16 saddr = addr >> 12; 
	MOVZX(32, 16, ESI, R(EAX));
	SHR(16, R(ESI), Imm16(12));

	//	if (saddr == 0)
	CMP(16, R(ESI), Imm16(0));
	FixupBranch dram = J_CC(CC_NZ);
	//	return g_dsp.dram[addr & DSP_DRAM_MASK];
	AND(16, R(ECX), Imm16(DSP_DRAM_MASK));
#ifdef _M_X64
	MOV(64, R(R11), Imm64((u64)g_dsp.dram)); 
	ADD(64, R(ECX), R(R11));
#else
	ADD(32, R(ECX), Imm32((u32)g_dsp.dram));
#endif
	MOV(16, R(EAX), MDisp(ECX,0));

	FixupBranch end = J();
	SetJumpTarget(dram);
	//	else if (saddr == 0x1)
	CMP(16, R(ESI), Imm16(0x1));
	FixupBranch ifx = J_CC(CC_NZ);
	//		return g_dsp.coef[addr & DSP_COEF_MASK];
	AND(16, R(ECX), Imm16(DSP_COEF_MASK));
#ifdef _M_X64
	MOV(64, R(R11), Imm64((u64)g_dsp.dram)); 
	ADD(64, R(ECX), R(R11));
#else
	ADD(32, R(ECX), Imm32((u32)g_dsp.dram));
#endif
	MOV(16, R(EAX), MDisp(ECX,0));

	FixupBranch end2 = J();
	SetJumpTarget(ifx);
	//	else if (saddr == 0xf)
	//		return gdsp_ifx_read(addr);
	ABI_CallFunctionR((void *)gdsp_ifx_read, ECX);
	
	SetJumpTarget(end);
	SetJumpTarget(end2);
}

#endif
