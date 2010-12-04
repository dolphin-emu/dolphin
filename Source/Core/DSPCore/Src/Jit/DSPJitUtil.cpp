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
void DSPEmitter::dsp_increment_one(X64Reg ar, X64Reg wr, X64Reg wr_pow, X64Reg temp_reg)
{
	// 	if ((tmp & tmb) == tmb)
	MOV(16, R(temp_reg), R(ar));
	AND(16, R(temp_reg), R(wr_pow));
	CMP(16, R(temp_reg), R(wr_pow));
	FixupBranch not_equal = J_CC(CC_NE);

	// tmp -= wr_reg
	SUB(16, R(ar), R(wr));

	FixupBranch end = J();
	SetJumpTarget(not_equal);

	//	else tmp++
	ADD(16, R(ar), Imm16(1));
	SetJumpTarget(end);
}

// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
void DSPEmitter::increment_addr_reg(int reg)
{
	//	s16 tmp = g_dsp.r[reg];
#ifdef _M_IX86 // All32
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	MOV(16, R(EAX), MDisp(R11,reg*2));
	MOV(16, R(EDX), MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	dsp_increment_one(EAX, EDX, EDI);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif
}

// See http://code.google.com/p/dolphin-emu/source/detail?r=3125
void DSPEmitter::dsp_decrement_one(X64Reg ar, X64Reg wr, X64Reg wr_pow, X64Reg temp_reg)
{
	// compute min from wr_pow and ar
	// min = (tmb+1-ar)&tmb;
	LEA(16, temp_reg, MDisp(wr_pow, 1));
	SUB(16, R(temp_reg), R(ar));
	AND(16, R(temp_reg), R(wr_pow));

	// wr < min
	CMP(16, R(wr), R(temp_reg));
	FixupBranch wr_lt_min = J_CC(CC_B);
	// !min
	TEST(16, R(temp_reg), R(temp_reg));
	FixupBranch min_zero = J_CC(CC_Z);

	//		ar--;
	SUB(16, R(ar), Imm16(1));
	FixupBranch end = J();

	//      ar += wr;
	SetJumpTarget(wr_lt_min);
	SetJumpTarget(min_zero);
	ADD(16, R(ar), R(wr));

	SetJumpTarget(end);
}

// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
void DSPEmitter::decrement_addr_reg(int reg)
{
	//	s16 ar = g_dsp.r[reg];
#ifdef _M_IX86 // All32
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(EAX), MDisp(R11,reg*2));
	MOV(16, R(EDX), MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	dsp_decrement_one(EAX, EDX, EDI);
	
	//	g_dsp.r[reg] = tmp;	
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif
}

// Increase addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::increase_addr_reg(int reg)
{	
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp.r[DSP_REG_IX0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(32, 16, ECX, MDisp(R11,(DSP_REG_IX0 + reg)*2));
#endif
	// IX0 == 0, bail out
	
	TEST(16, R(ECX), R(ECX));
	// code too long for a 5-byte jump
	// TODO: optimize a bit, maybe merge loops?
	FixupBranch end = J_CC(CC_Z, true);

#ifdef _M_IX86 // All32
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(16, R(EAX), MDisp(R11,reg*2));
	MOV(16, R(EDX), MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	// IX0 > 0
	// TODO: ToMask flushes flags set by TEST,
	// needs another CMP here.
	CMP(16, R(ECX), Imm16(0));
	FixupBranch neg = J_CC(CC_L);

	JumpTarget loop_pos = GetCodePtr();

	// dsp_increment
	dsp_increment_one(EAX, EDX, EDI);

	SUB(16, R(ECX), Imm16(1)); // value--
#ifdef _M_IX86 // All32
	CMP(16, M(&g_dsp.r[DSP_REG_IX0 + reg]), Imm16(127));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	CMP(16, MDisp(R11,(DSP_REG_IX0 + reg)*2), Imm16(127));
#endif
	FixupBranch dbg = J_CC(CC_NE);
	CMP(16, R(ECX), Imm16(1));
	FixupBranch dbg2 = J_CC(CC_NE);
	INT3();
	SetJumpTarget(dbg2);
	SetJumpTarget(dbg);
	CMP(16, R(ECX), Imm16(0)); // value > 0
	J_CC(CC_G, loop_pos); 
	FixupBranch end_pos = J();

	// else, IX0 < 0
	SetJumpTarget(neg);
	JumpTarget loop_neg = GetCodePtr();

	// dsp_decrement
	dsp_decrement_one(EAX, EDX, EDI);

	ADD(16, R(ECX), Imm16(1)); // value++
	CMP(16, R(ECX), Imm16(0)); // value < 0
	J_CC(CC_L, loop_neg);

	SetJumpTarget(end_pos);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif

	SetJumpTarget(end);
}

// Decrease addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::decrease_addr_reg(int reg)
{
#ifdef _M_IX86 // All32
	MOV(16, R(ECX), M(&g_dsp.r[DSP_REG_IX0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(ECX), MDisp(R11,(DSP_REG_IX0 + reg)*2));
#endif
	// IX0 == 0, bail out
	TEST(16, R(ECX), R(ECX));
	// code too long for a 5-byte jump
	// TODO: optimize a bit, maybe merge loops?
	FixupBranch end = J_CC(CC_Z, true);

#ifdef _M_IX86 // All32
	MOV(16, R(EAX), M(&g_dsp.r[reg]));
	MOV(16, R(EDX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(16, R(EAX), MDisp(R11,reg*2));
	MOV(16, R(EDX), MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	MOV(16, R(EDI), R(EDX));
	ToMask(EDI);

	// IX0 > 0
	// TODO: ToMask flushes flags set by TEST,
	// needs another CMP here.
	CMP(16, R(ECX), Imm16(0));
	FixupBranch neg = J_CC(CC_L);

	JumpTarget loop_pos = GetCodePtr();

	// dsp_decrement
	dsp_decrement_one(EAX, EDX, EDI);

	SUB(16, R(ECX), Imm16(1)); // value--
	CMP(16, R(ECX), Imm16(0)); // value > 0
	J_CC(CC_G, loop_pos); 
	FixupBranch end_pos = J();

	// else, IX0 < 0
	SetJumpTarget(neg);
	JumpTarget loop_neg = GetCodePtr();

	// dsp_increment
	dsp_increment_one(EAX, EDX, EDI);

	ADD(16, R(ECX), Imm16(1)); // value++
	CMP(16, R(ECX), Imm16(0)); // value < 0
	J_CC(CC_L, loop_neg);

	SetJumpTarget(end_pos);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif

	SetJumpTarget(end);
}
// EAX - destination address (g_dsp.r[dest])
// ECX - value (g_dsp.r[src])
// ESI - the upper bits of the address (>> 12)
void DSPEmitter::ext_dmem_write(u32 dest, u32 src)
{
	//	u16 addr = g_dsp.r[dest];
#ifdef _M_IX86 // All32
	MOVZX(32, 16, EAX, M(&g_dsp.r[dest]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(32, 16, EAX, MDisp(R11,dest*2));
#endif

	//	u16 val = g_dsp.r[src];
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp.r[src]));
#else
	MOVZX(32, 16, ECX, MDisp(R11,src*2));
#endif

	//	u16 saddr = addr >> 12; 
	MOV(32, R(ESI), R(EAX));
	SHR(16, R(ESI), Imm8(12));

	//	if (saddr == 0)
	TEST(16, R(ESI), R(ESI));
	FixupBranch ifx = J_CC(CC_NZ);

	//  g_dsp.dram[addr & DSP_DRAM_MASK] = val;
	AND(16, R(EAX), Imm16(DSP_DRAM_MASK));
#ifdef _M_X64
	MOV(64, R(ESI), ImmPtr(g_dsp.dram));
#else
	MOV(32, R(ESI), ImmPtr(g_dsp.dram));
#endif
	MOV(16, MComplex(ESI, EAX, 2, 0), R(ECX));

	FixupBranch end = J();
	//	else if (saddr == 0xf)
	SetJumpTarget(ifx);
	// Does it mean gdsp_ifx_write needs u32 rather than u16?
	ABI_CallFunctionRR((void *)gdsp_ifx_write, EAX, ECX);	
	SetJumpTarget(end);
}

// EAX - the result of the read (used by caller)
// ECX - the address to read
// ESI - the upper bits of the address (>> 12)
void DSPEmitter::ext_dmem_read(u16 addr)
{
	//	u16 addr = g_dsp.r[addr];
#ifdef _M_IX86 // All32
	MOVZX(32, 16, ECX, M(&g_dsp.r[addr]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVZX(32, 16, ECX, MDisp(R11,addr*2));
#endif

	//	u16 saddr = addr >> 12;
	MOV(32, R(ESI), R(ECX));
	SHR(16, R(ESI), Imm8(12));

	//	if (saddr == 0)
	TEST(16, R(ESI), R(ESI));
	FixupBranch dram = J_CC(CC_NZ);
	//	return g_dsp.dram[addr & DSP_DRAM_MASK];
	AND(16, R(ECX), Imm16(DSP_DRAM_MASK));
#ifdef _M_X64
	MOV(64, R(ESI), ImmPtr(g_dsp.dram));
#else
	MOV(32, R(ESI), ImmPtr(g_dsp.dram));
#endif
	MOV(16, R(EAX), MComplex(ESI, ECX, 2, 0));

	FixupBranch end = J();
	SetJumpTarget(dram);
	//	else if (saddr == 0x1)
	CMP(16, R(ESI), Imm16(0x1));
	FixupBranch ifx = J_CC(CC_NZ);
	//		return g_dsp.coef[addr & DSP_COEF_MASK];
	AND(16, R(ECX), Imm16(DSP_COEF_MASK));
#ifdef _M_X64
	MOV(64, R(ESI), ImmPtr(g_dsp.coef));
#else
	MOV(32, R(ESI), ImmPtr(g_dsp.coef));
#endif
	MOV(16, R(EAX), MComplex(ESI,ECX,2,0));

	FixupBranch end2 = J();
	SetJumpTarget(ifx);
	//	else if (saddr == 0xf)
	//		return gdsp_ifx_read(addr);
	ABI_CallFunctionR((void *)gdsp_ifx_read, ECX);
	
	SetJumpTarget(end);
	SetJumpTarget(end2);
}

// Returns s64 in RAX
// Clobbers RSI, RDI
void DSPEmitter::get_long_prod()
{
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	//s64 val   = (s8)(u8)g_dsp.r[DSP_REG_PRODH];	
	MOVSX(64, 8, RAX, MDisp(R11,DSP_REG_PRODH*2));
	//val <<= 32;
	SHL(64, R(RAX), Imm8(32));
	//s64 low_prod  = g_dsp.r[DSP_REG_PRODM];
	MOVSX(64, 16, RSI, MDisp(R11,DSP_REG_PRODM*2));
	//low_prod += g_dsp.r[DSP_REG_PRODM2];
	MOVSX(64, 16, EDI, MDisp(R11,DSP_REG_PRODM2*2));
	ADD(16, R(RSI), R(EDI));
	//low_prod <<= 16;
	SHL(64, R(RSI), Imm8(16));
	OR(64, R(RAX), R(RSI));
	//low_prod |= g_dsp.r[DSP_REG_PRODL];
	MOV(16, R(RAX), MDisp(R11,DSP_REG_PRODL*2));
	//return val;
#endif
}

// Returns s64 in RAX
// Clobbers RSI
void DSPEmitter::get_long_prod_round_prodl()
{
#ifdef _M_X64
	//s64 prod = dsp_get_long_prod();
	get_long_prod();

	//if (prod & 0x10000) prod = (prod + 0x8000) & ~0xffff;
	TEST(32, R(EAX), Imm32(0x10000));
	FixupBranch jump = J_CC(CC_Z);
	ADD(64, R(RAX), Imm32(0x8000));
	MOV(64, R(ESI), Imm64(~0xffff));
	AND(64, R(RAX), R(RSI));
	FixupBranch ret = J();
	//else prod = (prod + 0x7fff) & ~0xffff;
	SetJumpTarget(jump);
	ADD(64, R(RAX), Imm32(0x7fff));
	MOV(64, R(RSI), Imm64(~0xffff));
	AND(64, R(RAX), R(RSI));
	SetJumpTarget(ret);
	//return prod;
#endif
}

// For accurate emulation, this is wrong - but the real prod registers behave
// in completely bizarre ways. Probably not meaningful to emulate them accurately.
// In: RAX = s64 val
void DSPEmitter::set_long_prod()
{
#ifdef _M_X64
	//	g_dsp.r[DSP_REG_PRODL] = (u16)val;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11, DSP_REG_PRODL * 2), R(AX));
	//	val >>= 16;
	SHR(64, R(RAX), Imm8(16));
	//	g_dsp.r[DSP_REG_PRODM] = (u16)val;
	MOV(16, MDisp(R11, DSP_REG_PRODM * 2), R(AX));
	//	val >>= 16;
	SHR(64, R(RAX), Imm8(16));
	//	g_dsp.r[DSP_REG_PRODH] = (u8)val;
	MOVZX(64, 8, RAX, R(AL));
	MOV(8, MDisp(R11, DSP_REG_PRODH * 2), R(AL));
	//	g_dsp.r[DSP_REG_PRODM2] = 0;
	MOV(16, MDisp(R11, DSP_REG_PRODM2 * 2), Imm16(0));
#endif
}

// Returns s64 in RAX
// Clobbers ESI
void DSPEmitter::get_long_acc(int _reg)
{
#ifdef _M_X64
//	s64 high = (s64)(s8)g_dsp.r[DSP_REG_ACH0 + reg] << 32;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 8, EAX, MDisp(R11, (DSP_REG_ACH0 + _reg) * 2));
	SHL(64, R(EAX), Imm8(32));
//	u32 mid_low = ((u32)g_dsp.r[DSP_REG_ACM0 + reg] << 16) | g_dsp.r[DSP_REG_ACL0 + reg];
	MOVZX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2));
	SHL(32, R(RSI), Imm8(16));
	OR(64, R(EAX), R(RSI));
	MOVZX(64, 16, RSI, MDisp(R11, (DSP_REG_ACL0 + _reg) * 2));
	OR(64, R(EAX), R(RSI));
//	return high | mid_low;
#endif
}

// In: RAX = s64 val
void DSPEmitter::set_long_acc(int _reg)
{
#ifdef _M_X64
//	g_dsp.r[DSP_REG_ACL0 + _reg] = (u16)val;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11, (DSP_REG_ACL0 + _reg) * 2), R(AX));
//	val >>= 16;
	SHR(64, R(RAX), Imm8(16));
//	g_dsp.r[DSP_REG_ACM0 + _reg] = (u16)val;
	MOV(16, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2), R(AX));
//	val >>= 16;
	SHR(64, R(RAX), Imm8(16));
//	g_dsp.r[DSP_REG_ACH0 + _reg] = (u16)(s16)(s8)(u8)val;
	MOVSX(16, 8, AX, R(AX));
	MOV(16, MDisp(R11, (DSP_REG_ACH0 + _reg) * 2), R(AX));
#endif
}

// Returns s16 in AX
void DSPEmitter::get_acc_m(int _reg)
{
//	return g_dsp.r[DSP_REG_ACM0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RAX, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2));
#endif
}

// Returns s16 in EAX
void DSPEmitter::get_ax_l(int _reg)
{
//	return (s16)g_dsp.r[DSP_REG_AXL0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RAX, MDisp(R11, (DSP_REG_AXL0 + _reg) * 2));
#endif
}

// Returns s16 in EAX
void DSPEmitter::get_ax_h(int _reg)
{
//	return (s16)g_dsp.r[DSP_REG_AXH0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RAX, MDisp(R11, (DSP_REG_AXH0 + _reg) * 2));
#endif
}

#endif
