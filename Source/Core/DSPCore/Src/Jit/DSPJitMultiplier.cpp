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

// Additional copyrights go to Duddie and Tratax (c) 2004


// Multiplier and product register control

#include "../DSPIntUtil.h"
#include "../DSPEmitter.h"
#include "../DSPAnalyzer.h"
#include "x64Emitter.h"
#include "ABI.h"
using namespace Gen;

// Only MULX family instructions have unsigned/mixed support.
// Returns s64 in EAX
// In: RSI = u16 a, RDI = u16 b, RCX = u8 sign
void DSPEmitter::get_multiply_prod()
{
#ifdef _M_X64
//	if ((sign == 1) && (g_dsp.r[DSP_REG_SR] & SR_MUL_UNSIGNED)) //unsigned
	MOV(16, R(RDX), MDisp(R11, DSP_REG_SR * 2)); // TODO check 16bit
	AND(16, R(RDX), Imm16(SR_MUL_UNSIGNED));
	TEST(16, R(RDX), R(RDX));
	FixupBranch sign3 = J_CC(CC_Z);
	TEST(32, R(ECX), Imm32(1));
	FixupBranch sign1 = J_CC(CC_Z);
//		prod = (u32)(a * b);
	MOV(64, R(EAX), R(RDI));
	MUL(16, R(ESI));
	FixupBranch mult2 = J();
	SetJumpTarget(sign1);
	TEST(32, R(ECX), Imm32(2));
	FixupBranch sign2 = J_CC(CC_Z);
//	else if ((sign == 2) && (g_dsp.r[DSP_REG_SR] & SR_MUL_UNSIGNED)) //mixed
//		prod = a * (s16)b;
	MOVSX(64, 16, RDI, R(RDI));
	MOV(64, R(EAX), R(RDI));
	MUL(16, R(ESI));
//	else
	SetJumpTarget(sign2);
	SetJumpTarget(sign3);
//		prod = (s16)a * (s16)b; //signed
	MOV(64, R(EAX), R(RDI));
	IMUL(64, R(ESI));

//	Conditionally multiply by 2.
	SetJumpTarget(mult2);
//	if ((g_dsp.r[DSP_REG_SR] & SR_MUL_MODIFY) == 0)	
	TEST(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_MUL_MODIFY));
	FixupBranch noMult2 = J_CC(CC_NZ);
//		prod <<= 1;
	SHL(64, R(EAX), Imm8(1));
	SetJumpTarget(noMult2);
//	return prod;
#endif
}

// Returns s64 in RAX
// In: RSI = s16 a, RDI = s16 b
void DSPEmitter::multiply()
{
#ifdef _M_X64
//	prod = (s16)a * (s16)b; //signed
	MOV(64, R(EAX), R(RDI));
	IMUL(64, R(ESI));

//	Conditionally multiply by 2.
//	if ((g_dsp.r[DSP_REG_SR] & SR_MUL_MODIFY) == 0)	
	TEST(16, MDisp(R11, DSP_REG_SR * 2), Imm16(SR_MUL_MODIFY));
	FixupBranch noMult2 = J_CC(CC_NZ);
//		prod <<= 1;
	SHL(64, R(EAX), Imm8(1));
	SetJumpTarget(noMult2);
//	return prod;
#endif
}

// Returns s64 in RAX
// Clobbers RDX
void DSPEmitter::multiply_add()
{
//	s64 prod = dsp_get_long_prod() + dsp_get_multiply_prod(a, b, sign);
	multiply();
	MOV(64, R(RDX), R(RAX));
	get_long_prod();	
	ADD(64, R(RAX), R(RDX));
//	return prod;
}

// Returns s64 in RAX
// Clobbers RDX
void DSPEmitter::multiply_sub()
{
//	s64 prod = dsp_get_long_prod() - dsp_get_multiply_prod(a, b, sign);
	multiply();
	MOV(64, R(RDX), R(RAX));
	get_long_prod();	
	SUB(64, R(RAX), R(RDX));
//	return prod;
}

//inline s64 dsp_multiply_mulx(u8 axh0, u8 axh1, u16 val1, u16 val2)
//{
//	s64 result;

//	if ((axh0==0) && (axh1==0))
//		result = dsp_multiply(val1, val2, 1); // unsigned support ON if both ax?.l regs are used
//	else if ((axh0==0) && (axh1==1))
//		result = dsp_multiply(val1, val2, 2); // mixed support ON (u16)axl.0  * (s16)axh.1
//	else if ((axh0==1) && (axh1==0))
//		result = dsp_multiply(val2, val1, 2); // mixed support ON (u16)axl.1  * (s16)axh.0
//	else
//		result = dsp_multiply(val1, val2, 0); // unsigned support OFF if both ax?.h regs are used

//	return result;
//}

//----

// CLRP
// 1000 0100 xxxx xxxx
// Clears product register $prod.
// Magic numbers taken from duddie's doc

// 00ff_(fff0 + 0010)_0000 = 0100_0000_0000, conveniently, lower 40bits = 0

// It's not ok, to just zero all of them, correct values should be set because of
// direct use of prod regs by AX/AXWII (look @that part of ucode).
void DSPEmitter::clrp(const UDSPInstruction opc)
{
#ifdef _M_X64
//	g_dsp.r[DSP_REG_PRODL] = 0x0000;
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOV(16, MDisp(R11, DSP_REG_PRODL * 2), Imm16(0x0000));
//	g_dsp.r[DSP_REG_PRODM] = 0xfff0;
	MOV(16, MDisp(R11, DSP_REG_PRODM * 2), Imm16(0xfff0));
//	g_dsp.r[DSP_REG_PRODH] = 0x00ff;
	MOV(16, MDisp(R11, DSP_REG_PRODH * 2), Imm16(0x00ff));
//	g_dsp.r[DSP_REG_PRODM2] = 0x0010;
	MOV(16, MDisp(R11, DSP_REG_PRODM2 * 2), Imm16(0x0010));
#else
	Default(opc);
#endif
}

// TSTPROD
// 1000 0101 xxxx xxxx
// Test prod regs value.

// flags out: --xx xx0x
void DSPEmitter::tstprod(const UDSPInstruction opc)
{
#ifdef _M_X64
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
//		s64 prod = dsp_get_long_prod();
		get_long_prod();
//		Update_SR_Register64(prod);
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

//----

// MOVP $acD
// 0110 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD register.

// flags out: --xx xx0x
void DSPEmitter::movp(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 dreg = (opc >> 8) & 0x1;

//	s64 acc = dsp_get_long_prod();
	get_long_prod();
//	dsp_set_long_acc(dreg, acc);
	set_long_acc(dreg);
//	Update_SR_Register64(acc);
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MOVNP $acD
// 0111 111d xxxx xxxx 
// Moves negative of multiply product from $prod register to accumulator
// $acD register.

// flags out: --xx xx0x
void DSPEmitter::movnp(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 dreg = (opc >> 8) & 0x1;

//	s64 acc = -dsp_get_long_prod();
	get_long_prod();
	NEG(64, R(EAX));
//	dsp_set_long_acc(dreg, acc);
	set_long_acc(dreg);
//	Update_SR_Register64(acc);
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MOVPZ $acD
// 1111 111d xxxx xxxx
// Moves multiply product from $prod register to accumulator $acD
// register and sets (rounds) $acD.l to 0

// flags out: --xx xx0x
void DSPEmitter::movpz(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 dreg = (opc >> 8) & 0x01;

//	s64 acc = dsp_get_long_prod_round_prodl();
	get_long_prod_round_prodl();
//	dsp_set_long_acc(dreg, acc);
	set_long_acc(dreg);
//	Update_SR_Register64(acc);
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// ADDPAXZ $acD, $axS
// 1111 10sd xxxx xxxx
// Adds secondary accumulator $axS to product register and stores result
// in accumulator register. Low 16-bits of $acD ($acD.l) are set (round) to 0.

// flags out: --xx xx0x
//void DSPEmitter::addpaxz(const UDSPInstruction opc)
//{
//	u8 dreg = (opc >> 8) & 0x1;
//	u8 sreg = (opc >> 9) & 0x1;

//	s64 oldprod = dsp_get_long_prod();
//	s64 prod = dsp_get_long_prod_round_prodl();
//	s64 ax = dsp_get_long_acx(sreg);
//	s64 res = prod + (ax & ~0xffff);

//	zeroWriteBackLog();

//	dsp_set_long_acc(dreg, res);
//	res = dsp_get_long_acc(dreg);
//	Update_SR_Register64(res, isCarry(oldprod, res), false); 
//}

//----

// MULAXH
// 1000 0011 xxxx xxxx
// Multiply $ax0.h by $ax0.h 
void DSPEmitter::mulaxh(const UDSPInstruction opc)
{
#ifdef _M_X64
//	s64 prod = dsp_multiply(dsp_get_ax_h(0), dsp_get_ax_h(0));
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RSI, MDisp(R11, DSP_REG_AXH0 * 2));
	MOV(64, R(RDI), R(RSI));
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

//----

// MUL $axS.l, $axS.h
// 1001 s000 xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed).
void DSPEmitter::mul(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 sreg  = (opc >> 11) & 0x1;

//	u16 axl = dsp_get_ax_l(sreg);
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_AXL0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(sreg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + sreg) * 2));
//	s64 prod = dsp_multiply(axh, axl);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

// MULAC $axS.l, $axS.h, $acR
// 1001 s10r xxxx xxxx
// Add product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).

// flags out: --xx xx0x
void DSPEmitter::mulac(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg = (opc >> 8) & 0x1;
	u8 sreg = (opc >> 11) & 0x1;

//	s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	get_long_acc(rreg);
	MOV(64, R(RDX), R(RAX));
	get_long_prod();
	ADD(64, R(RAX), R(RDX));
	PUSH(64, R(RAX));
//	u16 axl = dsp_get_ax_l(sreg);
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_AXL0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(sreg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + sreg) * 2));
//	s64 prod = dsp_multiply(axl, axh);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MULMV $axS.l, $axS.h, $acR
// 1001 s11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply low part
// $axS.l of secondary accumulator $axS by high part $axS.h of secondary
// accumulator $axS (treat them both as signed).

// flags out: --xx xx0x
void DSPEmitter::mulmv(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg  = (opc >> 8) & 0x1;

//	s64 acc = dsp_get_long_prod();
	get_long_prod();
	PUSH(64, R(RAX));
	mul(opc);
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MULMVZ $axS.l, $axS.h, $acR
// 1001 s01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply low part $axS.l of secondary
// accumulator $axS by high part $axS.h of secondary accumulator $axS (treat
// them both as signed).

// flags out: --xx xx0x
void DSPEmitter::mulmvz(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg = (opc >> 8) & 0x1;

//	s64 acc = dsp_get_long_prod_round_prodl();
	get_long_prod_round_prodl();
	PUSH(64, R(RAX));
	mul(opc);
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

//----

// MULX $ax0.S, $ax1.T
// 101s t000 xxxx xxxx
// Multiply one part $ax0 by one part $ax1.
// Part is selected by S and T bits. Zero selects low part, one selects high part.
//void DSPEmitter::mulx(const UDSPInstruction opc)
//{
//	u8 treg = ((opc >> 11) & 0x1);
//	u8 sreg = ((opc >> 12) & 0x1);

//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//}

// MULXAC $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Add product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.

// flags out: --xx xx0x
//void DSPEmitter::mulxac(const UDSPInstruction opc)
//{
//	u8 rreg = (opc >> 8) & 0x1;
//	u8 treg = (opc >> 11) & 0x1;
//	u8 sreg = (opc >> 12) & 0x1;

//	s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);
//	
//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//	dsp_set_long_acc(rreg, acc);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
//}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t11r xxxx xxxx
// Move product register to accumulator register $acR. Multiply one part
// $ax0 by one part $ax1. Part is selected by S and
// T bits. Zero selects low part, one selects high part.

// flags out: --xx xx0x
//void DSPEmitter::mulxmv(const UDSPInstruction opc)
//{
//	u8 rreg = ((opc >> 8) & 0x1);
//	u8 treg = (opc >> 11) & 0x1;
//	u8 sreg = (opc >> 12) & 0x1;

//	s64 acc = dsp_get_long_prod();
//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//	dsp_set_long_acc(rreg, acc);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
//}

// MULXMV $ax0.S, $ax1.T, $acR
// 101s t01r xxxx xxxx
// Move product register to accumulator register $acR and clear (round) low part
// of accumulator register $acR.l. Multiply one part $ax0 by one part $ax1
// Part is selected by S and T bits. Zero selects low part,
// one selects high part.

// flags out: --xx xx0x
//void DSPEmitter::mulxmvz(const UDSPInstruction opc)
//{
//	u8 rreg  = (opc >> 8) & 0x1;
//	u8 treg = (opc >> 11) & 0x1;
//	u8 sreg = (opc >> 12) & 0x1;

//	s64 acc = dsp_get_long_prod_round_prodl();
//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_mulx(sreg, treg, val1, val2);

//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//	dsp_set_long_acc(rreg, acc);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
//}

//----

// MULC $acS.m, $axT.h
// 110s t000 xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS (treat them both as signed).
void DSPEmitter::mulc(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 treg = (opc >> 11) & 0x1;
	u8 sreg = (opc >> 12) & 0x1;

//	u16 accm = dsp_get_acc_m(sreg);
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, ESI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));	
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, EDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply(accm, axh);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

// MULCAC $acS.m, $axT.h, $acR
// 110s	t10r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axS.h of
// secondary accumulator $axS  (treat them both as signed). Add product
// register before multiplication to accumulator $acR.

// flags out: --xx xx0x
void DSPEmitter::mulcac(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg = (opc >> 8) & 0x1;
	u8 treg  = (opc >> 11) & 0x1;
	u8 sreg  = (opc >> 12) & 0x1;

//	s64 acc = dsp_get_long_acc(rreg) + dsp_get_long_prod();
	get_long_acc(rreg);
	MOV(64, R(RDX), R(RAX));
	get_long_prod();
	ADD(64, R(RAX), R(RDX));
	PUSH(64, R(RAX));
//	u16 accm = dsp_get_acc_m(sreg);
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply(accm, axh);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MULCMV $acS.m, $axT.h, $acR
// 110s t11r xxxx xxxx
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR.
// possible mistake in duddie's doc axT.h rather than axS.h

// flags out: --xx xx0x
void DSPEmitter::mulcmv(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg = (opc >> 8) & 0x1;
	u8 treg  = (opc >> 11) & 0x1;
	u8 sreg  = (opc >> 12) & 0x1;

//	s64 acc = dsp_get_long_prod();
	get_long_prod();
	PUSH(64, R(RAX));
//	u16 accm = dsp_get_acc_m(sreg);
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply(accm, axh);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

// MULCMVZ $acS.m, $axT.h, $acR
// 110s	t01r xxxx xxxx
// (fixed possible bug in duddie's description, s->t)
// Multiply mid part of accumulator register $acS.m by high part $axT.h of
// secondary accumulator $axT  (treat them both as signed). Move product
// register before multiplication to accumulator $acR, set (round) low part of 
// accumulator $acR.l to zero.

// flags out: --xx xx0x
void DSPEmitter::mulcmvz(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 rreg = (opc >> 8) & 0x1;
	u8 treg  = (opc >> 11) & 0x1;
	u8 sreg  = (opc >> 12) & 0x1;

	MOV(64, R(R11), ImmPtr(&g_dsp.r));
//	s64 acc = dsp_get_long_prod_round_prodl();
	get_long_prod_round_prodl();
	PUSH(64, R(RAX));
//	u16 accm = dsp_get_acc_m(sreg);
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply(accm, axh);
	multiply();
//	dsp_set_long_prod(prod);
	set_long_prod();
//	dsp_set_long_acc(rreg, acc);
	POP(64, R(RAX));
	set_long_acc(rreg);
//	Update_SR_Register64(dsp_get_long_acc(rreg));
	if (!(DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_START_OF_INST) || (DSPAnalyzer::code_flags[compilePC] & DSPAnalyzer::CODE_UPDATE_SR))
	{
		Update_SR_Register64();
	}
#else
	Default(opc);
#endif
}

//----

// MADDX ax0.S ax1.T
// 1110 00st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and add result to product register.
//void DSPEmitter::maddx(const UDSPInstruction opc)
//{
//	u8 treg = (opc >> 8) & 0x1;
//	u8 sreg = (opc >> 9) & 0x1;

//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_add(val1, val2);
//	
//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//}

// MSUBX $(0x18+S*2), $(0x19+T*2)
// 1110 01st xxxx xxxx
// Multiply one part of secondary accumulator $ax0 (selected by S) by
// one part of secondary accumulator $ax1 (selected by T) (treat them both as
// signed) and subtract result from product register.
//void DSPEmitter::msubx(const UDSPInstruction opc)
//{
//	u8 treg = (opc >> 8) & 0x1;
//	u8 sreg = (opc >> 9) & 0x1;

//	u16 val1 = (sreg == 0) ? dsp_get_ax_l(0) : dsp_get_ax_h(0);
//	u16 val2 = (treg == 0) ? dsp_get_ax_l(1) : dsp_get_ax_h(1);
//	s64 prod = dsp_multiply_sub(val1, val2);

//	zeroWriteBackLog();

//	dsp_set_long_prod(prod);
//}

// MADDC $acS.m, $axT.h
// 1110 10st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and add result to product
// register.
void DSPEmitter::maddc(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 treg = (opc >> 8) & 0x1;
	u8 sreg = (opc >> 9) & 0x1;

	MOV(64, R(R11), ImmPtr(&g_dsp.r));
//	u16 accm = dsp_get_acc_m(sreg);	
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply_add(accm, axh);
	multiply_add();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

// MSUBC $acS.m, $axT.h
// 1110 11st xxxx xxxx
// Multiply middle part of accumulator $acS.m by high part of secondary
// accumulator $axT.h (treat them both as signed) and subtract result from
// product register.
void DSPEmitter::msubc(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 treg = (opc >> 8) & 0x1;
	u8 sreg = (opc >> 9) & 0x1;
//	
//	u16 accm = dsp_get_acc_m(sreg);
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_ACM0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(treg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + treg) * 2));
//	s64 prod = dsp_multiply_sub(accm, axh);
	multiply_sub();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

// MADD $axS.l, $axS.h
// 1111 001s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and add
// result to product register.
void DSPEmitter::madd(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 sreg = (opc >> 8) & 0x1;

	MOV(64, R(R11), ImmPtr(&g_dsp.r));
//	u16 axl = dsp_get_ax_l(sreg);
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_AXL0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(sreg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + sreg) * 2));
//	s64 prod = dsp_multiply_add(axl, axh);
	multiply_add();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}

// MSUB $axS.l, $axS.h
// 1111 011s xxxx xxxx
// Multiply low part $axS.l of secondary accumulator $axS by high part
// $axS.h of secondary accumulator $axS (treat them both as signed) and
// subtract result from product register.
void DSPEmitter::msub(const UDSPInstruction opc)
{
#ifdef _M_X64
	u8 sreg = (opc >> 8) & 0x1;
//	
//	u16 axl = dsp_get_ax_l(sreg);
	MOV(64, R(R11), ImmPtr(&g_dsp.r));
	MOVSX(64, 16, RSI, MDisp(R11, (DSP_REG_AXL0 + sreg) * 2));
//	u16 axh = dsp_get_ax_h(sreg);
	MOVSX(64, 16, RDI, MDisp(R11, (DSP_REG_AXH0 + sreg) * 2));
//	s64 prod = dsp_multiply_sub(axl, axh);
	multiply_sub();
//	dsp_set_long_prod(prod);
	set_long_prod();
#else
	Default(opc);
#endif
}
