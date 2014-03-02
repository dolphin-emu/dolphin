// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

void JitILBase::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	if (inst.W)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_12);
	IREmitter::InstLoc val;

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	if (inst.OPCD == 61)
		ibuild.EmitStoreGReg(addr, inst.RA);

	val = ibuild.EmitLoadFReg(inst.RS);
	val = ibuild.EmitCompactMRegToPacked(val);
	ibuild.EmitStorePaired(val, addr, inst.I);
}

void JitILBase::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	if (inst.W)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_12);
	IREmitter::InstLoc val;

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	if (inst.OPCD == 57)
		ibuild.EmitStoreGReg(addr, inst.RA);

	val = ibuild.EmitLoadPaired(addr, inst.I | (inst.W << 3)); // The lower 3 bits is for GQR index. The next 1 bit is for inst.W
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.RD);
}
