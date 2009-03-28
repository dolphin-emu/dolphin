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

// TODO(ector): Tons of pshufb optimization of the loads/stores, for SSSE3+, possibly SSE4, only.
// Should give a very noticable speed boost to paired single heavy code.

#include "Common.h"

#include "../PowerPC.h"
#include "../../Core.h" // include "Common.h", "CoreParameter.h"
#include "../../HW/GPFifo.h"
#include "../../HW/CommandProcessor.h"
#include "../../HW/PixelEngine.h"
#include "../../HW/Memmap.h"
#include "../PPCTables.h"
#include "CPUDetect.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

//#define INSTRUCTION_START Default(inst); return;
#define INSTRUCTION_START

// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void Jit64::lfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void Jit64::lfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitLoadFReg(inst.RD);
	val = ibuild.EmitInsertDoubleInMReg(ibuild.EmitLoadDouble(addr), val);
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void Jit64::stfd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD & 1)
		ibuild.EmitStoreGReg(addr, inst.RA);
	ibuild.EmitStoreDouble(val, addr);
	return;
}


void Jit64::stfs(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
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


void Jit64::stfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB),
			   val  = ibuild.EmitLoadFReg(inst.RS);
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDoubleToSingle(val);
	ibuild.EmitStoreSingle(val, addr);
	return;
}


void Jit64::lfsx(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStoreFloating)
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
}

