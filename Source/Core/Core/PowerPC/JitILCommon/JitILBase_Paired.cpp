// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

void JitILBase::ps_arith(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc || (inst.SUBOP5 != 21 && inst.SUBOP5 != 20 && inst.SUBOP5 != 25));

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
	IREmitter::InstLoc rhs;

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

void JitILBase::ps_sum(UGeckoInstruction inst)
{
	// TODO: This operation strikes me as a bit strange...
	// perhaps we can optimize it depending on the users?
	// TODO: ps_sum breaks Sonic Colours (black screen)
	FALLBACK_IF(true);

	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc || inst.SUBOP5 != 10);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
	IREmitter::InstLoc temp;

	val = ibuild.EmitCompactMRegToPacked(val);
	val = ibuild.EmitFPDup0(val);
	temp = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
	val = ibuild.EmitFPAdd(val, temp);
	temp = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
	val = ibuild.EmitFPMerge11(val, temp);
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.FD);
}


void JitILBase::ps_muls(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
	IREmitter::InstLoc rhs = ibuild.EmitLoadFReg(inst.FC);

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
void JitILBase::ps_mergeXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	IREmitter::InstLoc val = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FA));
	IREmitter::InstLoc rhs = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));

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


void JitILBase::ps_maddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITPairedOff);
	FALLBACK_IF(inst.Rc);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA), op2, op3;
	val = ibuild.EmitCompactMRegToPacked(val);

	switch (inst.SUBOP5)
	{
	case 14: // madds0
	{
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		op2 = ibuild.EmitFPDup0(op2);
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 15: // madds1
	{
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		op2 = ibuild.EmitFPDup1(op2);
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 28: // msub
	{
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPSub(val, op3);
		break;
	}
	case 29: // madd
	{
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPAdd(val, op3);
		break;
	}
	case 30: // nmsub
	{
		op2 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FC));
		val = ibuild.EmitFPMul(val, op2);
		op3 = ibuild.EmitCompactMRegToPacked(ibuild.EmitLoadFReg(inst.FB));
		val = ibuild.EmitFPSub(val, op3);
		val = ibuild.EmitFPNeg(val);
		break;
	}
	case 31: // nmadd
	{
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
