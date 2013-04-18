// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "Thunk.h"
#include "../PowerPC.h"
#include "../../Core.h"
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

void JitIL::psq_st(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)
	if (js.memcheck) { Default(inst); return; }
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

void JitIL::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(LoadStorePaired)
	if (js.memcheck) { Default(inst); return; }
	if (inst.W) {Default(inst); return;}
	IREmitter::InstLoc addr = ibuild.EmitIntConst(inst.SIMM_12), val;
	if (inst.RA)
		addr = ibuild.EmitAdd(addr, ibuild.EmitLoadGReg(inst.RA));
	if (inst.OPCD == 57)
		ibuild.EmitStoreGReg(addr, inst.RA);
	val = ibuild.EmitLoadPaired(addr, inst.I | (inst.W << 3));	// The lower 3 bits is for GQR index. The next 1 bit is for inst.W
	val = ibuild.EmitExpandPackedToMReg(val);
	ibuild.EmitStoreFReg(val, inst.RD);
}
