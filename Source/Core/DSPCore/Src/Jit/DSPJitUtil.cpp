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
#include "../DSPHWInterface.h"
#include "../DSPEmitter.h"
#include "DSPJitUtil.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

// addr math
//
// These functions detect overflow by checking if
// the bit past the top of the mask(WR) has changed in AR.
// They detect values under the minimum for a mask by adding wr + 1
// and checking if the bit past the top of the mask doesn't change.
// Both are done while ignoring changes due to values/holes in IX
// above the mask.


// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// EDI = temp
// ECX = temp
void DSPEmitter::increment_addr_reg(int reg)
{
	OpArg ar_reg;
	OpArg wr_reg;
	gpr.getReg(DSP_REG_WR0+reg,wr_reg);
	MOVZX(32, 16, EDX, wr_reg);
	gpr.putReg(DSP_REG_WR0+reg);
	gpr.getReg(DSP_REG_AR0+reg,ar_reg);
	MOVZX(32, 16, EAX, ar_reg);

	//u32 nar = ar + 1;
	MOV(32, R(EDI), R(EAX));
	ADD(32, R(EAX), Imm8(1));

	// if ((nar ^ ar) > ((wr | 1) << 1))
	//		nar -= wr + 1;
	XOR(32, R(EDI), R(EAX));
	LEA(32, ECX, MComplex(EDX, EDX, 1, 0));
	OR(32, R(ECX), Imm8(2));
	CMP(32, R(EDI), R(ECX));
	FixupBranch nowrap = J_CC(CC_BE);
		SUB(16, R(AX), R(DX));
		SUB(16, R(AX), Imm8(1));
	SetJumpTarget(nowrap);
	// g_dsp.r.ar[reg] = nar;

	MOV(16, ar_reg, R(AX));
	gpr.putReg(DSP_REG_AR0+reg);
}

// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// EDI = temp
// ECX = temp
void DSPEmitter::decrement_addr_reg(int reg)
{
	OpArg ar_reg;
	OpArg wr_reg;
	gpr.getReg(DSP_REG_WR0+reg,wr_reg);
	MOVZX(32, 16, EDX, wr_reg);
	gpr.putReg(DSP_REG_WR0+reg);
	gpr.getReg(DSP_REG_AR0+reg,ar_reg);
	MOVZX(32, 16, EAX, ar_reg);

	// u32 nar = ar + wr;
	// edi = nar
	LEA(32, EDI, MComplex(EAX, EDX, 1, 0));

	// if (((nar ^ ar) & ((wr | 1) << 1)) > wr)
	//		nar -= wr + 1;
	XOR(32, R(EAX), R(EDI));
	LEA(32, ECX, MComplex(EDX, EDX, 1, 0));
	OR(32, R(ECX), Imm8(2));
	AND(32, R(EAX), R(ECX));
	CMP(32, R(EAX), R(EDX));
	FixupBranch nowrap = J_CC(CC_BE);
		SUB(16, R(DI), R(DX));
		SUB(16, R(DI), Imm8(1));
	SetJumpTarget(nowrap);		
	// g_dsp.r.ar[reg] = nar;

	MOV(16, ar_reg, R(DI));
	gpr.putReg(DSP_REG_AR0+reg);
}

// Increase addr register according to the correspond ix register
// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// ESI = g_dsp.r.ix[reg]
// ECX = temp
// EDI = temp
void DSPEmitter::increase_addr_reg(int reg)
{	
	OpArg ar_reg;
	OpArg wr_reg;
	OpArg ix_reg;
	gpr.getReg(DSP_REG_WR0+reg,wr_reg);
	gpr.getReg(DSP_REG_IX0+reg,ix_reg);
	MOVZX(32, 16, EDX, wr_reg);
	MOVSX(32, 16, ESI, ix_reg);
	gpr.putReg(DSP_REG_WR0+reg);
	gpr.putReg(DSP_REG_IX0+reg);
	gpr.getReg(DSP_REG_AR0+reg,ar_reg);
	MOVZX(32, 16, EAX, ar_reg);
	
	//u32 nar = ar + ix;
	//edi = nar
	LEA(32, EDI, MComplex(EAX, ESI, 1, 0));

	//u32 dar = (nar ^ ar ^ ix) & ((wr | 1) << 1);
	//eax = dar
	XOR(32, R(EAX), R(ESI));
	XOR(32, R(EAX), R(EDI));	
	LEA(32, ECX, MComplex(EDX, EDX, 1, 0));
	OR(32, R(ECX), Imm8(2));
	AND(32, R(EAX), R(ECX));

	//if (ix >= 0)
	TEST(32, R(ESI), R(ESI));
	FixupBranch negative = J_CC(CC_S);
		//if (dar > wr)
		CMP(32, R(EAX), R(EDX));
		FixupBranch done = J_CC(CC_BE);
			//nar -= wr + 1;
			SUB(16, R(DI), R(DX));
			SUB(16, R(DI), Imm8(1));
			FixupBranch done2 = J();

	//else
	SetJumpTarget(negative);
		//if ((((nar + wr + 1) ^ nar) & dar) <= wr)
		LEA(32, ECX, MComplex(EDI, EDX, 1, 1));
		XOR(32, R(ECX), R(EDI));
		AND(32, R(ECX), R(EAX));
		CMP(32, R(ECX), R(EDX));
		FixupBranch done3 = J_CC(CC_A);
			//nar += wr + 1;
			LEA(32, EDI, MComplex(EDI, EDX, 1, 1));

	SetJumpTarget(done);
	SetJumpTarget(done2);
	SetJumpTarget(done3);
	// g_dsp.r.ar[reg] = nar;

	MOV(16, ar_reg, R(DI));
	gpr.putReg(DSP_REG_AR0+reg);
}

// Decrease addr register according to the correspond ix register
// EAX = g_dsp.r.ar[reg]
// EDX = g_dsp.r.wr[reg]
// ESI = g_dsp.r.ix[reg]
// ECX = temp
// EDI = temp
void DSPEmitter::decrease_addr_reg(int reg)
{
	OpArg ar_reg;
	OpArg wr_reg;
	OpArg ix_reg;
	gpr.getReg(DSP_REG_WR0+reg,wr_reg);
	gpr.getReg(DSP_REG_IX0+reg,ix_reg);
	MOVZX(32, 16, EDX, wr_reg);
	MOVSX(32, 16, ESI, ix_reg);
	gpr.putReg(DSP_REG_WR0+reg);
	gpr.putReg(DSP_REG_IX0+reg);
	gpr.getReg(DSP_REG_AR0+reg,ar_reg);
	MOVZX(32, 16, EAX, ar_reg);

	NOT(32, R(ESI)); //esi = ~ix

	//u32 nar = ar - ix; (ar + ~ix + 1)
	LEA(32, EDI, MComplex(EAX, ESI, 1, 1));

	//u32 dar = (nar ^ ar ^ ~ix) & ((wr | 1) << 1);
	//eax = dar
	XOR(32, R(EAX), R(ESI));
	XOR(32, R(EAX), R(EDI));
	LEA(32, ECX, MComplex(EDX, EDX, 1, 0));
	OR(32, R(ECX), Imm8(2));
	AND(32, R(EAX), R(ECX));

	//if ((u32)ix > 0xFFFF8000)  ==> (~ix < 0x00007FFF)
	CMP(32, R(ESI), Imm32(0x00007FFF));
	FixupBranch positive = J_CC(CC_AE);
		//if (dar > wr)
		CMP(32, R(EAX), R(EDX));
		FixupBranch done = J_CC(CC_BE);
			//nar -= wr + 1;
			SUB(16, R(DI), R(DX));
			SUB(16, R(DI), Imm8(1));
			FixupBranch done2 = J();

	//else
	SetJumpTarget(positive);
		//if ((((nar + wr + 1) ^ nar) & dar) <= wr)
		LEA(32, ECX, MComplex(EDI, EDX, 1, 1));
		XOR(32, R(ECX), R(EDI));
		AND(32, R(ECX), R(EAX));
		CMP(32, R(ECX), R(EDX));
		FixupBranch done3 = J_CC(CC_A);
			//nar += wr + 1;
			LEA(32, EDI, MComplex(EDI, EDX, 1, 1));

	SetJumpTarget(done);
	SetJumpTarget(done2);
	SetJumpTarget(done3);
	//return nar

	MOV(16, ar_reg, R(DI));
	gpr.putReg(DSP_REG_AR0+reg);
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
	DSPJitRegCache c(gpr);
	SaveDSPRegs();
	ABI_CallFunctionRR((void *)gdsp_ifx_write, EAX, ECX);
	LoadDSPRegs();
	gpr.flushRegs(c);
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
		MOV(16, MDisp(RDX, (address & DSP_DRAM_MASK)*2), R(ECX));
#endif
		break;

	case 0xf: // Fxxx HW regs
		MOV(16, R(EAX), Imm16(address));
		SaveDSPRegs();
		ABI_CallFunctionRR((void *)gdsp_ifx_write, EAX, ECX);
		LoadDSPRegs();
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
	MOV(16, R(EAX), MComplex(ESI, ECX, 2, 0));

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
	MOV(16, R(EAX), MComplex(ESI, ECX, 2, 0));

	FixupBranch end2 = J();
	SetJumpTarget(ifx);
	//	else if (saddr == 0xf)
	//		return gdsp_ifx_read(addr);
	DSPJitRegCache c(gpr);
	SaveDSPRegs();
	ABI_CallFunctionR((void *)gdsp_ifx_read, ECX);
	LoadDSPRegs();
	gpr.flushRegs(c);
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
		MOV(16, R(EAX), MDisp(RDX, (address & DSP_DRAM_MASK)*2));
#endif
		break;

	case 0x1:  // 1xxx COEF
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), Imm16(g_dsp.coef[address & DSP_COEF_MASK]));
#else
		MOV(64, R(RDX), ImmPtr(g_dsp.coef));
		MOV(16, R(EAX), MDisp(RDX, (address & DSP_COEF_MASK)*2));
#endif
		break;

	case 0xf:  // Fxxx HW regs
		SaveDSPRegs();
		ABI_CallFunctionC16((void *)gdsp_ifx_read, address);
		LoadDSPRegs();
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
	// MOV(64, R(R11), ImmPtr(&g_dsp.r));
	//s64 val   = (s8)(u8)g_dsp.r[DSP_REG_PRODH];
	OpArg reg;
	gpr.getReg(DSP_REG_PROD_64, reg);
	X64Reg tmp;
	gpr.getFreeXReg(tmp);
	MOV(64, R(long_prod), reg);
	MOV(64, R(tmp), R(long_prod));
	SHL(64, R(long_prod), Imm8(64-40));//sign extend
	SAR(64, R(long_prod), Imm8(64-40));
	SHR(64, R(tmp), Imm8(48));
	SHL(64, R(tmp), Imm8(16));
	ADD(64, R(long_prod), R(tmp));
	gpr.putXReg(tmp);
	gpr.putReg(DSP_REG_PROD_64, false);

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
	FixupBranch _ret = J();
	//else prod = (prod + 0x7fff) & ~0xffff;
	SetJumpTarget(jump);
	ADD(64, R(long_prod), Imm32(0x7fff));
	MOV(64, R(RSI), Imm64(~0xffff));
	AND(64, R(long_prod), R(RSI));
	SetJumpTarget(_ret);
	//return prod;
#endif
}

// For accurate emulation, this is wrong - but the real prod registers behave
// in completely bizarre ways. Probably not meaningful to emulate them accurately.
// In: RAX = s64 val
void DSPEmitter::set_long_prod()
{
#ifdef _M_X64
	OpArg reg;
	gpr.getReg(DSP_REG_PROD_64, reg, false);
	X64Reg tmp;
	gpr.getFreeXReg(tmp);

	MOV(64, R(tmp), Imm64(0x000000ffffffffffULL));
	AND(64, R(RAX), R(tmp));
	//	g_dsp.r[DSP_REG_PRODL] = (u16)val;
	MOV(64, reg, R(RAX));

	gpr.putXReg(tmp);
	gpr.putReg(DSP_REG_PROD_64, true);
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
	FixupBranch _ret = J();
	//else prod = (prod + 0x7fff) & ~0xffff;
	SetJumpTarget(jump);
	ADD(64, R(long_acc), Imm32(0x7fff));
	MOV(64, R(RSI), Imm64(~0xffff));
	AND(64, R(long_acc), R(RSI));
	SetJumpTarget(_ret);
	//return prod;
#endif
}

// Returns s64 in acc
void DSPEmitter::get_long_acc(int _reg, X64Reg acc)
{
#ifdef _M_X64
	OpArg reg;
	gpr.getReg(DSP_REG_ACC0_64+_reg, reg);
	MOV(64, R(acc), reg);
	gpr.putReg(DSP_REG_ACC0_64+_reg, false);
#endif
}

// In: acc = s64 val
void DSPEmitter::set_long_acc(int _reg, X64Reg acc)
{
#ifdef _M_X64
	OpArg reg;
	gpr.getReg(DSP_REG_ACC0_64+_reg, reg, false);
	MOV(64, reg, R(acc));
	gpr.putReg(DSP_REG_ACC0_64+_reg);
#endif
}

// Returns s16 in AX
void DSPEmitter::get_acc_l(int _reg, X64Reg acl, bool sign)
{
	//	return g_dsp.r[DSP_REG_ACM0 + _reg];
	gpr.readReg(_reg+DSP_REG_ACL0, acl, sign?SIGN:ZERO);
}

void DSPEmitter::set_acc_l(int _reg, OpArg arg)
{
	//	return g_dsp.r[DSP_REG_ACM0 + _reg];
	gpr.writeReg(_reg+DSP_REG_ACL0,arg);
}

// Returns s16 in AX
void DSPEmitter::get_acc_m(int _reg, X64Reg acm, bool sign)
{
//	return g_dsp.r[DSP_REG_ACM0 + _reg];
	gpr.readReg(_reg+DSP_REG_ACM0, acm, sign?SIGN:ZERO);
}

// In: s16 in AX
void DSPEmitter::set_acc_m(int _reg, OpArg arg)
{
	//	return g_dsp.r.ac[_reg].m;
	gpr.writeReg(_reg+DSP_REG_ACM0,arg);
}

// Returns s16 in AX
void DSPEmitter::get_acc_h(int _reg, X64Reg ach, bool sign)
{
//	return g_dsp.r.ac[_reg].h;
	gpr.readReg(_reg+DSP_REG_ACH0, ach, sign?SIGN:ZERO);
}

// In: s16 in AX
void DSPEmitter::set_acc_h(int _reg, OpArg arg)
{
	//	return g_dsp.r[DSP_REG_ACM0 + _reg];
	gpr.writeReg(_reg+DSP_REG_ACH0,arg);
}

// Returns u32 in EAX
void DSPEmitter::get_long_acx(int _reg, X64Reg acx)
{
//	return ((u32)g_dsp.r[DSP_REG_AXH0 + _reg] << 16) | g_dsp.r[DSP_REG_AXL0 + _reg];
	gpr.readReg(_reg+DSP_REG_AX0_32, acx, SIGN);
}

// Returns s16 in EAX
void DSPEmitter::get_ax_l(int _reg, X64Reg axl)
{
//	return (s16)g_dsp.r[DSP_REG_AXL0 + _reg];
	gpr.readReg(_reg+DSP_REG_AXL0, axl, SIGN);
}

// Returns s16 in EAX
void DSPEmitter::get_ax_h(int _reg, X64Reg axh)
{
//	return (s16)g_dsp.r[DSP_REG_AXH0 + _reg];
	gpr.readReg(_reg+DSP_REG_AXH0, axh, SIGN);
}

void DSPEmitter::LoadDSPRegs()
{
#ifdef _M_X64
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
#endif

	// Load DSP register state here...
	gpr.loadStaticRegs();
}

void DSPEmitter::SaveDSPRegs()
{
	// Save DSP register state here...
	gpr.saveStaticRegs();
}
