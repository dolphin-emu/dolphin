// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "x64Emitter.h"

#include "Jit.h"
#include "JitRegCache.h"

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

	void Jit64::fp_arith_s(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(FloatingPoint)
		if (inst.Rc || (inst.SUBOP5 != 25 && inst.SUBOP5 != 20 && inst.SUBOP5 != 21)) {
			Default(inst); return;
		}
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
		switch (inst.SUBOP5)
		{
		case 25: //mul
			val = ibuild.EmitFDMul(val, ibuild.EmitLoadFReg(inst.FC));
			break;
		case 18: //div
		case 20: //sub
			val = ibuild.EmitFDSub(val, ibuild.EmitLoadFReg(inst.FB));
			break;
		case 21: //add
			val = ibuild.EmitFDAdd(val, ibuild.EmitLoadFReg(inst.FB));
			break;
		case 23: //sel
		case 24: //res
		default:
			_assert_msg_(DYNA_REC, 0, "fp_arith_s WTF!!!");
		}

		if (inst.OPCD == 59) {
			val = ibuild.EmitDoubleToSingle(val);
			val = ibuild.EmitDupSingleToMReg(val);
		} else {
			val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
		}
		ibuild.EmitStoreFReg(val, inst.FD);
	}

	void Jit64::fmaddXX(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(FloatingPoint)
		if (inst.Rc) {
			Default(inst); return;
		}

		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
		val = ibuild.EmitFDMul(val, ibuild.EmitLoadFReg(inst.FC));
		if (inst.SUBOP5 & 1)
			val = ibuild.EmitFDAdd(val, ibuild.EmitLoadFReg(inst.FB));
		else
			val = ibuild.EmitFDSub(val, ibuild.EmitLoadFReg(inst.FB));
		if (inst.SUBOP5 & 2)
			val = ibuild.EmitFDNeg(val);
		if (inst.OPCD == 59) {
			val = ibuild.EmitDoubleToSingle(val);
			val = ibuild.EmitDupSingleToMReg(val);
		} else {
			val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
		}
		ibuild.EmitStoreFReg(val, inst.FD);
	}
	
	void Jit64::fmrx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(FloatingPoint)
		if (inst.Rc) {
			Default(inst); return;
		}
		IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FB);
		val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
		ibuild.EmitStoreFReg(val, inst.FD);
	}

	void Jit64::fcmpx(UGeckoInstruction inst)
	{
		INSTRUCTION_START
		JITDISABLE(FloatingPoint)
		IREmitter::InstLoc lhs, rhs, res;
		lhs = ibuild.EmitLoadFReg(inst.FA);
		rhs = ibuild.EmitLoadFReg(inst.FB);
		res = ibuild.EmitFDCmpCR(lhs, rhs);
		ibuild.EmitStoreFPRF(res);
		ibuild.EmitStoreCR(res, inst.CRFD);
	}
