// Copyright (C) 2003 Dolphin Project.

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

// Wrong, THP videos like SMS and Ikaruga show artifacts
void JitArm::ps_add(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)

	Default(inst); return;

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	if (inst.Rc){
		Default(inst); return;
	}
	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d);
	ARMReg vD1 = fpr.R1(d);

	VADD(vD0, vA0, vB0);
	VADD(vD1, vA1, vB1);
}

// Wrong, THP videos like SMS and Ikaruga show artifacts
void JitArm::ps_madd(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	
	Default(inst); return;

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	if (inst.Rc) {
		Default(inst); return;
	}
	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d);
	ARMReg vD1 = fpr.R1(d);

	ARMReg V0 = fpr.GetReg();
	ARMReg V1 = fpr.GetReg();

	VMOV(V0, vC0);
	VMOV(V1, vC1);
	
	VMLA(V0, vA0, vB0);
	VMLA(V1, vA1, vB1);

	VMOV(vD0, V0);
	VMOV(vD1, V1);

	fpr.Unlock(V0);
	fpr.Unlock(V1);
}

void JitArm::ps_sum0(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)

	u32 a = inst.FA, b = inst.FB, c = inst.FC, d = inst.FD;

	if (inst.Rc) {
		Default(inst); return;
	}
	ARMReg vA0 = fpr.R0(a);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d);
	ARMReg vD1 = fpr.R1(d);
	
	VADD(vD0, vA0, vB1);
	VMOV(vD1, vC1);
}

void JitArm::ps_sub(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)

	u32 a = inst.FA, b = inst.FB, d = inst.FD;
	if (inst.Rc){
		Default(inst); return;
	}
	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vB0 = fpr.R0(b);
	ARMReg vB1 = fpr.R1(b);
	ARMReg vD0 = fpr.R0(d);
	ARMReg vD1 = fpr.R1(d);

	VSUB(vD0, vA0, vB0);
	VSUB(vD1, vA1, vB1);
}

void JitArm::ps_mul(UGeckoInstruction inst)
{
	INSTRUCTION_START
	JITDISABLE(Paired)
	u32 a = inst.FA, c = inst.FC, d = inst.FD;
	if (inst.Rc){
		Default(inst); return;
	}
	ARMReg vA0 = fpr.R0(a);
	ARMReg vA1 = fpr.R1(a);
	ARMReg vC0 = fpr.R0(c);
	ARMReg vC1 = fpr.R1(c);
	ARMReg vD0 = fpr.R0(d);
	ARMReg vD1 = fpr.R1(d);

	VMUL(vD0, vA0, vC0);
	VMUL(vD1, vA1, vC1);
}

