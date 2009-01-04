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

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

	static void ComputeRC(IREmitter::IRBuilder& ibuild,
			      IREmitter::InstLoc val) {
		IREmitter::InstLoc res =
			ibuild.EmitICmpCRSigned(val, ibuild.EmitIntConst(0));
		ibuild.EmitStoreCR(res, 0);
	}

	void Jit64::reg_imm(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		int d = inst.RD, a = inst.RA, s = inst.RS;
		IREmitter::InstLoc val, test, c;
		switch (inst.OPCD)
		{
		case 14: //addi
			val = ibuild.EmitIntConst(inst.SIMM_16);
			if (a)
				val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), val);
			ibuild.EmitStoreGReg(val, d);
			break;
		case 15: //addis
			val = ibuild.EmitIntConst(inst.SIMM_16 << 16);
			if (a)
				val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), val);
			ibuild.EmitStoreGReg(val, d);
			break;
		case 24: //ori
			val = ibuild.EmitIntConst(inst.UIMM);
			val = ibuild.EmitOr(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			break;
		case 25: //oris
			val = ibuild.EmitIntConst(inst.UIMM << 16);
			val = ibuild.EmitOr(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			break;
		case 28: //andi
			val = ibuild.EmitIntConst(inst.UIMM);
			val = ibuild.EmitAnd(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			ComputeRC(ibuild, val);
			break;
		case 29: //andis
			val = ibuild.EmitIntConst(inst.UIMM << 16);
			val = ibuild.EmitAnd(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			ComputeRC(ibuild, val);
			break;
		case 26: //xori
			val = ibuild.EmitIntConst(inst.UIMM);
			val = ibuild.EmitXor(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			break;
		case 27: //xoris
			val = ibuild.EmitIntConst(inst.UIMM << 16);
			val = ibuild.EmitXor(ibuild.EmitLoadGReg(s), val);
			ibuild.EmitStoreGReg(val, a);
			break;
		case 12: //addic
		case 13: //addic_rc
			c = ibuild.EmitIntConst(inst.SIMM_16);
			val = ibuild.EmitAdd(ibuild.EmitLoadGReg(a), c);
			ibuild.EmitStoreGReg(val, d);
			test = ibuild.EmitICmpUgt(c, val);
			ibuild.EmitStoreCarry(test);
			if (inst.OPCD == 13)
				ComputeRC(ibuild, val);
			break;
		default:
			Default(inst);
			break;
		}
	}

	void Jit64::cmpXX(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc lhs, rhs, res;
		lhs = ibuild.EmitLoadGReg(inst.RA);
		if (inst.OPCD == 31) {
			rhs = ibuild.EmitLoadGReg(inst.RB);
			if (inst.SUBOP10 == 32) {
				res = ibuild.EmitICmpCRUnsigned(lhs, rhs);
			} else {
				res = ibuild.EmitICmpCRSigned(lhs, rhs);
			}
		} else if (inst.OPCD == 10) {
			rhs = ibuild.EmitIntConst(inst.UIMM);
			res = ibuild.EmitICmpCRUnsigned(lhs, rhs);
		} else { // inst.OPCD == 11
			rhs = ibuild.EmitIntConst(inst.SIMM_16);
			res = ibuild.EmitICmpCRSigned(lhs, rhs);
		}
		
		ibuild.EmitStoreCR(res, inst.CRFD);
	}

	void Jit64::orx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitOr(ibuild.EmitLoadGReg(inst.RS), val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	
	// m_GPR[_inst.RA] = m_GPR[_inst.RS] ^ m_GPR[_inst.RB];
	void Jit64::xorx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitXor(ibuild.EmitLoadGReg(inst.RS), val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::andx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitAnd(ibuild.EmitLoadGReg(inst.RS), val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::extsbx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitSExt8(val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::extshx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitSExt16(val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::subfic(UGeckoInstruction inst)
	{
		Default(inst);
		return;
		// FIXME: Disabling until I figure out subfcx
		IREmitter::InstLoc val, test, c;
		c = ibuild.EmitIntConst(inst.SIMM_16);
		val = ibuild.EmitSub(c, ibuild.EmitLoadGReg(inst.RA));
		ibuild.EmitStoreGReg(val, inst.RD);
		test = ibuild.EmitICmpUgt(val, c);
		ibuild.EmitStoreCarry(test);
	}

	void Jit64::subfcx(UGeckoInstruction inst) 
	{
		Default(inst);
		return;
		// FIXME: Figure out what the heck is going wrong here...
		if (inst.OE) PanicAlert("OE: subfcx");
		IREmitter::InstLoc val, test, lhs, rhs;
		lhs = ibuild.EmitLoadGReg(inst.RB);
		rhs = ibuild.EmitLoadGReg(inst.RA);
		val = ibuild.EmitSub(lhs, rhs);
		ibuild.EmitStoreGReg(val, inst.RD);
		test = ibuild.EmitICmpUgt(rhs, lhs);
		ibuild.EmitStoreCarry(test);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::subfex(UGeckoInstruction inst) 
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

	void Jit64::subfx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		if (inst.OE) PanicAlert("OE: subfx");
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitSub(val, ibuild.EmitLoadGReg(inst.RA));
		ibuild.EmitStoreGReg(val, inst.RD);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::mulli(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RA);
		val = ibuild.EmitMul(val, ibuild.EmitIntConst(inst.SIMM_16));
		ibuild.EmitStoreGReg(val, inst.RD);
	}

	void Jit64::mullwx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitMul(ibuild.EmitLoadGReg(inst.RA), val);
		ibuild.EmitStoreGReg(val, inst.RD);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::mulhwux(UGeckoInstruction inst)
	{
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger

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
			CALL((u8*)asm_routines.computeRc);
		} else {
			MOV(32, gpr.R(d), R(EDX));
		}
	}

	// skipped some of the special handling in here - if we get crashes, let the interpreter handle this op
	void Jit64::divwux(UGeckoInstruction inst) {
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
			CALL((u8*)asm_routines.computeRc);
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

	void Jit64::addx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RB);
		val = ibuild.EmitAdd(ibuild.EmitLoadGReg(inst.RA), val);
		ibuild.EmitStoreGReg(val, inst.RD);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	// This can be optimized
	void Jit64::addex(UGeckoInstruction inst)
	{
		Default(inst); return;
		// USES_XER
		if(Core::g_CoreStartupParameter.bJITOff || Core::g_CoreStartupParameter.bJITIntegerOff)
			{Default(inst); return;} // turn off from debugger

		INSTRUCTION_START;
		int a = inst.RA, b = inst.RB, d = inst.RD;
		gpr.FlushLockX(ECX);
		gpr.Lock(a, b, d);
		if (d != a && d != b)
			gpr.LoadToX64(d, false);
		else
			gpr.LoadToX64(d, true);
		MOV(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_XER]));
		SHR(32, R(EAX), Imm8(30)); // shift the carry flag out into the x86 carry flag
		MOV(32, R(EAX), gpr.R(a));
		ADC(32, R(EAX), gpr.R(b));
		MOV(32, gpr.R(d), R(EAX));
		//GenerateCarry(ECX);
		gpr.UnlockAll();
		gpr.UnlockAllX();
		if (inst.Rc)
		{
			CALL((u8*)asm_routines.computeRc);
		}
	}

	void Jit64::rlwinmx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		unsigned mask = Helper_Mask(inst.MB, inst.ME);
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitRol(val, ibuild.EmitIntConst(inst.SH));
		val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}


	void Jit64::rlwimix(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		unsigned mask = Helper_Mask(inst.MB, inst.ME);
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitRol(val, ibuild.EmitIntConst(inst.SH));
		val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
		IREmitter::InstLoc ival = ibuild.EmitLoadGReg(inst.RA);
		ival = ibuild.EmitAnd(ival, ibuild.EmitIntConst(~mask));
		val = ibuild.EmitOr(ival, val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::rlwnmx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		unsigned mask = Helper_Mask(inst.MB, inst.ME);
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitRol(val, ibuild.EmitLoadGReg(inst.RB));
		val = ibuild.EmitAnd(val, ibuild.EmitIntConst(mask));
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::negx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RA);
		val = ibuild.EmitSub(ibuild.EmitIntConst(0), val);
		ibuild.EmitStoreGReg(val, inst.RD);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::srwx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS),
			           samt = ibuild.EmitLoadGReg(inst.RB),
			           corr;
		// FIXME: We can do better with a cmov
		// FIXME: We can do better on 64-bit
		val = ibuild.EmitShrl(val, samt);
		corr = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
		corr = ibuild.EmitSarl(corr, ibuild.EmitIntConst(31));
		corr = ibuild.EmitXor(corr, ibuild.EmitIntConst(-1));
		val = ibuild.EmitAnd(corr, val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::slwx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS),
			           samt = ibuild.EmitLoadGReg(inst.RB),
			           corr;
		// FIXME: We can do better with a cmov
		// FIXME: We can do better on 64-bit
		val = ibuild.EmitShl(val, samt);
		corr = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
		corr = ibuild.EmitSarl(corr, ibuild.EmitIntConst(31));
		corr = ibuild.EmitXor(corr, ibuild.EmitIntConst(-1));
		val = ibuild.EmitAnd(corr, val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	void Jit64::srawx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		// FIXME: We can do a lot better on 64-bit
		IREmitter::InstLoc val, samt, mask, mask2, test;
		val = ibuild.EmitLoadGReg(inst.RS);
		samt = ibuild.EmitLoadGReg(inst.RB);
		mask = ibuild.EmitIntConst(-1);
		val = ibuild.EmitSarl(val, samt);
		mask = ibuild.EmitShl(mask, samt);
		samt = ibuild.EmitShl(samt, ibuild.EmitIntConst(26));
		samt = ibuild.EmitSarl(samt, ibuild.EmitIntConst(31));
		samt = ibuild.EmitAnd(samt, ibuild.EmitIntConst(31));
		val = ibuild.EmitSarl(val, samt);
		ibuild.EmitStoreGReg(val, inst.RA);
		mask = ibuild.EmitShl(mask, samt);
		mask2 = ibuild.EmitAnd(mask, ibuild.EmitIntConst(0x7FFFFFFF));
		test = ibuild.EmitOr(val, mask2);
		test = ibuild.EmitICmpUgt(test, mask);
		ibuild.EmitStoreCarry(test);
	}

	void Jit64::srawix(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS), test;
		val = ibuild.EmitSarl(val, ibuild.EmitIntConst(inst.SH));
		ibuild.EmitStoreGReg(val, inst.RA);
		unsigned mask = -1u << inst.SH;
		test = ibuild.EmitOr(val, ibuild.EmitIntConst(mask & 0x7FFFFFFF));
		test = ibuild.EmitICmpUgt(test, ibuild.EmitIntConst(mask));
		
		ibuild.EmitStoreCarry(test);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}

	// count leading zeroes
	void Jit64::cntlzwx(UGeckoInstruction inst)
	{
		IREmitter::InstLoc val = ibuild.EmitLoadGReg(inst.RS);
		val = ibuild.EmitCntlzw(val);
		ibuild.EmitStoreGReg(val, inst.RA);
		if (inst.Rc)
			ComputeRC(ibuild, val);
	}
