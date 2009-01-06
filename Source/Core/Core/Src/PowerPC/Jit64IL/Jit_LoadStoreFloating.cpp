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
#include "JitCache.h"
#include "JitAsm.h"
#include "JitRegCache.h"

// pshufb todo: MOVQ
const u8 GC_ALIGNED16(bswapShuffle1x4[16]) = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle2x4[16]) = {3, 2, 1, 0, 7, 6, 5, 4, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15};
const u8 GC_ALIGNED16(bswapShuffle1x8Dupe[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 7, 6, 5, 4, 3, 2, 1, 0};
const u8 GC_ALIGNED16(bswapShuffle2x8[16]) = {7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8};

namespace {

u64 GC_ALIGNED16(temp64);
u32 GC_ALIGNED16(temp32);
}
// TODO: Add peephole optimizations for multiple consecutive lfd/lfs/stfd/stfs since they are so common,
// and pshufb could help a lot.
// Also add hacks for things like lfs/stfs the same reg consecutively, that is, simple memory moves.

void Jit64::lfs(UGeckoInstruction inst)
{
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_16), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
	return;
}


void Jit64::lfd(UGeckoInstruction inst)
{
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
	IREmitter::InstLoc addr = ibuild.EmitLoadGReg(inst.RB), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	val = ibuild.EmitDupSingleToMReg(ibuild.EmitLoadSingle(addr));
	ibuild.EmitStoreFReg(val, inst.RD);
}

