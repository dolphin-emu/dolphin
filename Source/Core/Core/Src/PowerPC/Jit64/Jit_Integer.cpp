// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../../Core.h" // include "Common.h", "CoreParameter.h", SCoreStartupParameter
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitRegCache.h"
#include "JitAsm.h"

// #define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

namespace Jit64
{
	// Assumes that the flags were just set through an addition.
	void GenerateCarry(X64Reg temp_reg) {
		SETcc(CC_C, R(temp_reg));
		AND(32, M(&XER), Imm32(~(1 << 29)));
		SHL(32, R(temp_reg), Imm8(29));
		OR(32, M(&XER), R(temp_reg));
	}

	typedef u32 (*Operation)(u32 a, u32 b);
	u32 Add(u32 a, u32 b) {return a + b;}
	u32 Or (u32 a, u32 b) {return a | b;}
	u32 And(u32 a, u32 b) {return a & b;}
	u32 Xor(u32 a, u32 b) {return a ^ b;}

	void regimmop(int d, int a, bool binary, u32 value, Operation doop, void(*op)(int, const OpArg&, const OpArg&), bool Rc = false, bool carry = false)
	{
		gpr.Lock(d, a);
		if (a || binary || carry)  // yeh nasty special case addic
		{
			if (a == d)
			{
				if (gpr.R(d).IsImm() && !carry)
				{
					gpr.SetImmediate32(d, doop((u32)gpr.R(d).offset, value));
				}
				else
				{
					if (gpr.R(d).IsImm())
						gpr.LoadToX64(d, false);
					op(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
					if (carry)
						GenerateCarry(EAX);
				}
			}
			else
			{
				gpr.LoadToX64(d, false);
				MOV(32, gpr.R(d), gpr.R(a));
				op(32, gpr.R(d), Imm32(value)); //m_GPR[d] = m_GPR[_inst.RA] + _inst.SIMM_16;
				if (carry)
					GenerateCarry(EAX);
			}
		}
		else if (doop == Add)
		{
			// a == 0, which for these instructions imply value = 0
			gpr.SetImmediate32(d, value);
		}
		else
		{
			_assert_msg_(DYNA_REC, 0, "WTF regimmop");
		}
		if (Rc)
		{
			// Todo - special case immediates.
			MOV(32, R(EAX), gpr.R(d));
			CALL((u8*)Asm::computeRc);
		}
		gpr.UnlockAll();
	}

	void reg_imm(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int d = inst.RD, a = inst.RA, s = inst.RS;
		switch (inst.OPCD)
		{
		case 14:  // addi
			// occasionally used as MOV - emulate, with immediate propagation
			if (gpr.R(a).IsImm() && d != a && a != 0) {
				gpr.SetImmediate32(d, (u32)gpr.R(a).offset + (u32)(s32)(s16)inst.SIMM_16);
			} else if (inst.SIMM_16 == 0 && d != a && a != 0) {
				gpr.Lock(a, d);
				gpr.LoadToX64(d, false, true);
				MOV(32, gpr.R(d), gpr.R(a));
				gpr.UnlockAll();
			} else {
				regimmop(d, a, false, (u32)(s32)inst.SIMM_16,  Add, ADD); //addi
			}
			break;
		case 15: regimmop(d, a, false, (u32)inst.SIMM_16 << 16, Add, ADD); break; //addis
		case 24: 
			if (a == 0 && s == 0 && inst.UIMM == 0 && !inst.Rc)  //check for nop
				{NOP(); return;} //make the nop visible in the generated code. not much use but interesting if we see one.
			regimmop(a, s, true, inst.UIMM, Or, OR); 
			break; //ori
		case 25: regimmop(a, s, true, inst.UIMM << 16, Or,  OR, false); break;//oris
		case 28: regimmop(a, s, true, inst.UIMM,       And, AND, true); break;
		case 29: regimmop(a, s, true, inst.UIMM << 16, And, AND, true); break;
		case 26: regimmop(a, s, true, inst.UIMM,       Xor, XOR, false); break; //xori
		case 27: regimmop(a, s, true, inst.UIMM << 16, Xor, XOR, false); break; //xoris
		case 12: //regimmop(d, a, false, (u32)(s32)inst.SIMM_16, Add, ADD, false, true); //addic
		case 13: //regimmop(d, a, true, (u32)(s32)inst.SIMM_16, Add, ADD, true, true); //addic_rc
		default:
			Default(inst);
			break;
		}
	}

	// unsigned
	void cmpli(UGeckoInstruction inst)
	{	
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		u32 uimm = inst.UIMM;
		int crf = inst.CRFD;
		int shift = crf * 4;
		gpr.KillImmediate(a); // todo, optimize instead, but unlikely to make a difference
		AND(32, M(&CR), Imm32(~(0xF0000000 >> (crf*4))));
		CMP(32, gpr.R(a), Imm32(uimm));
		FixupBranch pLesser  = J_CC(CC_B);
		FixupBranch pGreater = J_CC(CC_A);
		
		MOV(32, R(EAX), Imm32(0x20000000 >> shift)); // _x86Reg == 0
		FixupBranch continue1 = J();
		
		SetJumpTarget(pGreater);
		MOV(32, R(EAX), Imm32(0x40000000 >> shift)); // _x86Reg > 0
		FixupBranch continue2 = J();
		
		SetJumpTarget(pLesser);
		MOV(32, R(EAX), Imm32(0x80000000 >> shift));// _x86Reg < 0
		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
		OR(32, M(&CR), R(EAX));
	}

	// signed
	void cmpi(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		s32 simm = (s32)(s16)inst.UIMM;
		int crf = inst.CRFD;
		int shift = crf * 4;
		gpr.KillImmediate(a); // todo, optimize instead, but unlikely to make a difference
		AND(32, M(&CR), Imm32(~(0xF0000000 >> (crf*4))));
		CMP(32, gpr.R(a), Imm32(simm));
		FixupBranch pLesser  = J_CC(CC_L);
		FixupBranch pGreater = J_CC(CC_G);
		// _x86Reg == 0
		MOV(32, R(EAX), Imm32(0x20000000 >> shift));
		FixupBranch continue1 = J();
		// _x86Reg > 0
		SetJumpTarget(pGreater);
		MOV(32, R(EAX), Imm32(0x40000000 >> shift));
		FixupBranch continue2 = J();
		// _x86Reg < 0
		SetJumpTarget(pLesser);
		MOV(32, R(EAX), Imm32(0x80000000 >> shift));
		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
		OR(32, M(&CR), R(EAX));	
	}

	// signed
	void cmp(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int b = inst.RB;
		int crf = inst.CRFD;
		int shift = crf * 4;
		gpr.Lock(a, b);
		gpr.LoadToX64(a, true, false);
		AND(32, M(&CR), Imm32(~(0xF0000000 >> (crf*4))));
		CMP(32, gpr.R(a), gpr.R(b));
		FixupBranch pLesser  = J_CC(CC_L);
		FixupBranch pGreater = J_CC(CC_G);
		// _x86Reg == 0
		MOV(32, R(EAX), Imm32(0x20000000 >> shift));
		FixupBranch continue1 = J();
		// _x86Reg > 0
		SetJumpTarget(pGreater);
		MOV(32, R(EAX), Imm32(0x40000000 >> shift));
		FixupBranch continue2 = J();
		// _x86Reg < 0
		SetJumpTarget(pLesser);
		MOV(32, R(EAX), Imm32(0x80000000 >> shift));
		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
		OR(32, M(&CR), R(EAX));	
		gpr.UnlockAll();
	}

	// unsigned
	void cmpl(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int b = inst.RB;
		int crf = inst.CRFD;
		int shift = crf * 4;
		gpr.Lock(a, b);
		gpr.LoadToX64(a, true, false);
		AND(32, M(&CR), Imm32(~(0xF0000000 >> (crf*4))));
		CMP(32, gpr.R(a), gpr.R(b));
		FixupBranch pLesser  = J_CC(CC_B);
		FixupBranch pGreater = J_CC(CC_A);
		// _x86Reg == 0
		MOV(32, R(EAX), Imm32(0x20000000 >> shift));
		FixupBranch continue1 = J();
		// _x86Reg > 0
		SetJumpTarget(pGreater);
		MOV(32, R(EAX), Imm32(0x40000000 >> shift));
		FixupBranch continue2 = J();
		// _x86Reg < 0
		SetJumpTarget(pLesser);
		MOV(32, R(EAX), Imm32(0x80000000 >> shift));
		SetJumpTarget(continue1);
		SetJumpTarget(continue2);
		OR(32, M(&CR), R(EAX));	
		gpr.UnlockAll();
	}
	
	void orx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		int b = inst.RB;

		if (s == b && s != a)
		{
			gpr.Lock(a,s);
			gpr.LoadToX64(a, false);
			MOV(32, gpr.R(a), gpr.R(s));
			gpr.UnlockAll();
		}
		else
		{
			gpr.Lock(a, s, b);
			gpr.LoadToX64(a, (a == s || a == b), true);
			if (a == s) 
				OR(32, gpr.R(a), gpr.R(b));
			else if (a == b)
				OR(32, gpr.R(a), gpr.R(s));
			else {
				MOV(32, gpr.R(a), gpr.R(b));
				OR(32, gpr.R(a), gpr.R(s));
			}
			gpr.UnlockAll();
		}

		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	
	// m_GPR[_inst.RA] = m_GPR[_inst.RS] ^ m_GPR[_inst.RB];
	void xorx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		int b = inst.RB;

		if (s == b) {
			gpr.SetImmediate32(a, 0);
		}
		else
		{
			gpr.LoadToX64(a, a == s || a == b, true);
			gpr.Lock(a, s, b);
			MOV(32, R(EAX), gpr.R(s));
			XOR(32, R(EAX), gpr.R(b));
			MOV(32, gpr.R(a), R(EAX));
			gpr.UnlockAll();
		}

		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void andx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, s = inst.RS, b = inst.RB;
		if (a != s && a != b) {
			gpr.LoadToX64(a, false, true);
		} else {
			gpr.LoadToX64(a, true, true);
		}
		gpr.Lock(a, s, b);
		MOV(32, R(EAX), gpr.R(s));
		AND(32, R(EAX), gpr.R(b));
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();

		if (inst.Rc) {
			// result is already in eax
			CALL((u8*)Asm::computeRc);
		}
	}

	void extsbx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA,
			s = inst.RS;
		gpr.LoadToX64(a, a == s, true);
		// Always force moving to EAX because it isn't possible
		// to refer to the lowest byte of some registers, at least in
		// 32-bit mode.
		MOV(32, R(EAX), gpr.R(s));
		MOVSX(32, 8, gpr.RX(a), R(AL)); // watch out for ah and friends
		if (inst.Rc) {
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void extshx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, s = inst.RS;
		gpr.KillImmediate(s);
		gpr.LoadToX64(a, a == s, true);
		// This looks a little dangerous, but it's safe because
		// every 32-bit register has a 16-bit half at the same index
		// as the 32-bit register.
		MOVSX(32, 16, gpr.RX(a), gpr.R(s));
		if (inst.Rc) {
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void subfic(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, d = inst.RD;
		gpr.FlushLockX(ECX);
		gpr.Lock(a, d);
		gpr.LoadToX64(d, a == d, true);
		int imm = inst.SIMM_16;
		MOV(32, R(EAX), gpr.R(a));
		NOT(32, R(EAX));
		ADD(32, R(EAX), Imm32(imm + 1));
		MOV(32, gpr.R(d), R(EAX));
		GenerateCarry(ECX);
		gpr.UnlockAll();
		gpr.UnlockAllX();
		// This instruction has no RC flag
	}

	void subfcx(UGeckoInstruction inst) 
	{
		INSTRUCTION_START;
		Default(inst);
		return;
		/*
		u32 a = m_GPR[_inst.RA];
		u32 b = m_GPR[_inst.RB];
		m_GPR[_inst.RD] = b - a;
		SetCarry(a == 0 || Helper_Carry(b, 0-a));

		if (_inst.OE) PanicAlert("OE: subfcx");
		if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
		*/
	}

	void subfex(UGeckoInstruction inst) 
	{
		INSTRUCTION_START;
		Default(inst);
		return;
		/*
		u32 a = m_GPR[_inst.RA];
		u32 b = m_GPR[_inst.RB];
		int carry = GetCarry();
		m_GPR[_inst.RD] = (~a) + b + carry;
		SetCarry(Helper_Carry(~a, b) || Helper_Carry((~a) + b, carry));

		if (_inst.OE) PanicAlert("OE: subfcx");
		if (_inst.Rc) Helper_UpdateCR0(m_GPR[_inst.RD]);
		*/
	}

	void subfx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.Lock(a, b, d);
		if (d != a && d != b) {
			gpr.LoadToX64(d, false, true);
		} else {
			gpr.LoadToX64(d, true, true);
		}
		MOV(32, R(EAX), gpr.R(b));
		SUB(32, R(EAX), gpr.R(a));
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		if (inst.OE) PanicAlert("OE: subfx");
		if (inst.Rc) {
			// result is already in eax
			CALL((u8*)Asm::computeRc);
		}
	}

	void mulli(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, d = inst.RD;
 		gpr.Lock(a, d);
		gpr.LoadToX64(d, (d == a), true);
		gpr.KillImmediate(a);
		IMUL(32, gpr.RX(d), gpr.R(a), Imm32((u32)(s32)inst.SIMM_16));
 		gpr.UnlockAll();
	}

	void mullwx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
 		gpr.Lock(a, b, d);
		gpr.LoadToX64(d, (d == a || d == b), true);
		if (d == a) {
			IMUL(32, gpr.RX(d), gpr.R(b));
		} else if (d == b) {
			IMUL(32, gpr.RX(d), gpr.R(a));
		} else {
			MOV(32, gpr.R(d), gpr.R(b));
			IMUL(32, gpr.RX(d), gpr.R(a));
		}
		gpr.UnlockAll();
		if (inst.Rc) {
			MOV(32, R(EAX), gpr.R(d));
			CALL((u8*)Asm::computeRc);
		}
	}

	void mulhwux(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.FlushLockX(EDX);
		gpr.Lock(a, b, d);
		if (d != a && d != b) {
			gpr.LoadToX64(d, false, true);
		} else {
			gpr.LoadToX64(d, true, true);
		}
		if (gpr.RX(d) == EDX)
			PanicAlert("mulhwux : WTF");
		MOV(32, R(EAX), gpr.R(a));
		gpr.KillImmediate(b);
		MUL(32, gpr.R(b));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc) {
			MOV(32, R(EAX), R(EDX));
			MOV(32, gpr.R(d), R(EDX));
			// result is already in eax
			CALL((u8*)Asm::computeRc);
		} else {
			MOV(32, gpr.R(d), R(EDX));
		}
	}

	// skipped some of the special handling in here - if we get crashes, let the interpreter handle this op
	void divwux(UGeckoInstruction inst) {
		Default(inst); return;

		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.FlushLockX(EDX);
		gpr.Lock(a, b, d);
		if (d != a && d != b) {
			gpr.LoadToX64(d, false, true);
		} else {
			gpr.LoadToX64(d, true, true);
		}
		MOV(32, R(EAX), gpr.R(a));
		XOR(32, R(EDX), R(EDX));
		gpr.KillImmediate(b);
		DIV(32, gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc) {
			CALL((u8*)Asm::computeRc);
		}
	}

	u32 Helper_Mask(u8 mb, u8 me)
	{
		return (((mb > me) ?
			~(((u32)-1 >> mb) ^ ((me >= 31) ? 0 : (u32) -1 >> (me + 1)))
			:
			(((u32)-1 >> mb) ^ ((me >= 31) ? 0 : (u32) -1 >> (me + 1))))
			);
	}

	void addx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
		_assert_msg_(DYNA_REC, !inst.OE, "Add - OE enabled :(");
		
		if (a != d && b != d && a != b)
		{
			gpr.Lock(a, b, d);
			gpr.LoadToX64(d, false);
			if (gpr.R(a).IsSimpleReg() && gpr.R(b).IsSimpleReg()) {
				LEA(32, gpr.RX(d), MComplex(gpr.RX(a), gpr.RX(b), 1, 0));
			} else {
				MOV(32, gpr.R(d), gpr.R(a));
				ADD(32, gpr.R(d), gpr.R(b));
			}
			if (inst.Rc)
			{
				MOV(32, R(EAX), gpr.R(d));
				CALL((u8*)Asm::computeRc);
			}
			gpr.UnlockAll();
		}
		else if (d == a && d != b)
		{
			gpr.Lock(b, d);
			gpr.LoadToX64(d, true);
			ADD(32, gpr.R(d), gpr.R(b));
			if (inst.Rc)
			{
				MOV(32, R(EAX), gpr.R(d));
				CALL((u8*)Asm::computeRc);
			}
			gpr.UnlockAll();
		}
		else if (d == b && d != a)
		{
			gpr.Lock(a, d);
			gpr.LoadToX64(d, true);
			ADD(32, gpr.R(d), gpr.R(a));
			if (inst.Rc)
			{
				MOV(32, R(EAX), gpr.R(d));
				CALL((u8*)Asm::computeRc);
			}
			gpr.UnlockAll();
		}
		else
		{
			Default(inst);	return;
		}
	}

	// This can be optimized
	void addex(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, d);
		if (d != a && d != b)
			gpr.LoadToX64(d, false);
		else
			gpr.LoadToX64(d, true);
		MOV(32, R(EAX), M(&XER));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, R(EAX), gpr.R(a));
		ADC(32, R(EAX), gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		GenerateCarry(ECX);
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc)
		{
			CALL((u8*)Asm::computeRc);
		}
	}

	void rlwinmx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		if (gpr.R(a).IsImm() || gpr.R(s).IsImm())
		{
			if (gpr.R(s).IsImm())
			{
				if (gpr.R(s).offset == 0 && !inst.Rc) {
					// This is pretty common for some reason
					gpr.LoadToX64(a, false);
					XOR(32, gpr.R(a), gpr.R(a));
					return;
				}
				// This might also be worth doing.
			}
			Default(inst);
			return;
		}

		if (a != s)
		{
			gpr.Lock(a, s);
			gpr.LoadToX64(a, false);
			MOV(32, gpr.R(a), gpr.R(s));
		}

		if (inst.MB == 0 && inst.ME==31-inst.SH)
		{
			SHL(32, gpr.R(a), Imm8(inst.SH));
		}
		else if (inst.ME == 31 && inst.MB == 32 - inst.SH)
		{
			SHR(32, gpr.R(a), Imm8(inst.MB));
		}
		else
		{
			bool written = false;
			if (inst.SH != 0)
			{
				ROL(32, gpr.R(a), Imm8(inst.SH));
				written = true;
			}
			if (!(inst.MB==0 && inst.ME==31)) 
			{
				written = true;
				AND(32, gpr.R(a), Imm32(Helper_Mask(inst.MB, inst.ME)));
			}
			_assert_msg_(DYNA_REC, written, "W T F!!!");
		}
		gpr.UnlockAll();

		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}


	void rlwimix(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		if (gpr.R(a).IsImm() || gpr.R(s).IsImm())
		{
			Default(inst);
			return;
		}

		if (a != s)
		{
			gpr.Lock(a, s);
			gpr.LoadToX64(a, true);
		}
				
		u32 mask = Helper_Mask(inst.MB, inst.ME);
		MOV(32, R(EAX), gpr.R(s));
		AND(32, gpr.R(a), Imm32(~mask));
		if (inst.SH)
			ROL(32, R(EAX), Imm8(inst.SH));
		AND(32, R(EAX), Imm32(mask));
		OR(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void rlwnmx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, s = inst.RS;
		if (gpr.R(a).IsImm())
		{
			Default(inst);
			return;
		}

		u32 mask = Helper_Mask(inst.MB, inst.ME);
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		MOV(32, R(EAX), gpr.R(s));
		MOV(32, R(ECX), gpr.R(b));
		AND(32, R(ECX), Imm32(0x1f));
		ROL(32, R(EAX), R(ECX));
		AND(32, R(EAX), Imm32(mask));
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void negx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int d = inst.RD;
		gpr.Lock(a, d);
		gpr.LoadToX64(d, a == d, true);
		if (a != d)
			MOV(32, gpr.R(d), gpr.R(a));
		NEG(32, gpr.R(d));
		gpr.UnlockAll();
		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void srwx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int b = inst.RB;
		int s = inst.RS;
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.LoadToX64(a, a == s || a == b || s == b, true);
		MOV(32, R(ECX), gpr.R(b));
		XOR(32, R(EAX), R(EAX));
		TEST(32, R(ECX), Imm32(32));
		FixupBranch branch = J_CC(CC_NZ);
		MOV(32, R(EAX), gpr.R(s));
		SHR(32, R(EAX), R(ECX));
		SetJumpTarget(branch);
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc) 
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void slwx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int b = inst.RB;
		int s = inst.RS;
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, s);
		gpr.LoadToX64(a, a == s || a == b || s == b, true);
		MOV(32, R(ECX), gpr.R(b));
		XOR(32, R(EAX), R(EAX));
		TEST(32, R(ECX), Imm32(32));
		FixupBranch branch = J_CC(CC_NZ);
		MOV(32, R(EAX), gpr.R(s));
		SHL(32, R(EAX), R(ECX));
		SetJumpTarget(branch);
		MOV(32, gpr.R(a), R(EAX));
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc) 
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void srawx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int b = inst.RB;
		int s = inst.RS;
		gpr.Lock(a, s);
		gpr.FlushLockX(ECX);
		gpr.LoadToX64(a, a == s || a == b, true);
		MOV(32, R(ECX), gpr.R(b));
		TEST(32, R(ECX), Imm32(32));
		FixupBranch topBitSet = J_CC(CC_NZ);
		if (a != s)
			MOV(32, gpr.R(a), gpr.R(s));
		MOV(32, R(EAX), Imm32(1));
		SHL(32, R(EAX), R(ECX));
		ADD(32, R(EAX), Imm32(0x7FFFFFFF));
		AND(32, R(EAX), gpr.R(a));
		ADD(32, R(EAX), Imm32(-1));
		CMP(32, R(EAX), Imm32(-1));
		SETcc(CC_L, R(EAX));
		SAR(32, gpr.R(a), R(ECX));
		AND(32, M(&XER), Imm32(~(1 << 29)));
		SHL(32, R(EAX), Imm8(29));
		OR(32, M(&XER), R(EAX));
		FixupBranch end = J();
		SetJumpTarget(topBitSet);
		MOV(32, R(EAX), gpr.R(s));
		SAR(32, R(EAX), Imm8(31));
		MOV(32, gpr.R(a), R(EAX));
		AND(32, M(&XER), Imm32(~(1 << 29)));
		AND(32, R(EAX), Imm32(1<<29));
		OR(32, M(&XER), R(EAX));
		SetJumpTarget(end);
		gpr.UnlockAll();
		gpr.UnlockAllX();

		if (inst.Rc) {
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	void srawix(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		int amount = inst.SH;
		if (amount != 0)
		{
			gpr.Lock(a, s);
			gpr.LoadToX64(a, a == s, true);
			MOV(32, R(EAX), gpr.R(s));
			MOV(32, gpr.R(a), R(EAX));
			SAR(32, gpr.R(a), Imm8(amount));
			CMP(32, R(EAX), Imm8(0));
			FixupBranch nocarry1 = J_CC(CC_GE);
			TEST(32, R(EAX), Imm32((u32)0xFFFFFFFF >> (32 - amount))); // were any 1s shifted out?
			FixupBranch nocarry2 = J_CC(CC_Z);
			OR(32, M(&XER), Imm32(XER_CA_MASK)); //XER.CA = 1
			FixupBranch carry = J(false);
			SetJumpTarget(nocarry1);
			SetJumpTarget(nocarry2);
			AND(32, M(&XER), Imm32(~XER_CA_MASK)); //XER.CA = 0
			SetJumpTarget(carry);
			gpr.UnlockAll();
		}
		else
		{
			Default(inst); return;
			gpr.Lock(a, s);
			AND(32, M(&XER), Imm32(~XER_CA_MASK)); //XER.CA = 0
			gpr.LoadToX64(a, a == s, true);
			if (a != s)
				MOV(32, gpr.R(a), gpr.R(s));
			gpr.UnlockAll();
		}

		if (inst.Rc) {
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
		}
	}

	// count leading zeroes
	void cntlzwx(UGeckoInstruction inst)
	{
#ifdef JIT_OFF_OPTIONS
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger
#endif
		INSTRUCTION_START;
		int a = inst.RA;
		int s = inst.RS;
		if (gpr.R(a).IsImm() || gpr.R(s).IsImm() || s == a)
		{
			Default(inst);
			return;
		}
		gpr.Lock(a,s);
		gpr.LoadToX64(a,false);
		BSR(32, gpr.R(a).GetSimpleReg(), gpr.R(s));
		FixupBranch gotone = J_CC(CC_NZ);
		MOV(32, gpr.R(a), Imm32(63));
		SetJumpTarget(gotone);
		XOR(32, gpr.R(a), Imm8(0x1f));  // flip order
		gpr.UnlockAll();

		if (inst.Rc)
		{
			MOV(32, R(EAX), gpr.R(a));
			CALL((u8*)Asm::computeRc);
			// TODO: Check PPC manual too
		}
	}

}
