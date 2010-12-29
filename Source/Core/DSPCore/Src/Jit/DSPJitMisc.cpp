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
//R10 = &g_dsp.reg_stack[stack_reg][0]
//R11 = &g_dsp.r
//CX = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]]
void DSPEmitter::dsp_reg_stack_push(int stack_reg)
{
	//g_dsp.reg_stack_ptr[stack_reg]++;
	//g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
#ifdef _M_IX86 // All32
	MOV(8, R(AL), M(&g_dsp.reg_stack_ptr[stack_reg]));
#else
	MOV(64, R(R10), ImmPtr(g_dsp.reg_stack_ptr));
	MOV(8, R(AL), MDisp(R10,stack_reg));
#endif
	ADD(8, R(AL), Imm8(1));
	AND(8, R(AL), Imm8(DSP_STACK_MASK));
#ifdef _M_IX86 // All32
	MOV(8, M(&g_dsp.reg_stack_ptr[stack_reg]), R(AL));
#else
	MOV(8, MDisp(R10,stack_reg), R(AL));
#endif

	//g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]] = g_dsp.r[DSP_REG_ST0 + stack_reg];
#ifdef _M_IX86 // All32
	MOV(16, R(CX), M(&g_dsp._r.st[stack_reg]));
	MOVZX(32, 8, EAX, R(AL));
	MOV(16, MComplex(EAX,EAX,1,(u32)&g_dsp.reg_stack[stack_reg][0]), R(CX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, R(CX), MDisp(R11,STRUCT_OFFSET(g_dsp._r, st[stack_reg])));
	MOVZX(64, 8, RAX, R(AL));
	MOV(64, R(R10), ImmPtr(&g_dsp.reg_stack[stack_reg][0]));
	MOV(16, MComplex(R10,RAX,2,0), R(CX));
#endif
}

//clobbers:
//EAX = (s8)g_dsp.reg_stack_ptr[stack_reg]
//R10 = &g_dsp.reg_stack[stack_reg][0]
//R11 = &g_dsp.r
//CX = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]]
void DSPEmitter::dsp_reg_stack_pop(int stack_reg)
{
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = g_dsp.reg_stack[stack_reg][g_dsp.reg_stack_ptr[stack_reg]];
#ifdef _M_IX86 // All32
	MOV(8, R(AL), M(&g_dsp.reg_stack_ptr[stack_reg]));
#else
	MOV(64, R(R10), ImmPtr(g_dsp.reg_stack_ptr));
	MOV(8, R(AL), MDisp(R10,stack_reg));
#endif
#ifdef _M_IX86 // All32
	MOVZX(32, 8, EAX, R(AL));
	MOV(16, R(CX), MComplex(EAX,EAX,1,(u32)&g_dsp.reg_stack[stack_reg][0]));
	MOV(16, M(&g_dsp._r.st[stack_reg]), R(CX));
#else
	MOVZX(64, 8, RAX, R(AL));
	MOV(64, R(R10), ImmPtr(&g_dsp.reg_stack[stack_reg][0]));
	MOV(16, R(CX), MComplex(R10,RAX,2,0));
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, st[stack_reg])), R(CX));
#endif

	//g_dsp.reg_stack_ptr[stack_reg]--;
	//g_dsp.reg_stack_ptr[stack_reg] &= DSP_STACK_MASK;
	SUB(8, R(AL), Imm8(1));
	AND(8, R(AL), Imm8(DSP_STACK_MASK));
#ifdef _M_IX86 // All32
	MOV(8, M(&g_dsp.reg_stack_ptr[stack_reg]), R(AL));
#else
	MOV(64, R(R10), ImmPtr(g_dsp.reg_stack_ptr));
	MOV(8, MDisp(R10,stack_reg), R(AL));
#endif
}


void DSPEmitter::dsp_reg_store_stack(int stack_reg, Gen::X64Reg host_sreg)
{
	if (host_sreg != EDX) {
		MOV(16, R(EDX), R(host_sreg));
	}
	dsp_reg_stack_push(stack_reg);
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.st[stack_reg]), R(EDX));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, st[stack_reg])), R(EDX));
#endif
}

void DSPEmitter::dsp_reg_load_stack(int stack_reg, Gen::X64Reg host_dreg)
{
	//u16 val = g_dsp.r[DSP_REG_ST0 + stack_reg];
#ifdef _M_IX86 // All32
	MOV(16, R(EDX), M(&g_dsp._r.st[stack_reg]));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, R(EDX), MDisp(R11,STRUCT_OFFSET(g_dsp._r, st[stack_reg])));
#endif
	dsp_reg_stack_pop(stack_reg);
	if (host_dreg != EDX) {
		MOV(16, R(host_dreg), R(EDX));
	}
}

void DSPEmitter::dsp_reg_store_stack_imm(int stack_reg, u16 val)
{
	dsp_reg_stack_push(stack_reg);
	//g_dsp.r[DSP_REG_ST0 + stack_reg] = val;
#ifdef _M_IX86 // All32
	MOV(16, M(&g_dsp._r.st[stack_reg]), Imm16(val));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, st[stack_reg])), Imm16(val));
#endif
}

void DSPEmitter::dsp_op_write_reg(int reg, Gen::X64Reg host_sreg)
{
	switch (reg & 0x1f) {
	// 8-bit sign extended registers. Should look at prod.h too...
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		// sign extend from the bottom 8 bits.
		MOVSX(16,8,host_sreg,R(host_sreg));
#ifdef _M_IX86 // All32
		MOV(16, M(&g_dsp._r.ac[reg-DSP_REG_ACH0].h), R(host_sreg));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACH0].h)), R(host_sreg));
#endif
		break;

	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack(reg - DSP_REG_ST0, host_sreg);
		break;

	default:
	{
		u16 *regp = reg_ptr(reg);
#ifdef _M_IX86 // All32
		MOV(16, M(regp), R(host_sreg));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, MDisp(R11,PtrOffset(regp,&g_dsp._r)), R(host_sreg));
#endif
		break;
	}
	}
}

void DSPEmitter::dsp_op_write_reg_imm(int reg, u16 val)
{
	switch (reg & 0x1f) {
	// 8-bit sign extended registers. Should look at prod.h too...
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		// sign extend from the bottom 8 bits.
#ifdef _M_IX86 // All32
		MOV(16, M(&g_dsp._r.ac[reg-DSP_REG_ACH0].h), Imm16((u16)(s16)(s8)(u8)val));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACH0].h)), Imm16((u16)(s16)(s8)(u8)val));
#endif
		break;

	// Stack registers.
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp_reg_store_stack_imm(reg - DSP_REG_ST0, val);
		break;

	default:
	{
		u16 *regp = reg_ptr(reg);
#ifdef _M_IX86 // All32
		MOV(16, M(regp), Imm16(val));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, MDisp(R11,PtrOffset(regp,&g_dsp._r)), Imm16(val));
#endif
		break;
	}
	}
}

void DSPEmitter::dsp_conditional_extend_accum(int reg)
{
	switch (reg)
	{
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
	{
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), M(&g_dsp._r.sr));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, R(EAX), MDisp(R11,STRUCT_OFFSET(g_dsp._r, sr)));
#endif
		TEST(16, R(EAX), Imm16(SR_40_MODE_BIT));
		FixupBranch not_40bit = J_CC(CC_Z);
		//if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
		//{
		// Sign extend into whole accum.
		//u16 val = g_dsp.r[reg];
#ifdef _M_IX86 // All32
		MOVSX(32, 16, EAX, M(&g_dsp._r.ac[reg-DSP_REG_ACM0].m));
#else
		MOVSX(64, 16, EAX, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACM0].m)));
#endif
		SHR(32,R(EAX),Imm8(16));
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
#ifdef _M_IX86 // All32
		MOV(16,M(&g_dsp._r.ac[reg - DSP_REG_ACM0].h),
		    R(EAX));
		MOV(16,M(&g_dsp._r.ac[reg - DSP_REG_ACM0].l),
		    Imm16(0));
#else
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACM0].h)), R(EAX));
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACM0].l)), Imm16(0));
#endif
		//}
		SetJumpTarget(not_40bit);
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
#ifdef _M_IX86 // All32
		MOV(16, R(EAX), M(&g_dsp._r.sr));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, R(EAX), MDisp(R11,STRUCT_OFFSET(g_dsp._r, sr)));
#endif
		TEST(16, R(EAX), Imm16(SR_40_MODE_BIT));
		FixupBranch not_40bit = J_CC(CC_Z);
		//if (g_dsp.r[DSP_REG_SR] & SR_40_MODE_BIT)
		//{
		// Sign extend into whole accum.
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACH0] = (val & 0x8000) ? 0xFFFF : 0x0000;
		//g_dsp.r[reg - DSP_REG_ACM0 + DSP_REG_ACL0] = 0;
#ifdef _M_IX86 // All32
		MOV(16,M(&g_dsp._r.ac[reg - DSP_REG_ACM0].h),
		    Imm16((val & 0x8000)?0xffff:0x0000));
		MOV(16,M(&g_dsp._r.ac[reg - DSP_REG_ACM0].l),
		    Imm16(0));
#else
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACM0].h)),
		    Imm16((val & 0x8000)?0xffff:0x0000));
		MOV(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, ac[reg-DSP_REG_ACM0].l)),
		    Imm16(0));
#endif
		//}
		SetJumpTarget(not_40bit);
	}
	}
}

void DSPEmitter::dsp_op_read_reg(int reg, Gen::X64Reg host_dreg)
{
	switch (reg & 0x1f) {
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return dsp_reg_load_stack(reg - DSP_REG_ST0, host_dreg);
	default:
	{
		u16 *regp = reg_ptr(reg);
		//return g_dsp.r[reg];
#ifdef _M_IX86 // All32
		MOV(16, R(host_dreg), M(regp));
#else
		MOV(64, R(R11), ImmPtr(&g_dsp._r));
		MOV(16, R(host_dreg), MDisp(R11,PtrOffset(regp,&g_dsp._r)));
#endif
	}
	}
}

// MRR $D, $S
// 0001 11dd ddds ssss
// Move value from register $S to register $D.
// FIXME: Perform additional operation depending on destination register.
void DSPEmitter::mrr(const UDSPInstruction opc)
{
	u8 sreg = opc & 0x1f;
	u8 dreg = (opc >> 5) & 0x1f;

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
// FIXME: Perform additional operation depending on destination register.
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
	zeroWriteBackLog(opc);
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
	increase_addr_reg(opc & 0x3);
}

//----


void DSPEmitter::setCompileSR(u16 bit) {
	
	//	g_dsp.r[DSP_REG_SR] |= bit
#ifdef _M_IX86 // All32
	OR(16, M(&g_dsp._r.sr), Imm16(bit));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	OR(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, sr)), Imm16(bit));
#endif

	compileSR |= bit;
}

void DSPEmitter::clrCompileSR(u16 bit) {
	
	//	g_dsp.r[DSP_REG_SR] &= bit
#ifdef _M_IX86 // All32
	AND(16, M(&g_dsp._r.sr), Imm16(~bit));
#else
	MOV(64, R(R11), ImmPtr(&g_dsp._r));
	AND(16, MDisp(R11,STRUCT_OFFSET(g_dsp._r, sr)), Imm16(~bit));
#endif

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
	zeroWriteBackLog(opc);
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

