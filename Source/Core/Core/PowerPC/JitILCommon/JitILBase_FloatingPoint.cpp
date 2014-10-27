// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

void JitILBase::fp_arith_s(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc || (inst.SUBOP5 != 25 && inst.SUBOP5 != 20 && inst.SUBOP5 != 21));

	// Only the interpreter has "proper" support for (some) FP flags
	FALLBACK_IF(inst.SUBOP5 == 25 && SConfig::GetInstance().m_LocalCoreStartupParameter.bFPRF);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
	switch (inst.SUBOP5)
	{
	case 20: //sub
		val = ibuild.EmitFDSub(val, ibuild.EmitLoadFReg(inst.FB));
		break;
	case 21: //add
		val = ibuild.EmitFDAdd(val, ibuild.EmitLoadFReg(inst.FB));
		break;
	case 25: //mul
		val = ibuild.EmitFDMul(val, ibuild.EmitLoadFReg(inst.FC));
		break;
	default:
		_assert_msg_(DYNA_REC, 0, "fp_arith_s WTF!!!");
	}

	if (inst.OPCD == 59)
	{
		val = ibuild.EmitDoubleToSingle(val);
		val = ibuild.EmitDupSingleToMReg(val);
	}
	else
	{
		val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
	}
	ibuild.EmitStoreFReg(val, inst.FD);
}

void JitILBase::fmaddXX(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	// Only the interpreter has "proper" support for (some) FP flags
	FALLBACK_IF(inst.SUBOP5 == 29 && SConfig::GetInstance().m_LocalCoreStartupParameter.bFPRF);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FA);
	val = ibuild.EmitFDMul(val, ibuild.EmitLoadFReg(inst.FC));

	if (inst.SUBOP5 & 1)
		val = ibuild.EmitFDAdd(val, ibuild.EmitLoadFReg(inst.FB));
	else
		val = ibuild.EmitFDSub(val, ibuild.EmitLoadFReg(inst.FB));

	if (inst.SUBOP5 & 2)
		val = ibuild.EmitFDNeg(val);

	if (inst.OPCD == 59)
	{
		val = ibuild.EmitDoubleToSingle(val);
		val = ibuild.EmitDupSingleToMReg(val);
	}
	else
	{
		val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
	}

	ibuild.EmitStoreFReg(val, inst.FD);
}

void JitILBase::fmrx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	FALLBACK_IF(inst.Rc);

	IREmitter::InstLoc val = ibuild.EmitLoadFReg(inst.FB);
	val = ibuild.EmitInsertDoubleInMReg(val, ibuild.EmitLoadFReg(inst.FD));
	ibuild.EmitStoreFReg(val, inst.FD);
}

void JitILBase::fcmpx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);
	IREmitter::InstLoc lhs, rhs, res;
	lhs = ibuild.EmitLoadFReg(inst.FA);
	rhs = ibuild.EmitLoadFReg(inst.FB);
	int ordered = (inst.SUBOP10 == 32) ? 1 : 0;
	res = ibuild.EmitFDCmpCR(lhs, rhs, ordered);
	ibuild.EmitStoreFPRF(res);
	ibuild.EmitStoreCR(ibuild.EmitConvertToFastCR(res), inst.CRFD);
}

void JitILBase::fsign(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITFloatingPointOff);

	FALLBACK_IF(true);

	// TODO
	switch (inst.SUBOP10)
	{
	case 40:  // fnegx
		break;
	case 264: // fabsx
		break;
	case 136: // fnabs
		break;
	default:
		PanicAlert("fsign bleh");
		break;
	}
}
