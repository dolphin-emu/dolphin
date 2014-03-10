// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArmIL/JitIL.h"

// FIXME
#define NORMALBRANCH_START FallBackToInterpreter(inst); ibuild.EmitInterpreterBranch(); return;
//#define NORMALBRANCH_START

void JitArmIL::bx(UGeckoInstruction inst)
{
	//NORMALBRANCH_START
	INSTRUCTION_START;

	// We must always process the following sentence
	// even if the blocks are merged by PPCAnalyst::Flatten().
	if (inst.LK)
		ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));

	// If this is not the last instruction of a block,
	// we will skip the rest process.
	// Because PPCAnalyst::Flatten() merged the blocks.
	if (!js.isLastInstruction) {
		return;
	}

	u32 destination;
	if (inst.AA)
		destination = SignExt26(inst.LI << 2);
	else
		destination = js.compilerPC + SignExt26(inst.LI << 2);

	if (destination == js.compilerPC) {
		ibuild.EmitShortIdleLoop(ibuild.EmitIntConst(js.compilerPC));
		return;
	}

	ibuild.EmitBranchUncond(ibuild.EmitIntConst(destination));
}
static IREmitter::InstLoc TestBranch(IREmitter::IRBuilder& ibuild, UGeckoInstruction inst) {
	IREmitter::InstLoc CRTest = 0, CTRTest = 0;
	if ((inst.BO & 16) == 0)  // Test a CR bit
	{
		IREmitter::InstLoc CRReg = ibuild.EmitLoadCR(inst.BI >> 2);
		IREmitter::InstLoc CRCmp = ibuild.EmitIntConst(8 >> (inst.BI & 3));
		CRTest = ibuild.EmitAnd(CRReg, CRCmp);
		if (!(inst.BO & 8))
			CRTest = ibuild.EmitXor(CRCmp, CRTest);
	}

	if ((inst.BO & 4) == 0) {
		IREmitter::InstLoc c = ibuild.EmitLoadCTR();
		c = ibuild.EmitSub(c, ibuild.EmitIntConst(1));
		ibuild.EmitStoreCTR(c);
		if (inst.BO & 2) {
			CTRTest = ibuild.EmitICmpEq(c,
					ibuild.EmitIntConst(0));
		} else {
			CTRTest = c;
		}
	}

	IREmitter::InstLoc Test = CRTest;
	if (CTRTest) {
		if (Test)
			Test = ibuild.EmitAnd(Test, CTRTest);
		else
			Test = CTRTest;
	}

	if (!Test) {
		Test = ibuild.EmitIntConst(1);
	}
	return Test;
}

void JitArmIL::bclrx(UGeckoInstruction inst)
{
	NORMALBRANCH_START

	if (!js.isLastInstruction &&
		(inst.BO & (1 << 4)) && (inst.BO & (1 << 2))) {
		if (inst.LK)
			ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));
		return;
	}

	if (inst.hex == 0x4e800020) {
		ibuild.EmitBranchUncond(ibuild.EmitLoadLink());
		return;
	}
	IREmitter::InstLoc test = TestBranch(ibuild, inst);
	test = ibuild.EmitICmpEq(test, ibuild.EmitIntConst(0));
	ibuild.EmitBranchCond(test, ibuild.EmitIntConst(js.compilerPC + 4));

	IREmitter::InstLoc destination = ibuild.EmitLoadLink();
	destination = ibuild.EmitAnd(destination, ibuild.EmitIntConst(-4));
	if (inst.LK)
		ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));
	ibuild.EmitBranchUncond(destination);
}
void JitArmIL::bcx(UGeckoInstruction inst)
{
	NORMALBRANCH_START
	if (inst.LK)
		ibuild.EmitStoreLink(
			ibuild.EmitIntConst(js.compilerPC + 4));

	IREmitter::InstLoc Test = TestBranch(ibuild, inst);

	u32 destination;
	if (inst.AA)
		destination = SignExt16(inst.BD << 2);
	else
		destination = js.compilerPC + SignExt16(inst.BD << 2);

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle &&
		inst.hex == 0x4182fff8 &&
		(Memory::ReadUnchecked_U32(js.compilerPC - 8) & 0xFFFF0000) == 0x800D0000 &&
		(Memory::ReadUnchecked_U32(js.compilerPC - 4) == 0x28000000 ||
		(SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && Memory::ReadUnchecked_U32(js.compilerPC - 4) == 0x2C000000))
		)
	{
		ibuild.EmitIdleBranch(Test, ibuild.EmitIntConst(destination));
	}
	else
	{
		ibuild.EmitBranchCond(Test, ibuild.EmitIntConst(destination));
	}
	ibuild.EmitBranchUncond(ibuild.EmitIntConst(js.compilerPC + 4));
}

void JitArmIL::bcctrx(UGeckoInstruction inst)
{
	NORMALBRANCH_START
	if ((inst.BO & 4) == 0) {
		IREmitter::InstLoc c = ibuild.EmitLoadCTR();
		c = ibuild.EmitSub(c, ibuild.EmitIntConst(1));
		ibuild.EmitStoreCTR(c);
	}
	IREmitter::InstLoc test;
	if ((inst.BO & 16) == 0)  // Test a CR bit
	{
		IREmitter::InstLoc CRReg = ibuild.EmitLoadCR(inst.BI >> 2);
		IREmitter::InstLoc CRCmp = ibuild.EmitIntConst(8 >> (inst.BI & 3));
		test = ibuild.EmitAnd(CRReg, CRCmp);
		if (!(inst.BO & 8))
			test = ibuild.EmitXor(test, CRCmp);
	} else {
		test = ibuild.EmitIntConst(1);
	}
	test = ibuild.EmitICmpEq(test, ibuild.EmitIntConst(0));
	ibuild.EmitBranchCond(test, ibuild.EmitIntConst(js.compilerPC + 4));

	IREmitter::InstLoc destination = ibuild.EmitLoadCTR();
	destination = ibuild.EmitAnd(destination, ibuild.EmitIntConst(-4));
	if (inst.LK)
		ibuild.EmitStoreLink(ibuild.EmitIntConst(js.compilerPC + 4));
	ibuild.EmitBranchUncond(destination);
}

