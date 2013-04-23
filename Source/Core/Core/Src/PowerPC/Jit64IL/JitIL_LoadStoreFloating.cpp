// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common.h"

#include "../PowerPC.h"
#include "../../Core.h" // include "Common.h", "CoreParameter.h"
#include "../../HW/GPFifo.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "CPUDetect.h"
#include "x64Emitter.h"
#include "x64ABI.h"

#include "JitIL.h"
#include "JitILAsm.h"

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void JitIL::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void JitIL::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitLoadFReg(inst.RD);
	val = ibuild.EmitInsertDoubleInMReg(ibuild.EmitLoadDouble(addr), val);
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void JitIL::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	ibuild.EmitStoreDouble(val, addr);
	return;
}


void JitIL::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
	return;
}


void JitIL::stfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
	return;
}


void JitIL::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	if (js.memcheck) { Default(inst); return; }
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
}

