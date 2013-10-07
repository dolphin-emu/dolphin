// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "../PowerPC.h"
#include "../PPCTables.h"

#include "JitIL.h"

#include "../../HW/Memmap.h"

// =======================================================================================
// Don't interpret this, if we do we get thrown out
// --------------
void JitArmIL::mtmsr(UGeckoInstruction inst)
{
	ibuild.EmitStoreMSR(ibuild.EmitLoadGReg(inst.RS), ibuild.EmitIntConst(js.compilerPC));
	ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
}
