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

#include "Thunk.h"
#include "../PowerPC.h"
#include "../../Core.h"
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

// The big problem is likely instructions that set the quantizers in the same block.
// We will have to break block after quantizers are written to.
void Jit64::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)
	if (inst.W) {Default(inst); return;}
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_12), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD == 61)
		ibuild.EmitStoreGReg(addr, inst.RA);
	val = ibuild.EmitLoadFReg(inst.RS);
	val = ibuild.EmitCompactMRegToPacked(val);
	ibuild.EmitStorePaired(val, addr, inst.I);
}

void Jit64::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)
	if (inst.W) {Default(inst); return;}
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_12), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD == 57)
		ibuild.EmitStoreGReg(addr, inst.RA);
	val = ibuild.EmitLoadPaired(addr, inst.I);
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.RD);
}
