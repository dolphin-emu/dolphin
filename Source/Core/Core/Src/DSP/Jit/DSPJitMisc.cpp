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


#include "../DSPIntUtil.h"
#include "../DSPEmitter.h"
#include "DSPJitUtil.h"
#include "x64Emitter.h"
#include "ABI.h"
using namespace Gen;

//clobbers:
//EAX = (s8)g_dsp.reg_stack_ptr[stack_reg]
//expects:
void DSPEmitter::dsp_reg_stack_push(int stack_reg)
{
	//g_dsp.reg_stack_ptr[stack_reg]++;
	//g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
	MOV(8, R(AL), M(&g_dsp.reg_stack_ptr[stack_reg]));
	ADD(8, R(AL), Imm8(1));
	AND(8, R(AL), Imm8(DSP_STACK_MASK));
	MOV(8, M(&g_dsp.reg_stack_ptr[stack_reg]), R(AL));

	X64Reg tmp1;
	gpr.getFreeXReg(tmp1);
	//g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]] = g_dsp.r[DSP_REG_ST0 + stack_reg];
	MOV(16, R(tmp1), M(&g_dsp.r.st[stack_reg]));
#ifdef _M_IX86 // All32
	MOVZX(32, 8, EAX, R(AL));
#else
	MOVZX(64, 8, RAX, R(AL));
#endif
	MOV(16, MComplex(EAX, EAX, 1,
			 PtrOffset(&g_dsp.reg_stack[stack_reg][0],0)), R(tmp1));
	gpr.putXReg(tmp1);
}

//clobbers:
//EAX = (s8)g_dsp.reg_stack_ptr[stack_reg]
//expects:
void DSPEmitter::dsp_reg_stack_pop(int stack_reg)
{
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]];
	MOV(8, R(AL), M(&g_dsp.reg_stack_ptr[stack_reg]));
	X64Reg tmp1;
	gpr.getFreeXReg(tmp1);
#ifdef _M_IX86 // All32
	MOVZX(32, 8, EAX, R(AL));
#else
	MOVZX(64, 8, RAX, R(AL));
#endif
	MOV(16, R(tmp1), MComplex(EAX, EAX, 1,
				  PtrOffset(&g_dsp.reg_stack[stack_reg][0],0)));
	MOV(16, M(&g_dsp.r.st[stack_reg]), R(tmp1));
	gpr.putXReg(tmp1);

	//g_dsp.reg_stack_ptr[stack_reg]--;
	//g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
	SUB(8, R(AL), Imm8(1));
	AND(8, R(AL), Imm8(DSP_STACK_MASK));
	MOV(8, M(&g_dsp.reg_stack_ptr[stack_reg]), R(AL));
}


void DSPEmitter::dsp_reg_store_stack(int stack_reg, Gen::X64Reg host_sreg)
{
	if (host_sreg != EDX) {
		MOV(16, R(EDX), R(host_sreg));
	}
	dsp_reg_stack_push(stack_reg);
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
	MOV(16, M(&g_dsp.r.st[stack_reg]), R(EDX));
}

void DSPEmitter::dsp_reg_load_stack(int stack_reg, Gen::X64Reg host_dreg)
{
	//u16 val = g_dsp.r[DSP_REG_ST0 + stack_reg];
	MOV(16, R(EDX), M(&g_dsp.r.st[stack_reg]));
	dsp_reg_stack_pop(stack_reg);
	if (host_dreg != EDX) {
		MOV(16, R(host_dreg), R(EDX));
	}
}

void DSPEmitter::dsp_reg_store_stack_imm(int stack_reg, u16 val)
{
	dsp_reg_stack_push(stack_reg);
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
	MOV(16, M(&g_dsp.r.st[stack_reg]), Imm16(val));
}

void DSPEmitter::dsp_op_write_reg(int reg, Gen::X64Reg host_sreg)
{
	switch (reg & 0x1f) {
	// 8-bit sign extended registers.
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		gpr.writeReg(reg, R(host_sreg));
		break;

	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack(reg - DSP_REG_ST0, host_sreg);
		break;

	default:
		gpr.writeReg(reg, R(host_sreg));
		break;
	}
}

void DSPEmitter::dsp_op_write_reg_imm(int reg, u16 val)
{
	switch (reg & 0x1f) {
	// 8-bit sign extended registers. Should look at prod.h too...
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		gpr.writeReg(reg, Imm16((u16)(s16)(s8)(u8)val));
		break;
	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack_imm(reg - DSP_REG_ST0, val);
		break;

	default:
		gpr.writeReg(reg, Imm16(val));
		break;
	}
}

void DSPEmitter::dsp_conditional_extend_accum(int reg)
{
	switch (reg)
	{
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
	{
		OpArg sr_reg;
		gpr.getReg(DSP_REG_SR,sr_reg);
		DSPJitRegCache c(gpr);
		TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
		FixupBranch not_40bit = J_CC(CC_Z,true);
		//if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
		//{
		// Sign extend into whole accum.
		//u16 val = g_dsp.r[reg];
		get_acc_m(reg - DSP_REG_ACM0, EAX);
		SHR(32, R(EAX), Imm8(16));
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
		set_acc_h(reg - DSP_REG_ACM0, R(RAX));
		set_acc_l(reg - DSP_REG_ACM0, Imm16(0));
		//}
		gpr.flushRegs(c);
		SetJumpTarget(not_40bit);
		gpr.putReg(DSP_REG_SR, false);
	}
	}
}

void DSPEmitter::dsp_conditional_extend_accum_imm(int reg, u16 val)
{
	switch (reg)
	{
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
	{
		OpArg sr_reg;
		gpr.getReg(DSP_REG_SR,sr_reg);
		DSPJitRegCache c(gpr);
		TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
		FixupBranch not_40bit = J_CC(CC_Z, true);
		//if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
		//{
		// Sign extend into whole accum.
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
		set_acc_h(reg - DSP_REG_ACM0, Imm16((val & 0x8000)?0xffff:0x0000));
		set_acc_l(reg - DSP_REG_ACM0, Imm16(0));
		//}
		gpr.flushRegs(c);
		SetJumpTarget(not_40bit);
		gpr.putReg(DSP_REG_SR, false);
	}
	}
}

void DSPEmitter::dsp_op_read_reg(int reg, Gen::X64Reg host_dreg, DSPJitSignExtend extend)
{
	switch (reg & 0x1f) {
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_load_stack(reg - DSP_REG_ST0, host_dreg);
		switch(extend) {
		case SIGN:
#ifdef _M_IX86 // All32
			MOVSX(32, 16, host_dreg, R(host_dreg));
#else
			MOVSX(64, 16, host_dreg, R(host_dreg));
#endif
			break;
		case ZERO:
#ifdef _M_IX86 // All32
			MOVZX(32, 16, host_dreg, R(host_dreg));
#else
			MOVZX(64, 16, host_dreg, R(host_dreg));
#endif
			break;
		case NONE:
		default:
			break;
		}
		return;
	default:
		gpr.readReg(reg, host_dreg, extend);
		return;
	}
}

void DSPEmitter::dsp_op_read_reg_and_saturate(int reg, Gen::X64Reg host_dreg, DSPJitSignExtend extend)
{
	//we already know this is ACCM0 or ACCM1
#ifdef _M_IX86 // All32
	gpr.readReg(reg, host_dreg, extend);
#else
	OpArg acc_reg;
	gpr.getReg(reg-DSP_REG_ACM0+DSP_REG_ACC0_64, acc_reg);
#endif
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);

	DSPJitRegCache c(gpr);
	TEST(16, sr_reg, Imm16(SR_40_MODE_BIT));
	FixupBranch not_40bit = J_CC(CC_Z, true);

#ifdef _M_IX86 // All32
	DSPJitRegCache c2(gpr);
	gpr.putReg(DSP_REG_SR, false);
	X64Reg tmp1;
	gpr.getFreeXReg(tmp1);
	gpr.readReg(reg-DSP_REG_ACM0+DSP_REG_ACH0, tmp1, NONE);
	MOVSX(32,16,host_dreg,R(host_dreg));
	SHL(32, R(tmp1), Imm8(16));
	MOV(16,R(tmp1),R(host_dreg));
	CMP(32,R(host_dreg), R(tmp1));

	FixupBranch no_saturate = J_CC(CC_Z);

	CMP(32,R(tmp1),Imm32(0));
	FixupBranch negative = J_CC(CC_LE);

	MOV(32,R(host_dreg),Imm32(0x7fff));//this works for all extend modes
	FixupBranch done_positive = J();

	SetJumpTarget(negative);
	if (extend == NONE || extend == ZERO)
		MOV(32,R(host_dreg),Imm32(0x00008000));
	else
		MOV(32,R(host_dreg),Imm32(0xffff8000));
	FixupBranch done_negative = J();

	SetJumpTarget(no_saturate);
	if (extend == ZERO)
		MOVZX(32,16,host_dreg,R(host_dreg));
	SetJumpTarget(done_positive);
	SetJumpTarget(done_negative);
	gpr.putXReg(tmp1);
	gpr.flushRegs(c2);
	SetJumpTarget(not_40bit);
	gpr.flushRegs(c);
#else

	MOVSX(64,32,host_dreg,acc_reg);
	CMP(64,R(host_dreg),acc_reg);
	FixupBranch no_saturate = J_CC(CC_Z);

	CMP(64,acc_reg,Imm32(0));
	FixupBranch negative = J_CC(CC_LE);

	MOV(64,R(host_dreg),Imm32(0x7fff));//this works for all extend modes
	FixupBranch done_positive = J();

	SetJumpTarget(negative);
	if (extend == NONE || extend == ZERO)
		MOV(64,R(host_dreg),Imm32(0x00008000));
	else
		MOV(64,R(host_dreg),Imm32(0xffff8000));
	FixupBranch done_negative = J();

	SetJumpTarget(no_saturate);
	SetJumpTarget(not_40bit);

	MOV(64, R(host_dreg), acc_reg);
	if (extend == NONE || extend == ZERO)
		SHR(64, R(host_dreg), Imm8(16));
	else
		SAR(64, R(host_dreg), Imm8(16));
	SetJumpTarget(done_positive);
	SetJumpTarget(done_negative);
	gpr.flushRegs(c);
	gpr.putReg(reg-DSP_REG_ACM0+DSP_REG_ACC0_64, false);
#endif

	gpr.putReg(DSP_REG_SR, false);
}

// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
void DSPEmitter::mrr(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x1f;
	u8 dreg = (opc >> 5) & 0x1f;

	if (sreg >= DSP_REG_ACM0)
		dsp_op_read_reg_and_saturate(sreg, EDX);
	else
		dsp_op_read_reg(sreg, EDX);
	dsp_op_write_reg(dreg, EDX);
	dsp_conditional_extend_accum(dreg);
}

// LRI $D, #I
// 0000 0000 100d dddd
// iiii iiii iiii iiii
// Load immediate value I to register $D.
//
// DSPSpy discovery: This, and possibly other instructions that load a
// register, has a different behaviour in S40 mode if loaded to AC0.M: The
// value gets sign extended to the whole accumulator! This does not happen in
// S16 mode.
void DSPEmitter::lri(const UDSPInstruction opc)
{
	u8 reg  = opc & DSP_REG_MASK;
	u16 imm = dsp_imem_read(compilePC+1);
	dsp_op_write_reg_imm(reg, imm);
	dsp_conditional_extend_accum_imm(reg, imm);
}

// LRIS $(0x18+D), #I
// 0000 1ddd iiii iiii
// Load immediate value I (8-bit sign extended) to accumulator register.
void DSPEmitter::lris(const UDSPInstruction opc)
{
	u8 reg  = ((opc >> 8) & 0x7) + DSP_REG_AXL0;
	u16 imm = (s8)opc;
	dsp_op_write_reg_imm(reg, imm);
	dsp_conditional_extend_accum_imm(reg, imm);
}

//----

// NX
// 1000 -000 xxxx xxxx
// No operation, but can be extended with extended opcode.
// This opcode is supposed to do nothing - it's used if you want to use
// an opcode extension but not do anything. At least according to duddie.
void DSPEmitter::nx(const UDSPInstruction opc)
{
}

//----

// DAR $arD
// 0000 0000 0000 01dd
// Decrement address register $arD.
void DSPEmitter::dar(const UDSPInstruction opc)
{
	//	g_dsp.r[opc & 0x3] = dsp_decrement_addr_reg(opc & 0x3);
	decrement_addr_reg(opc & 0x3);

}

// IAR $arD
// 0000 0000 0000 10dd
// Increment address register $arD.
void DSPEmitter::iar(const UDSPInstruction opc)
{
	//	g_dsp.r[opc & 0x3] = dsp_increment_addr_reg(opc & 0x3);
	increment_addr_reg(opc & 0x3);
}

// SUBARN $arD  
// 0000 0000 0000 11dd
// Subtract indexing register $ixD from an addressing register $arD.
// used only in IPL-NTSC ucode
void DSPEmitter::subarn(const UDSPInstruction opc)
{
	//	u8 dreg = opc & 0x3;
	//	g_dsp.r[dreg] = dsp_decrease_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + dreg]);
	decrease_addr_reg(opc & 0x3);
}

// ADDARN $arD, $ixS
// 0000 0000 0001 ssdd
// Adds indexing register $ixS to an addressing register $arD.
// It is critical for the Zelda ucode that this one wraps correctly.
void DSPEmitter::addarn(const UDSPInstruction opc)
{
	//	u8 dreg = opc & 0x3;
	//	u8 sreg = (opc >> 2) & 0x3;
	//	g_dsp.r[dreg] = dsp_increase_addr_reg(dreg, (s16)g_dsp.r[DSP_REG_IX0 + sreg]);

	// From looking around it is always called with the matching index register
    increase_addr_reg(opc & 0x3, (opc >> 2) & 0x3);
}

//----


void DSPEmitter::setCompileSR(u16 bit) {
	
	//	g_dsp.r[DSP_REG_SR] |= bit
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	OR(16, sr_reg, Imm16(bit));
	gpr.putReg(DSP_REG_SR);

	compileSR |= bit;
}

void DSPEmitter::clrCompileSR(u16 bit) {
	
	//	g_dsp.r[DSP_REG_SR] &= bit
	OpArg sr_reg;
	gpr.getReg(DSP_REG_SR,sr_reg);
	AND(16, sr_reg, Imm16(~bit));
	gpr.putReg(DSP_REG_SR);

	compileSR  &= ~bit;
}
// SBCLR #I
// 0001 0011 aaaa aiii
// bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void DSPEmitter::sbclr(const UDSPInstruction opc)
{
	u8 bit = (opc & 0x7) + 6;

	clrCompileSR(1 << bit);
}

// SBSET #I
// 0001 0010 aaaa aiii
// Set bit of status register $sr. Bit number is calculated by adding 6 to
// immediate value I.
void DSPEmitter::sbset(const UDSPInstruction opc)
{
	u8 bit = (opc & 0x7) + 6;

	setCompileSR(1 << bit);
}

// 1000 1bbb xxxx xxxx, bbb >= 010
// This is a bunch of flag setters, flipping bits in SR. So far so good,
// but it's harder to know exactly what effect they have.
void DSPEmitter::srbith(const UDSPInstruction opc)
{
	switch ((opc >> 8) & 0xf)
	{
	// M0/M2 change the multiplier mode (it can multiply by 2 for free).
	case 0xa:  // M2
		clrCompileSR(SR_MUL_MODIFY);
		break;
	case 0xb:  // M0
		setCompileSR(SR_MUL_MODIFY);
		break;

	// If set, treat multiplicands as unsigned.
	// If clear, treat them as signed.
	case 0xc:  // CLR15
		clrCompileSR(SR_MUL_UNSIGNED);
		break;
	case 0xd:  // SET15
		setCompileSR(SR_MUL_UNSIGNED);
		break;

	// Automatic 40-bit sign extension when loading ACx.M.
    // SET40 changes something very important: see the LRI instruction above.
	case 0xe:  // SET16 (CLR40)
		clrCompileSR(SR_40_MODE_BIT);
		break;

	case 0xf:  // SET40
		setCompileSR(SR_40_MODE_BIT);
		break;

	default:
		break;
	}
}

