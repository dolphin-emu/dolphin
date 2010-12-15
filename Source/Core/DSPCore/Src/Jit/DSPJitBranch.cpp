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
#include "../DSPStacks.h"
#include "x64Emitter.h"
#include "ABI.h"

using namespace Gen;

const int GetCodeSize(void(*jitCode)(const UDSPInstruction, DSPEmitter&), const UDSPInstruction opc, DSPEmitter &emitter)
{
	u16 pc = g_dsp.pc;
	const u8* ptr = emitter.GetCodePtr();
	jitCode(opc, emitter);
	//emitter.JMP(emitter.GetCodePtr());
	int size = (int)(emitter.GetCodePtr() - ptr);
	emitter.SetCodePtr((u8*)ptr);
	g_dsp.pc = pc;
	return size;
}

const u8* CheckCondition(DSPEmitter& emitter, u8 cond, u8 skipCodeSize)
{
	if (cond == 0xf) // Always true.
		return NULL;
	//emitter.INT3();
	FixupBranch skipCode2;
	emitter.MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_SR]));
	switch(cond)
	{
	case 0x0: // GE - Greater Equal
	case 0x1: // L - Less
	case 0x2: // G - Greater
	case 0x3: // LE - Less Equal
		emitter.MOV(16, R(EDX), R(EAX));
		emitter.SHR(16, R(EDX), Imm8(3)); //SR_SIGN flag
		emitter.NOT(16, R(EDX));
		emitter.SHR(16, R(EAX), Imm8(1)); //SR_OVERFLOW flag
		emitter.NOT(16, R(EAX));
		emitter.XOR(16, R(EAX), R(EDX));
		emitter.TEST(16, R(EAX), Imm16(1));
		if (cond < 0x2)
			break;
		
		//LE: problem in here, half the tests fail
		skipCode2 = emitter.J_CC(CC_NE);
		//skipCode2 = emitter.J_CC((CCFlags)(CC_NE - (cond & 1)));
		emitter.MOV(16, R(EAX), M(&g_dsp.r[DSP_REG_SR]));
		emitter.TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
		break;
	case 0x4: // NZ - Not Zero
	case 0x5: // Z - Zero 
		emitter.TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
		break;
	case 0x6: // NC - Not carry
	case 0x7: // C - Carry 
		emitter.TEST(16, R(EAX), Imm16(SR_CARRY));
		break;
	case 0x8: // ? - Not over s32
	case 0x9: // ? - Over s32
		emitter.TEST(16, R(EAX), Imm16(SR_OVER_S32));
		break;
	case 0xa: // ?
	case 0xb: // ?
	{
		//full of fail, both
		emitter.TEST(16, R(EAX), Imm16(SR_OVER_S32 | SR_TOP2BITS));
		FixupBranch skipArithZero = emitter.J_CC(CC_E);
		emitter.TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
		FixupBranch setZero = emitter.J_CC(CC_NE);

		emitter.MOV(16, R(EAX), Imm16(1));
		FixupBranch toEnd = emitter.J();

		emitter.SetJumpTarget(skipArithZero);
		emitter.SetJumpTarget(setZero);
		emitter.XOR(16, R(EAX), R(EAX));
		emitter.SetJumpTarget(toEnd);
		emitter.SETcc(CC_E, R(EAX));
		emitter.TEST(8, R(EAX), R(EAX));
		break;
		//emitter.TEST(16, R(EAX), Imm16(SR_OVER_S32 | SR_TOP2BITS));
		//skipCode2 = emitter.J_CC((CCFlags)(CC_E + (cond & 1)));
		//emitter.TEST(16, R(EAX), Imm16(SR_ARITH_ZERO));
		//break;
	}
	case 0xc: // LNZ  - Logic Not Zero
	case 0xd: // LZ - Logic Zero
		emitter.TEST(16, R(EAX), Imm16(SR_LOGIC_ZERO));
		break;
	case 0xe: // 0 - Overflow
		emitter.TEST(16, R(EAX), Imm16(SR_OVERFLOW));
		break;
	}
	FixupBranch skipCode = cond == 0xe ? emitter.J_CC(CC_E) : emitter.J_CC((CCFlags)(CC_NE - (cond & 1)));
	const u8* res = emitter.GetCodePtr();
	emitter.NOP(skipCodeSize);
	emitter.SetJumpTarget(skipCode);
	if ((cond | 1) == 0x3) // || (cond | 1) == 0xb)
		emitter.SetJumpTarget(skipCode2);
	return res;
}

template <void(*jitCode)(const UDSPInstruction, DSPEmitter&)>
void ReJitConditional(const UDSPInstruction opc, DSPEmitter& emitter)
{
	static const int codeSize = GetCodeSize(jitCode, opc, emitter);
	//emitter.INT3();
	const u8* codePtr = CheckCondition(emitter, opc & 0xf, codeSize);
	//const u8* afterSkip = emitter.GetCodePtr();
	if (codePtr != NULL) 
		emitter.SetCodePtr((u8*)codePtr);
	jitCode(opc, emitter);
	//if (codePtr != NULL) 
	//{
	//	emitter.JMP(afterSkip + 4 + sizeof(void*));
	//	emitter.SetCodePtr((u8*)afterSkip);
	//	emitter.ADD(16, M(&g_dsp.pc), Imm8(1)); //4 bytes + pointer
	//}
}

void r_jcc(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u16 dest = dsp_imem_read(emitter.compilePC + 1);
#ifdef _M_IX86 // All32
	emitter.MOV(16, M(&(g_dsp.pc)), Imm16(dest));

	// Jump directly to the called block if it has already been compiled.
	// TODO: Subtract cycles from cyclesLeft
	if (emitter.blockLinks[dest])
	{
		emitter.JMPptr(M(&emitter.blockLinks[dest]));
	}
#else
	emitter.MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	emitter.MOV(16, MDisp(RAX,0), Imm16(dest));

	// Jump directly to the next block if it has already been compiled.
	// TODO: Subtract cycles from cyclesLeft
	if (emitter.blockLinks[dest])
	{
		emitter.MOV(64, R(RAX), ImmPtr((void *)(emitter.blockLinks[dest])));
		emitter.JMPptr(R(RAX));
	}
#endif
}
// Generic jmp implementation
// Jcc addressA
// 0000 0010 1001 cccc
// aaaa aaaa aaaa aaaa
// Jump to addressA if condition cc has been met. Set program counter to
// address represented by value that follows this "jmp" instruction.
void DSPEmitter::jcc(const UDSPInstruction opc)
{
#ifdef _M_IX86 // All32
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
#else
	MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	MOV(16, MDisp(RAX,0), Imm16(compilePC + 1));
#endif
	ReJitConditional<r_jcc>(opc, *this);
}

void r_jmprcc(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u8 reg = (opc >> 5) & 0x7;
	//reg can only be DSP_REG_ARx and DSP_REG_IXx now,
	//no need to handle DSP_REG_STx.
#ifdef _M_IX86 // All32
	emitter.MOV(16, R(EAX), M(&g_dsp.r[reg]));
	emitter.MOV(16, M(&g_dsp.pc), R(EAX));
#else
	emitter.MOV(16, R(RSI), M(&g_dsp.r[reg]));
	emitter.MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	emitter.MOV(16, MDisp(RAX,0), R(RSI));
#endif
}
// Generic jmpr implementation
// JMPcc $R
// 0001 0111 rrr0 cccc
// Jump to address; set program counter to a value from register $R.
void DSPEmitter::jmprcc(const UDSPInstruction opc)
{
	ReJitConditional<r_jmprcc>(opc, *this);
}

void r_call(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u16 dest = dsp_imem_read(emitter.compilePC + 1);
	emitter.ABI_CallFunctionCC16((void *)dsp_reg_store_stack, DSP_STACK_C, emitter.compilePC + 2);
#ifdef _M_IX86 // All32
	emitter.MOV(16, M(&(g_dsp.pc)), Imm16(dest));

	// Jump directly to the called block if it has already been compiled.
	// TODO: Subtract cycles from cyclesLeft
	if (emitter.blockLinks[dest])
	{
		emitter.JMPptr(M(&emitter.blockLinks[dest]));
	}
#else
	emitter.MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	emitter.MOV(16, MDisp(RAX,0), Imm16(dest));

	// Jump directly to the called block if it has already been compiled.
	// TODO: Subtract cycles from cyclesLeft
	if (emitter.blockLinks[dest])
	{
		emitter.MOV(64, R(RAX), ImmPtr((void *)(emitter.blockLinks[dest])));
		emitter.JMPptr(R(RAX));
	}
#endif
}
// Generic call implementation
// CALLcc addressA
// 0000 0010 1011 cccc
// aaaa aaaa aaaa aaaa
// Call function if condition cc has been met. Push program counter of
// instruction following "call" to $st0. Set program counter to address
// represented by value that follows this "call" instruction.
void DSPEmitter::call(const UDSPInstruction opc)
{
#ifdef _M_IX86 // All32
	MOV(16, M(&(g_dsp.pc)), Imm16(compilePC + 1));
#else
	MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	MOV(16, MDisp(RAX,0), Imm16(compilePC + 1));
#endif
	ReJitConditional<r_call>(opc, *this);
}

void r_callr(const UDSPInstruction opc, DSPEmitter& emitter)
{
	u8 reg = (opc >> 5) & 0x7;
	emitter.ABI_CallFunctionCC16((void *)dsp_reg_store_stack, DSP_STACK_C, emitter.compilePC + 1);
#ifdef _M_IX86 // All32
	emitter.MOV(16, R(EAX), M(&g_dsp.r[reg]));
	emitter.MOV(16, M(&g_dsp.pc), R(EAX));
#else
	emitter.MOV(16, R(RSI), M(&g_dsp.r[reg]));
	emitter.MOV(64, R(RAX), ImmPtr(&(g_dsp.pc)));
	emitter.MOV(16, MDisp(RAX,0), R(RSI));
#endif
}
// Generic callr implementation
// CALLRcc $R
// 0001 0111 rrr1 cccc
// Call function if condition cc has been met. Push program counter of 
// instruction following "call" to call stack $st0. Set program counter to 
// register $R.
void DSPEmitter::callr(const UDSPInstruction opc)
{
	ReJitConditional<r_callr>(opc, *this);
}
