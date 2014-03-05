// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Core/PowerPC/JitILCommon/JitILBase.h"

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void JitILBase::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void JitILBase::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	val = ibuild.EmitLoadFReg(inst.RD);
	val = ibuild.EmitInsertDoubleInMReg(ibuild.EmitLoadDouble(addr), val);
	ibuild.EmitStoreFReg(val, inst.RD);
}


void JitILBase::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	IREmitter::InstLoc val  = ibuild.EmitLoadFReg(inst.RS);

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);

	ibuild.EmitStoreDouble(val, addr);
}


void JitILBase::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16);
	IREmitter::InstLoc val  = ibuild.EmitLoadFReg(inst.RS);

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);

	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
}


void JitILBase::stfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB);
	IREmitter::InstLoc val  = ibuild.EmitLoadFReg(inst.RS);

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
}


void JitILBase::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStoreFloatingOff)

	if (js.memcheck)
	{
		FallBackToInterpreter(inst);
		return;
	}

	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB), val;

	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));

	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
}

