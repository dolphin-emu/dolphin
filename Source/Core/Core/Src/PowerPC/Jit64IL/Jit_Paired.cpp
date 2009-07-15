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
#include "../../HW/GPFifo.h"

#include "Jit.h"

void Jit64::ps_mr(UGeckoInstruction inst)
{
	Default(inst); return;
}

void Jit64::ps_sel(UGeckoInstruction inst)
{
	Default(inst); return;
}

void Jit64::ps_sign(UGeckoInstruction inst)
{
	Default(inst); return;
}

void Jit64::ps_rsqrte(UGeckoInstruction inst)
{
	Default(inst); return;
}

void Jit64::ps_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc || (inst.SUBOP5 != 21 && inst.SUBOP5 != 20 && inst.SUBOP5 != 25)) {
		Default(inst); return;
	}
	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), rhs;
	if (inst.SUBOP5 == 25)
		rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
	else
		rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
	val = ibuild.EmitCompactMRegToPacked(val);
	
	switch (inst.SUBOP5)
	{
	case 20:
		val = ibuild.EmitFPSub(val, rhs);
		break;
	case 21:
		val = ibuild.EmitFPAdd(val, rhs);
		break;
	case 25:
		val = ibuild.EmitFPMul(val, rhs);
	}
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}

void Jit64::ps_sum(UGeckoInstruction inst)
{
	// FIXME: This operation strikes me as a bit strange...
	// perhaps we can optimize it depending on the users?
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc || inst.SUBOP5 != 10) {
		Default(inst); return;
	}
	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), temp;
	val = ibuild.EmitCompactMRegToPacked(val);
	val = ibuild.EmitFPDup0(val);
	temp = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
	val = ibuild.EmitFPAdd(val, temp);
	temp = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
	val = ibuild.EmitFPMerge11(val, temp);
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}


void Jit64::ps_muls(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA),
	                   rhs = ibuild.EmitLoadFReg(inst.FC);

	val = ibuild.EmitCompactMRegToPacked(val);
	rhs = ibuild.EmitCompactMRegToPacked(rhs);

	if (inst.SUBOP5 == 12)
		rhs = ibuild.EmitFPDup0(rhs);
	else
		rhs = ibuild.EmitFPDup1(rhs);

	val = ibuild.EmitFPMul(val, rhs);
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}


//TODO: find easy cases and optimize them, do a breakout like ps_arith
void Jit64::ps_mergeXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}

	IREmitter::InstLoc val = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FA)),
			   rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));

	switch (inst.SUBOP10)
	{
	case 528: 
		val = ibuild.EmitFPMerge00(val, rhs);
		break; //00
	case 560:
		val = ibuild.EmitFPMerge01(val, rhs);
		break; //01
	case 592:
		val = ibuild.EmitFPMerge10(val, rhs);
		break; //10
	case 624:
		val = ibuild.EmitFPMerge11(val, rhs);
		break; //11
	default:
		_assert_msg_(DYNA_REC, 0, "ps_merge - invalid op");
	}
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}


void Jit64::ps_maddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	if (inst.Rc) {
		Default(inst); return;
	}
	
	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), op2, op3;
	val = ibuild.EmitCompactMRegToPacked(val);
	switch (inst.SUBOP5)
	{
	case 14: {//madds0
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		op2 = ibuild.EmitFPDup0(op2);
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 15: {//madds1
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		op2 = ibuild.EmitFPDup1(op2);
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 28: {//msub
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPSub(val, op3);
		break;
	}
	case 29: {//madd
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 30: {//nmsub
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPSub(val, op3);
		val = ibuild.EmitFPNeg(val);
		break;
	}
	case 31: {//nmadd
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		val = ibuild.EmitFPNeg(val);
		break;
	}
	}
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}
