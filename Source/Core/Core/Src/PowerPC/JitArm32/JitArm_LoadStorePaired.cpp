// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include "Common.h"
#include "Thunk.h"

#include "../../Core.h"
#include "../PowerPC.h"
#include "../../CoreTiming.h"
#include "../PPCTables.h"
#include "ArmEmitter.h"

#include "Jit.h"
#include "JitRegCache.h"
#include "JitAsm.h"

void JitArm::psq_l(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(bJITLoadStorePairedOff)
	
	bool update = inst.OPCD == 57;
	s32 offset = inst.SIMM_12;

	// R12 contains scale
	// R11 contains type
	// R10 is the ADDR
	
	if (js.memcheck) { Default(inst); return; }
	
	LDR(R11, R9, PPCSTATE_OFF(spr[SPR_GQR0 + inst.I]));
	UBFX(R12, R11, 13, 3); // Type
	UBFX(R11, R11, 2, 6); // Scale

	MOVI2R(R10, (u32)offset);
	if (inst.RA)
		ADD(R10, R10, gpr.R(inst.RA));
	if (update)
		MOV(gpr.R(inst.RA), R10);
	MOVI2R(R14, (u32)asm_routines.pairedLoadQuantized);
	ADD(R14, R14, R12);
	LDR(R14, R14, inst.W ? 8 * 4 : 0);

	// Values returned in S0, S1
	BL(R14); // Jump to the quantizer Load

	ARMReg vD0 = fpr.R0(inst.RS, false);
	ARMReg vD1 = fpr.R1(inst.RS, false);
	VCVT(vD0, S0, 0);
	VCVT(vD1, S1, 0);
}
