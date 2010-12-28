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
// clobbers RCX
void DSPEmitter::ToMask(X64Reg value_reg)
{
#if 0
	MOV(16, R(CX), R(value_reg));
	SHR(16, R(CX), Imm8(8));
	OR(16, R(value_reg), R(CX));
	MOV(16, R(CX), R(value_reg));
	SHR(16, R(CX), Imm8(4));
	OR(16, R(value_reg), R(CX));
	MOV(16, R(CX), R(value_reg));
	SHR(16, R(CX), Imm8(2));
	OR(16, R(value_reg), R(CX));
	MOV(16, R(CX), R(value_reg));
	SHR(16, R(CX), Imm8(1));
	OR(16, R(value_reg), R(CX));
	MOVZX(32,16,value_reg, R(value_reg));
#else
	BSR(16, CX, R(value_reg));
	FixupBranch undef = J_CC(CC_Z); //CX is written, but undefined

	MOV(32, R(value_reg), Imm32(2));
	SHL(32, R(value_reg), R(CL));
	SUB(32, R(value_reg), Imm32(1));
	//don't waste an instruction on jumping over an effective noop

	SetJumpTarget(undef);
#endif
	OR(16, R(value_reg), Imm16(1));
	XOR(64, R(RCX), R(RCX));
}

// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
//clobbers RCX
void DSPEmitter::increment_addr_reg(int reg)
{
	/*
	u16 ar = g_dsp.r[reg];
	u16 wr = g_dsp.r[reg+8];
	u16 nar = ar+1;
	//this works, because nar^ar will have all the bits from the highest
	//changed bit downwards set(true only for +1!)
	//based on an idea by Mylek
	if((nar^ar)>=((wr<<1)|1))
		nar -= wr+1;
	*/

	//	s16 tmp = g_dsp.r[reg];
#ifdef _M_IX86 // All32
	MOV(16, R(AX), M(&g_dsp.r[reg]));
	MOV(16, R(DX), M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(AX), MDisp(R11,reg*2));
	MOV(16, R(DX), MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	MOV(16,R(DI), R(AX));
	ADD(16,R(AX), Imm16(1));
	XOR(16,R(DI), R(AX));
	MOV(16,R(SI), R(DX));

	SHL(16,R(SI), Imm8(1));
	OR(16,R(SI), Imm16(3));
	CMP(16,R(DI), R(SI));
	FixupBranch nowrap = J_CC(CC_L);

	SUB(16,R(AX), R(DX));
	SUB(16,R(AX), Imm16(1));

	SetJumpTarget(nowrap);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(AX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(AX));
#endif
}

// EAX = g_dsp.r[reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
//clobbers RCX
void DSPEmitter::decrement_addr_reg(int reg)
{
	/*
	u16 ar = g_dsp.r[reg];
	u16 wr = g_dsp.r[reg+8];
	u16 m = ToMask(wr) | 1;
	u16 nar = ar-1;
	if((ar&m) - 1 < m-wr)
		nar += wr+1;
	return nar;
	 */

	//	s16 ar = g_dsp.r[reg];
#ifdef _M_IX86 // All32
	MOV(16, R(AX), M(&g_dsp.r[reg]));
	MOVZX(32, 16, EDX, M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(AX), MDisp(R11,reg*2));
	MOVZX(32, 16, EDX, MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	//u16 m = ToMask(wr) | 1;
	MOV(16, R(DI), R(DX));
	ToMask(DI);

	//u16 nar = ar-1;
	MOV(16, R(CX), R(AX));
	SUB(16, R(AX), Imm16(1));

	//(ar&m) - 1
	AND(32, R(ECX), R(EDI));
	SUB(32, R(ECX), Imm32(1));

	//m-wr
	SUB(32, R(EDI), R(EDX));
	CMP(32, R(ECX), R(EDI));
	FixupBranch out1 = J_CC(CC_GE);
	ADD(16,R(AX),R(DX));
	ADD(16,R(AX),Imm16(1));

	SetJumpTarget(out1);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(AX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(AX));
#endif
}

// Increase addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::increase_addr_reg(int reg)
{
	/*
	u16 ar = g_dsp.r[reg];
	u16 wr = g_dsp.r[reg+8];
	u16 ix = g_dsp.r[reg+4];
	u16 m = ToMask(wr) | 1;
	u16 nar = ar+ix;
	if (ix >= 0) {
		if((ar&m) + (ix&m) -(int)m-1 >= 0)
			nar -= wr+1;
	} else {
		if((ar&m) + (ix&m) -(int)m-1 < m-wr)
			nar += wr+1;
	}
	return nar;
	 */

#ifdef _M_IX86 // All32
	MOV(16, R(SI), M(&g_dsp.r[DSP_REG_IX0 + reg]));
	MOV(16, R(AX), M(&g_dsp.r[reg]));
	MOVZX(32, 16, EDX, M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(SI), MDisp(R11,(DSP_REG_IX0 + reg)*2));
	MOV(16, R(AX), MDisp(R11,reg*2));
	MOVZX(32, 16, EDX, MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	//u16 m = ToMask(wr) | 1;
	MOV(16, R(DI), R(DX));
	ToMask(DI);

	//u16 nar = ar+ix;
	MOV(16, R(CX), R(AX));
	ADD(16, R(AX), R(SI));

	//(ar&m) + (ix&m) -(int)m-1
	AND(32, R(ECX), R(EDI));
	AND(32, R(ESI), R(EDI));
	ADD(32, R(ECX), R(ESI));
	SUB(32, R(ECX), R(EDI));
	SUB(32, R(ECX), Imm32(1));

	TEST(16,R(SI), Imm16(0x8000));
	FixupBranch negative = J_CC(CC_NZ);

	CMP(32, R(ECX), Imm32(0));
	FixupBranch out1 = J_CC(CC_L);
	SUB(16,R(AX),R(DX));
	SUB(16,R(AX),Imm16(1));
	FixupBranch out2 = J();

	SetJumpTarget(negative);

	//m-wr
	SUB(32, R(EDI), R(EDX));
	CMP(32, R(ECX), R(EDI));
	FixupBranch out3 = J_CC(CC_GE);
	ADD(16,R(AX),R(DX));
	ADD(16,R(AX),Imm16(1));

	SetJumpTarget(out1);
	SetJumpTarget(out2);
	SetJumpTarget(out3);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif
}

// Decrease addr register according to the correspond ix register
// EAX = g_dsp.r[reg]
// ECX = g_dsp.r[DSP_REG_IX0 + reg]
// EDX = g_dsp.r[DSP_REG_WR0 + reg]
// EDI = tomask(EDX)
void DSPEmitter::decrease_addr_reg(int reg)
{
	/*
	u16 ar = g_dsp.r[reg];
	u16 wr = g_dsp.r[reg+8];
	u16 ix = g_dsp.r[reg+4];
	u16 m = ToMask(wr) | 1;
	u16 nar = ar-ix;   //!!
	if ((u16)ix > 0x8000) { // equiv: ix < 0 && ix != -0x8000  //!!
		if((ar&m) - (int)(ix&m) >= 0)  //!!
			nar -= wr+1;
	} else {
		if((ar&m) - (int)(ix&m) < m-wr) //!!
			nar += wr+1;
	}
	return nar;
	 */

#ifdef _M_IX86 // All32
	MOV(16, R(SI), M(&g_dsp.r[DSP_REG_IX0 + reg]));
	MOV(16, R(AX), M(&g_dsp.r[reg]));
	MOVZX(32, 16, EDX, M(&g_dsp.r[DSP_REG_WR0 + reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, R(SI), MDisp(R11,(DSP_REG_IX0 + reg)*2));
	MOV(16, R(AX), MDisp(R11,reg*2));
	MOVZX(32, 16, EDX, MDisp(R11,(DSP_REG_WR0 + reg)*2));
#endif

	// ToMask(WR0), calculating it into EDI
	//u16 m = ToMask(wr) | 1;
	MOV(16, R(DI), R(DX));
	ToMask(DI);

	//u16 nar = ar-ix;
	MOV(16, R(CX), R(AX));
	SUB(16, R(AX), R(SI));

	//(ar&m) + (ix&m)
	AND(32, R(ECX), R(EDI));
	AND(32, R(ESI), R(EDI));
	SUB(32, R(ECX), R(ESI));

	CMP(16,R(SI), Imm16(0x8000));
	FixupBranch negative = J_CC(CC_BE);

	CMP(32, R(ECX), Imm32(0));
	FixupBranch out1 = J_CC(CC_L);
	SUB(16,R(AX),R(DX));
	SUB(16,R(AX),Imm16(1));
	FixupBranch out2 = J();

	SetJumpTarget(negative);

	//m-wr
	SUB(32, R(EDI), R(EDX));
	CMP(32, R(ECX), R(EDI));
	FixupBranch out3 = J_CC(CC_GE);
	ADD(16,R(AX),R(DX));
	ADD(16,R(AX),Imm16(1));

	SetJumpTarget(out1);
	SetJumpTarget(out2);
	SetJumpTarget(out3);

	//	g_dsp.r[reg] = tmp;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp.r[reg]), R(EAX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11,reg*2), R(EAX));
#endif
}


// EAX - destination address
// ECX - value
// ESI - Base of dram
void DSPEmitter::dmem_write()
{
	//	if (saddr == 0)
	CMP(16, R(EAX), Imm16(0x0fff));
	FixupBranch ifx = J_CC(CC_A);

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

// ECX - value
void DSPEmitter::dmem_write_imm(u16 address)
{
	switch (address >> 12)
	{
	case 0x0: // 0xxx DRAM
#ifdef _M_IX86 // All32
		MOV(16, M(&g_dsp.dram[address & DSP_DRAM_MASK]), R(ECX));
#else
		MOV(64, R(RDX), ImmPtr(g_dsp.dram));
		MOV(16, MDisp(RDX,(address & DSP_DRAM_MASK)*2), R(ECX));
#endif
		break;

	case 0xf: // Fxxx HW regs
		MOV(16, R(EAX), Imm16(address));
		ABI_CallFunctionRR((void *)gdsp_ifx_write, EAX, ECX);
		break;

	default:  // Unmapped/non-existing memory
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Write to UNKNOWN (%04x) memory",
			g_dsp.pc, address);
		break;
	}
}

// In:  ECX - the address to read
// Out: EAX - the result of the read (used by caller)
// ESI - Base
void DSPEmitter::imem_read()
{
	//	if (addr == 0)
	CMP(16, R(ECX), Imm16(0x0fff));
	FixupBranch irom = J_CC(CC_A);
	//	return g_dsp.iram[addr & DSP_IRAM_MASK];
	AND(16, R(ECX), Imm16(DSP_IRAM_MASK));
#ifdef _M_X64
	MOV(64, R(ESI), ImmPtr(g_dsp.iram));
#else
	MOV(32, R(ESI), ImmPtr(g_dsp.iram));
#endif
	MOV(16, R(EAX), MComplex(ESI, ECX, 2, 0));

	FixupBranch end = J();
	SetJumpTarget(irom);
	//	else if (addr == 0x8)
	//		return g_dsp.irom[addr & DSP_IROM_MASK];
	AND(16, R(ECX), Imm16(DSP_IROM_MASK));
#ifdef _M_X64
	MOV(64, R(ESI), ImmPtr(g_dsp.irom));
#else
	MOV(32, R(ESI), ImmPtr(g_dsp.irom));
#endif
	MOV(16, R(EAX), MComplex(ESI,ECX,2,0));

	SetJumpTarget(end);
}

// In:  ECX - the address to read
// Out: EAX - the result of the read (used by caller)
// ESI - Base
// Trashes R11 on gdsp_ifx_read
void DSPEmitter::dmem_read()
{
	//	if (saddr == 0)
	CMP(16, R(ECX), Imm16(0x0fff));
	FixupBranch dram = J_CC(CC_A);
	//	return g_dsp.dram[addr & DSP_DRAM_MASK];
#ifdef _M_X64
	AND(16, R(ECX), Imm16(DSP_DRAM_MASK));
	MOVZX(64, 16, RCX, R(RCX));
	MOV(64, R(ESI), ImmPtr(g_dsp.dram));
#else
	AND(32, R(ECX), Imm32(DSP_DRAM_MASK));
	MOV(32, R(ESI), ImmPtr(g_dsp.dram));
#endif
	MOV(16, R(EAX), MComplex(ESI, ECX, 2, 0));

	FixupBranch end = J();
	SetJumpTarget(dram);
	//	else if (saddr == 0x1)
	CMP(16, R(ECX), Imm16(0x1fff));
	FixupBranch ifx = J_CC(CC_A);
	//		return g_dsp.coef[addr & DSP_COEF_MASK];
#ifdef _M_X64
	AND(16, R(ECX), Imm16(DSP_COEF_MASK));
	MOVZX(64, 16, RCX, R(RCX));
	MOV(64, R(ESI), ImmPtr(g_dsp.coef));
#else
	AND(32, R(ECX), Imm32(DSP_COEF_MASK));
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

void DSPEmitter::dmem_read_imm(u16 address)
{
	switch (address >> 12)
	{
	case 0x0:  // 0xxx DRAM
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), M(&g_dsp.dram[address & DSP_DRAM_MASK]));
#else
		MOV(64, R(RDX), ImmPtr(g_dsp.dram));
		MOV(16, R(EAX), MDisp(RDX,(address & DSP_DRAM_MASK)*2));
#endif
		break;

	case 0x1:  // 1xxx COEF
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), Imm16(g_dsp.coef[address & DSP_COEF_MASK]));
#else
		MOV(64, R(RDX), ImmPtr(g_dsp.coef));
		MOV(16, R(EAX), MDisp(RDX,(address & DSP_COEF_MASK)*2));
#endif
		break;

	case 0xf:  // Fxxx HW regs
		ABI_CallFunctionC16((void *)gdsp_ifx_read, address);
		break;

	default:   // Unmapped/non-existing memory
		ERROR_LOG(DSPLLE, "%04x DSP ERROR: Read from UNKNOWN (%04x) memory",
			g_dsp.pc, address);
	}
}

// Returns s64 in RAX
void DSPEmitter::get_long_prod(X64Reg long_prod)
{
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	//s64 val   = (s8)(u8)g_dsp.r[DSP_REG_PRODH];	
	MOVSX(64, 8, long_prod, MDisp(R11,DSP_REG_PRODH*2));
	//val <<= 32;
	SHL(64, R(long_prod), Imm8(16));
	//s64 low_prod  = g_dsp.r[DSP_REG_PRODM];
	OR(16, R(long_prod), MDisp(R11,DSP_REG_PRODM*2));
	//low_prod += g_dsp.r[DSP_REG_PRODM2];
	ADD(16, R(long_prod), MDisp(R11,DSP_REG_PRODM2*2));
	//low_prod <<= 16;
	SHL(64, R(long_prod), Imm8(16));
	//low_prod |= g_dsp.r[DSP_REG_PRODL];
	OR(16, R(long_prod), MDisp(R11,DSP_REG_PRODL*2));
	//return val;
#endif
}

// Returns s64 in RAX
// Clobbers RSI
void DSPEmitter::get_long_prod_round_prodl(X64Reg long_prod)
{
#ifdef _M_X64
	//s64 prod = dsp_get_long_prod();
	get_long_prod(long_prod);

	//if (prod & 0x10000) prod = (prod + 0x8000) & ~0xffff;
	TEST(32, R(long_prod), Imm32(0x10000));
	FixupBranch jump = J_CC(CC_Z);
	ADD(64, R(long_prod), Imm32(0x8000));
	MOV(64, R(ESI), Imm64(~0xffff));
	AND(64, R(long_prod), R(RSI));
	FixupBranch ret = J();
	//else prod = (prod + 0x7fff) & ~0xffff;
	SetJumpTarget(jump);
	ADD(64, R(long_prod), Imm32(0x7fff));
	MOV(64, R(RSI), Imm64(~0xffff));
	AND(64, R(long_prod), R(RSI));
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
	SAR(64, R(RAX), Imm8(16));
	//	g_dsp.r[DSP_REG_PRODM] = (u16)val;
	MOV(16, MDisp(R11, DSP_REG_PRODM * 2), R(AX));
	//	val >>= 16;
	SAR(64, R(RAX), Imm8(16));
	//	g_dsp.r[DSP_REG_PRODH] = (u8)val;
	MOVSX(64, 8, RAX, R(AL));
	MOV(8, MDisp(R11, DSP_REG_PRODH * 2), R(AL));
	//	g_dsp.r[DSP_REG_PRODM2] = 0;
	MOV(16, MDisp(R11, DSP_REG_PRODM2 * 2), Imm16(0));
#endif
}

// Returns s64 in RAX
// Clobbers RSI
void DSPEmitter::round_long_acc(X64Reg long_acc)
{
#ifdef _M_X64
	//if (prod & 0x10000) prod = (prod + 0x8000) & ~0xffff;
	TEST(32, R(long_acc), Imm32(0x10000));
	FixupBranch jump = J_CC(CC_Z);
	ADD(64, R(long_acc), Imm32(0x8000));
	MOV(64, R(ESI), Imm64(~0xffff));
	AND(64, R(long_acc), R(RSI));
	FixupBranch ret = J();
	//else prod = (prod + 0x7fff) & ~0xffff;
	SetJumpTarget(jump);
	ADD(64, R(long_acc), Imm32(0x7fff));
	MOV(64, R(RSI), Imm64(~0xffff));
	AND(64, R(long_acc), R(RSI));
	SetJumpTarget(ret);
	//return prod;
#endif
}

// Returns s64 in RAX
void DSPEmitter::get_long_acc(int _reg, X64Reg acc)
{
#ifdef _M_X64
//	s64 high = (s64)(s8)g_dsp.r[DSP_REG_ACH0 + reg] << 32;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 8, acc, MDisp(R11, (DSP_REG_ACH0 + _reg) * 2));
	SHL(64, R(acc), Imm8(16));
//	u32 mid_low = ((u32)g_dsp.r[DSP_REG_ACM0 + reg] << 16) | g_dsp.r[DSP_REG_ACL0 + reg];
	OR(16, R(acc), MDisp(R11, (DSP_REG_ACM0 + _reg) * 2));
	SHL(64, R(acc), Imm8(16));
	OR(16, R(acc), MDisp(R11, (DSP_REG_ACL0 + _reg) * 2));
//	return high | mid_low;
#endif
}

// In: RAX = s64 val
// Clobbers the input reg
void DSPEmitter::set_long_acc(int _reg, X64Reg acc)
{
#ifdef _M_X64
//	g_dsp.r[DSP_REG_ACL0 + _reg] = (u16)val;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11, (DSP_REG_ACL0 + _reg) * 2), R(acc));
//	val >>= 16;
	SHR(64, R(acc), Imm8(16));
//	g_dsp.r[DSP_REG_ACM0 + _reg] = (u16)val;
	MOV(16, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2), R(acc));
//	val >>= 16;
	SHR(64, R(acc), Imm8(16));
//	g_dsp.r[DSP_REG_ACH0 + _reg] = (u16)(s16)(s8)(u8)val;
	MOVSX(64, 8, acc, R(acc));
	MOV(16, MDisp(R11, (DSP_REG_ACH0 + _reg) * 2), R(acc));
#endif
}

// Returns s16 in AX
void DSPEmitter::get_acc_m(int _reg, X64Reg acm)
{
//	return g_dsp.r[DSP_REG_ACM0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, acm, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2));
#endif
}

// Returns s16 in AX
void DSPEmitter::set_acc_m(int _reg)
{
	//	return g_dsp.r[DSP_REG_ACM0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11, (DSP_REG_ACM0 + _reg) * 2), R(RAX));
#endif
}

// Returns u32 in EAX
void DSPEmitter::get_long_acx(int _reg, X64Reg acx)
{
//	return ((u32)g_dsp.r[DSP_REG_AXH0 + _reg] << 16) | g_dsp.r[DSP_REG_AXL0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, acx, MDisp(R11, (DSP_REG_AXH0 + _reg) * 2));
	SHL(64, R(acx), Imm8(16));
	OR(16, R(acx), MDisp(R11, (DSP_REG_AXL0 + _reg) * 2));
#endif
}

// Returns s16 in EAX
void DSPEmitter::get_ax_l(int _reg, X64Reg axl)
{
//	return (s16)g_dsp.r[DSP_REG_AXL0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, axl, MDisp(R11, (DSP_REG_AXL0 + _reg) * 2));
#endif
}

// Returns s16 in EAX
void DSPEmitter::get_ax_h(int _reg, X64Reg axh)
{
//	return (s16)g_dsp.r[DSP_REG_AXH0 + _reg];
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, axh, MDisp(R11, (DSP_REG_AXH0 + _reg) * 2));
#endif
}

#endif
