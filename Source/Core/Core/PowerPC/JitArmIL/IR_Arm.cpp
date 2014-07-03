// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/ArmEmitter.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitArmIL/JitIL.h"
#include "Core/PowerPC/JitArmIL/JitILAsm.h"
#include "Core/PowerPC/JitILCommon/IR.h"

using namespace IREmitter;
using namespace ArmGen;
static const unsigned int MAX_NUMBER_OF_REGS = 32;

struct RegInfo {
	JitArmIL *Jit;
	IRBuilder* Build;
	InstLoc FirstI;
	std::vector<unsigned> IInfo;
	std::vector<InstLoc> lastUsed;
	InstLoc regs[MAX_NUMBER_OF_REGS];
	InstLoc fregs[MAX_NUMBER_OF_REGS];
	unsigned numSpills;
	unsigned numFSpills;
	unsigned exitNumber;

	RegInfo(JitArmIL* j, InstLoc f, unsigned insts) : Jit(j), FirstI(f), IInfo(insts), lastUsed(insts) {
		for (unsigned i = 0; i < MAX_NUMBER_OF_REGS; i++) {
			regs[i] = 0;
			fregs[i] = 0;
		}
		numSpills = 0;
		numFSpills = 0;
		exitNumber = 0;
	}

	private:
		RegInfo(RegInfo&); // DO NOT IMPLEMENT
};

static const ARMReg RegAllocOrder[] = {R0, R1, R2, R3, R4, R5, R6, R7, R8};
static const int RegAllocSize = sizeof(RegAllocOrder) / sizeof(ARMReg);

static unsigned SlotSet[1000];

static void regMarkUse(RegInfo& R, InstLoc I, InstLoc Op, unsigned OpNum) {
	unsigned& info = R.IInfo[Op - R.FirstI];
	if (info == 0) R.IInfo[I - R.FirstI] |= 1 << (OpNum + 1);
	if (info < 2) info++;
	R.lastUsed[Op - R.FirstI] = std::max(R.lastUsed[Op - R.FirstI], I);
}
static void regClearInst(RegInfo& RI, InstLoc I) {
	for (int i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == I)
			RI.regs[RegAllocOrder[i]] = 0;
}
static void regNormalRegClear(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4)
		regClearInst(RI, getOp1(I));
	if (RI.IInfo[I - RI.FirstI] & 8)
		regClearInst(RI, getOp2(I));
}

static unsigned regReadUse(RegInfo& R, InstLoc I) {
	return R.IInfo[I - R.FirstI] & 3;
}

static u32 regLocForSlot(RegInfo& RI, unsigned slot) {
	return (u32)&SlotSet[slot - 1];
}

static unsigned regCreateSpill(RegInfo& RI, InstLoc I) {
	unsigned newSpill = ++RI.numSpills;
	RI.IInfo[I - RI.FirstI] |= newSpill << 16;
	return newSpill;
}

static unsigned regGetSpill(RegInfo& RI, InstLoc I) {
	return RI.IInfo[I - RI.FirstI] >> 16;
}

static void regSpill(RegInfo& RI, ARMReg reg) {
	if (!RI.regs[reg]) return;
	unsigned slot = regGetSpill(RI, RI.regs[reg]);
	if (!slot) {
		slot = regCreateSpill(RI, RI.regs[reg]);
		RI.Jit->MOVI2R(R14, regLocForSlot(RI, slot));
		RI.Jit->STR(reg, R14, 0);
	}
	RI.regs[reg] = 0;
}

static ARMReg regFindFreeReg(RegInfo& RI) {
	for (int i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == 0)
			return RegAllocOrder[i];

	int bestIndex = -1;
	InstLoc bestEnd = 0;
	for (int i = 0; i < RegAllocSize; ++i) {
		const InstLoc start = RI.regs[RegAllocOrder[i]];
		const InstLoc end = RI.lastUsed[start - RI.FirstI];
		if (bestEnd < end) {
			bestEnd = end;
			bestIndex = i;
		}
	}

	ARMReg reg = RegAllocOrder[bestIndex];
	regSpill(RI, reg);
	return reg;
}
static ARMReg regLocForInst(RegInfo& RI, InstLoc I) {
	for (int i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == I)
			return RegAllocOrder[i];

	if (regGetSpill(RI, I) == 0)
		PanicAlert("Retrieving unknown spill slot?!");
	RI.Jit->MOVI2R(R14, regLocForSlot(RI, regGetSpill(RI, I)));
	ARMReg reg = regFindFreeReg(RI);
	RI.Jit->LDR(reg, R14, 0);
	return reg;
}
static ARMReg regBinLHSReg(RegInfo& RI, InstLoc I) {
	ARMReg reg = regFindFreeReg(RI);
	RI.Jit->MOV(reg, regLocForInst(RI, getOp1(I)));
	return reg;
}

// If the lifetime of the register used by an operand ends at I,
// return the register. Otherwise return a free register.
static ARMReg regBinReg(RegInfo& RI, InstLoc I) {
	// FIXME: When regLocForInst() is extracted as a local variable,
	//        "Retrieving unknown spill slot?!" is shown.
	if (RI.IInfo[I - RI.FirstI] & 4)
		return regLocForInst(RI, getOp1(I));
	else if (RI.IInfo[I - RI.FirstI] & 8)
		return regLocForInst(RI, getOp2(I));

	return regFindFreeReg(RI);
}

static void regSpillCallerSaved(RegInfo& RI) {
	regSpill(RI, R0);
	regSpill(RI, R1);
	regSpill(RI, R2);
	regSpill(RI, R3);
}

static ARMReg regEnsureInReg(RegInfo& RI, InstLoc I) {
	return regLocForInst(RI, I);
}

static void regWriteExit(RegInfo& RI, InstLoc dest) {
	if (isImm(*dest)) {
		RI.exitNumber++;
		RI.Jit->WriteExit(RI.Build->GetImmValue(dest));
	} else {
		RI.Jit->WriteExitDestInReg(regLocForInst(RI, dest));
	}
}
static void regStoreInstToPPCState(RegInfo& RI, unsigned width, InstLoc I, s32 offset) {
	void (JitArmIL::*op)(ARMReg, ARMReg, Operand2, bool);
	switch (width)
	{
		case 32:
			op = &JitArmIL::STR;
		break;
		case 8:
			op = &JitArmIL::STRB;
		break;
		default:
			PanicAlert("Not implemented!");
			return;
		break;
	}

	if (isImm(*I)) {
		RI.Jit->MOVI2R(R12, RI.Build->GetImmValue(I));
		(RI.Jit->*op)(R12, R9, offset, true);
		return;
	}
	ARMReg reg = regEnsureInReg(RI, I);
	(RI.Jit->*op)(reg, R9, offset, true);
}

//
// Mark and calculation routines for profiled load/store addresses
// Could be extended to unprofiled addresses.
static void regMarkMemAddress(RegInfo& RI, InstLoc I, InstLoc AI, unsigned OpNum) {
	if (isImm(*AI)) {
		unsigned addr = RI.Build->GetImmValue(AI);
		if (Memory::IsRAMAddress(addr))
			return;
	}
	if (getOpcode(*AI) == Add && isImm(*getOp2(AI))) {
		regMarkUse(RI, I, getOp1(AI), OpNum);
		return;
	}
	regMarkUse(RI, I, AI, OpNum);
}
// Binary ops
void JitArmIL::BIN_XOR(ARMReg reg, Operand2 op2)
{
	EOR(reg, reg, op2);
}
void JitArmIL::BIN_OR(ARMReg reg, Operand2 op2)
{
	ORR(reg, reg, op2);
}
void JitArmIL::BIN_AND(ARMReg reg, Operand2 op2)
{
	AND(reg, reg, op2);
}
void JitArmIL::BIN_ADD(ARMReg reg, Operand2 op2)
{
	ADD(reg, reg, op2);
}
static void regEmitShiftInst(RegInfo& RI, InstLoc I, void (JitArmIL::*op)(ARMReg, ARMReg, Operand2))
{
	ARMReg reg = regBinLHSReg(RI, I);
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		(RI.Jit->*op)(reg, reg, RHS);
		RI.regs[reg] = I;
		return;
	}
	(RI.Jit->*op)(reg, reg, regLocForInst(RI, getOp2(I)));
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void regEmitBinInst(RegInfo& RI, InstLoc I,
			   void (JitArmIL::*op)(ARMReg, Operand2),
			   bool commutable = false) {
	ARMReg reg;
	bool commuted = false;
	if (RI.IInfo[I - RI.FirstI] & 4) {
		reg = regEnsureInReg(RI, getOp1(I));
	} else if (commutable && (RI.IInfo[I - RI.FirstI] & 8)) {
		reg = regEnsureInReg(RI, getOp2(I));
		commuted = true;
	} else {
		reg = regFindFreeReg(RI);
		RI.Jit->MOV(reg, regLocForInst(RI, getOp1(I)));
	}
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		Operand2 RHSop;
		if (TryMakeOperand2(RHS, RHSop))
			(RI.Jit->*op)(reg, RHSop);
		else
		{
			RI.Jit->MOVI2R(R12, RHS);
			(RI.Jit->*op)(reg, R12);
		}
	} else if (commuted) {
		(RI.Jit->*op)(reg, regLocForInst(RI, getOp1(I)));
	} else {
		(RI.Jit->*op)(reg, regLocForInst(RI, getOp2(I)));
	}
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}
static void regEmitCmp(RegInfo& RI, InstLoc I) {
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		Operand2 op;
		if (TryMakeOperand2(RHS, op))
			RI.Jit->CMP(regLocForInst(RI, getOp1(I)), op);
		else
		{
			RI.Jit->MOVI2R(R12, RHS);
			RI.Jit->CMP(regLocForInst(RI, getOp1(I)), R12);
		}
	} else {
		ARMReg reg = regEnsureInReg(RI, getOp1(I));
		RI.Jit->CMP(reg, regLocForInst(RI, getOp2(I)));
	}
}

static void DoWriteCode(IRBuilder* ibuild, JitArmIL* Jit, u32 exitAddress) {
	RegInfo RI(Jit, ibuild->getFirstInst(), ibuild->getNumInsts());
	RI.Build = ibuild;

	// Pass to compute liveness
	ibuild->StartBackPass();
	for (unsigned int index = (unsigned int)RI.IInfo.size() - 1; index != -1U; --index) {
		InstLoc I = ibuild->ReadBackward();
		unsigned int op = getOpcode(*I);
		bool thisUsed = regReadUse(RI, I) ? true : false;
		switch (op) {
		default:
			PanicAlert("Unexpected inst!");
		case Nop:
		case CInt16:
		case CInt32:
		case LoadGReg:
		case LoadLink:
		case LoadCR:
		case LoadCarry:
		case LoadCTR:
		case LoadMSR:
		case LoadFReg:
		case LoadFRegDENToZero:
		case LoadGQR:
		case BlockEnd:
		case BlockStart:
		case FallBackToInterpreter:
		case SystemCall:
		case RFIExit:
		case InterpreterBranch:
		case ShortIdleLoop:
		case FPExceptionCheck:
		case DSIExceptionCheck:
		case ISIException:
		case ExtExceptionCheck:
		case BreakPointCheck:
		case Int3:
		case Tramp:
			// No liveness effects
			break;
		case SExt8:
		case SExt16:
		case BSwap32:
		case BSwap16:
		case Cntlzw:
		case Not:
		case DupSingleToMReg:
		case DoubleToSingle:
		case ExpandPackedToMReg:
		case CompactMRegToPacked:
		case FPNeg:
		case FPDup0:
		case FPDup1:
		case FSNeg:
		case FDNeg:
			if (thisUsed)
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case Load8:
		case Load16:
		case Load32:
			regMarkMemAddress(RI, I, getOp1(I), 1);
			break;
		case LoadDouble:
		case LoadSingle:
		case LoadPaired:
			if (thisUsed)
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case StoreCR:
		case StoreCarry:
		case StoreFPRF:
			regMarkUse(RI, I, getOp1(I), 1);
			break;
		case StoreGReg:
		case StoreLink:
		case StoreCTR:
		case StoreMSR:
		case StoreGQR:
		case StoreSRR:
		case StoreFReg:
			if (!isImm(*getOp1(I)))
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case Add:
		case Sub:
		case And:
		case Or:
		case Xor:
		case Mul:
		case MulHighUnsigned:
		case Rol:
		case Shl:
		case Shrl:
		case Sarl:
		case ICmpCRUnsigned:
		case ICmpCRSigned:
		case ICmpEq:
		case ICmpNe:
		case ICmpUgt:
		case ICmpUlt:
		case ICmpUge:
		case ICmpUle:
		case ICmpSgt:
		case ICmpSlt:
		case ICmpSge:
		case ICmpSle:
		case FSMul:
		case FSAdd:
		case FSSub:
		case FDMul:
		case FDAdd:
		case FDSub:
		case FPAdd:
		case FPMul:
		case FPSub:
		case FPMerge00:
		case FPMerge01:
		case FPMerge10:
		case FPMerge11:
		case FDCmpCR:
		case InsertDoubleInMReg:
			if (thisUsed) {
				regMarkUse(RI, I, getOp1(I), 1);
				if (!isImm(*getOp2(I)))
					regMarkUse(RI, I, getOp2(I), 2);
			}
			break;
		case Store8:
		case Store16:
		case Store32:
			if (!isImm(*getOp1(I)))
				regMarkUse(RI, I, getOp1(I), 1);
			regMarkMemAddress(RI, I, getOp2(I), 2);
			break;
		case StoreSingle:
		case StoreDouble:
		case StorePaired:
			regMarkUse(RI, I, getOp1(I), 1);
			regMarkUse(RI, I, getOp2(I), 2);
			break;
		case BranchUncond:
			if (!isImm(*getOp1(I)))
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case IdleBranch:
			regMarkUse(RI, I, getOp1(getOp1(I)), 1);
			break;
		case BranchCond: {
			if (isICmp(*getOp1(I)) &&
			    isImm(*getOp2(getOp1(I)))) {
				regMarkUse(RI, I, getOp1(getOp1(I)), 1);
			} else {
				regMarkUse(RI, I, getOp1(I), 1);
			}
			if (!isImm(*getOp2(I)))
				regMarkUse(RI, I, getOp2(I), 2);
			break;
		}
		}
	}

	ibuild->StartForwardPass();
		for (unsigned i = 0; i != RI.IInfo.size(); i++) {
		InstLoc I = ibuild->ReadForward();

		bool thisUsed = regReadUse(RI, I) ? true : false;
		if (thisUsed) {
			// Needed for IR Writer
			ibuild->SetMarkUsed(I);
		}

		switch (getOpcode(*I)) {
		case CInt32:
		case CInt16: {
			if (!thisUsed) break;
			ARMReg reg = regFindFreeReg(RI);
			Jit->MOVI2R(reg, ibuild->GetImmValue(I));
			RI.regs[reg] = I;
			break;
			}
		case BranchUncond: {
			regWriteExit(RI, getOp1(I));
			regNormalRegClear(RI, I);
			break;
			}
		case BranchCond: {
			if (isICmp(*getOp1(I)) &&
			    isImm(*getOp2(getOp1(I)))) {
				unsigned imm = RI.Build->GetImmValue(getOp2(getOp1(I)));
				if (imm > 255)
				{
					Jit->MOVI2R(R14, imm);
					Jit->CMP(regLocForInst(RI, getOp1(getOp1(I))), R14);
				}
				else
					Jit->CMP(regLocForInst(RI, getOp1(getOp1(I))), imm);
				CCFlags flag;
				switch (getOpcode(*getOp1(I))) {
					case ICmpEq: flag = CC_NEQ; break;
					case ICmpNe: flag = CC_EQ; break;
					case ICmpUgt: flag = CC_LS; break;
					case ICmpUlt: flag = CC_HI; break;
					case ICmpUge: flag = CC_HS; break;
					case ICmpUle: flag = CC_LO; break;
					case ICmpSgt: flag = CC_LT; break;
					case ICmpSlt: flag = CC_GT; break;
					case ICmpSge: flag = CC_LE; break;
					case ICmpSle: flag = CC_GE; break;
					default: PanicAlert("cmpXX"); flag = CC_AL; break;
				}
				FixupBranch cont = Jit->B_CC(flag);
				regWriteExit(RI, getOp2(I));
				Jit->SetJumpTarget(cont);
				if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(getOp1(I)));
			} else {
				Jit->CMP(regLocForInst(RI, getOp1(I)), 0);
				FixupBranch cont = Jit->B_CC(CC_EQ);
				regWriteExit(RI, getOp2(I));
				Jit->SetJumpTarget(cont);
				if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(I));
			}
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}

		case StoreGReg: {
			unsigned ppcreg = *I >> 16;
			regStoreInstToPPCState(RI, 32, getOp1(I), PPCSTATE_OFF(gpr[ppcreg]));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCR: {
			unsigned ppcreg = *I >> 16;
			regStoreInstToPPCState(RI, 8, getOp1(I), PPCSTATE_OFF(cr_fast[ppcreg]));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreLink: {
			regStoreInstToPPCState(RI, 32, getOp1(I), PPCSTATE_OFF(spr[SPR_LR]));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCTR: {
			regStoreInstToPPCState(RI, 32, getOp1(I), PPCSTATE_OFF(spr[SPR_CTR]));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreMSR: {
			regStoreInstToPPCState(RI, 32, getOp1(I), PPCSTATE_OFF(msr));
			regNormalRegClear(RI, I);
			break;
		}
		case LoadGReg: {
			if (!thisUsed) break;
			ARMReg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->LDR(reg, R9, PPCSTATE_OFF(gpr[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCR: {
			if (!thisUsed) break;
			ARMReg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->LDRB(reg, R9, PPCSTATE_OFF(cr_fast[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCTR: {
			if (!thisUsed) break;
			ARMReg reg = regFindFreeReg(RI);
			Jit->LDR(reg, R9, PPCSTATE_OFF(spr[SPR_CTR]));
			RI.regs[reg] = I;
			break;
		}
		case LoadLink: {
			if (!thisUsed) break;
			ARMReg reg = regFindFreeReg(RI);
			Jit->LDR(reg, R9, PPCSTATE_OFF(spr[SPR_LR]));
			RI.regs[reg] = I;
			break;
		}
		case FallBackToInterpreter: {
			unsigned InstCode = ibuild->GetImmValue(getOp1(I));
			unsigned InstLoc = ibuild->GetImmValue(getOp2(I));
			// There really shouldn't be anything live across an
			// interpreter call at the moment, but optimizing interpreter
			// calls isn't completely out of the question...
			regSpillCallerSaved(RI);
			Jit->MOVI2R(R14, InstLoc);
			Jit->STR(R14, R9, PPCSTATE_OFF(pc));
			Jit->MOVI2R(R14, InstLoc + 4);
			Jit->STR(R14, R9, PPCSTATE_OFF(npc));

			Jit->MOVI2R(R0, InstCode);
			Jit->MOVI2R(R14, (u32)GetInterpreterOp(InstCode));
			Jit->BL(R14);
			break;
		}
		case SystemCall: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->MOVI2R(R14, InstLoc + 4);
			Jit->STR(R14, R9, PPCSTATE_OFF(pc));
			Jit->LDR(R14, R9, PPCSTATE_OFF(Exceptions));
			Jit->ORR(R14, R14, EXCEPTION_SYSCALL);
			Jit->STR(R14, R9, PPCSTATE_OFF(Exceptions));
			Jit->WriteExceptionExit();
			break;
		}
		case ShortIdleLoop: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->MOVI2R(R14, (u32)&CoreTiming::Idle);
			Jit->BL(R14);
			Jit->MOVI2R(R14, InstLoc);
			Jit->STR(R14, R9, PPCSTATE_OFF(pc));
			Jit->WriteExceptionExit();
			break;
		}
		case InterpreterBranch: {
			Jit->LDR(R14, R9, PPCSTATE_OFF(npc));
			Jit->WriteExitDestInReg(R14);
			break;
		}
		case RFIExit: {
			const u32 mask = 0x87C0FFFF;
			const u32 clearMSR13 = 0xFFFBFFFF; // Mask used to clear the bit MSR[13]
			// MSR = ((MSR & ~mask) | (SRR1 & mask)) & clearMSR13;
			// R0 = MSR location
			// R1 = MSR contents
			// R2 = Mask
			// R3 = Mask
			ARMReg rA = R14;
			ARMReg rB = R12;
			ARMReg rC = R11;
			ARMReg rD = R10;
			Jit->MOVI2R(rB, (~mask) & clearMSR13);
			Jit->MOVI2R(rC, mask & clearMSR13);

			Jit->LDR(rD, R9, PPCSTATE_OFF(msr));

			Jit->AND(rD, rD, rB); // rD = Masked MSR

			Jit->LDR(rB, R9, PPCSTATE_OFF(spr[SPR_SRR1])); // rB contains SRR1 here

			Jit->AND(rB, rB, rC); // rB contains masked SRR1 here
			Jit->ORR(rB, rD, rB); // rB = Masked MSR OR masked SRR1

			Jit->STR(rB, R9, PPCSTATE_OFF(msr)); // STR rB in to rA

			Jit->LDR(rA, R9, PPCSTATE_OFF(spr[SPR_SRR0]));

			Jit->WriteRfiExitDestInR(rA); // rA gets unlocked here
			break;
		}
		case Shl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitArmIL::LSL);
			break;
		}
		case Shrl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitArmIL::LSR);
			break;
		}
		case Sarl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitArmIL::ASR);
			break;
		}
		case And: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitArmIL::BIN_AND, true);
			break;
		}
		case Not: {
			if (!thisUsed) break;
			ARMReg reg = regBinLHSReg(RI, I);
			Jit->MVN(reg, reg);
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Or: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitArmIL::BIN_OR, true);
			break;
		}
		case Xor: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitArmIL::BIN_XOR, true);
			break;
		}
		case Add: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitArmIL::BIN_ADD, true);
			break;
		}
		case ICmpCRUnsigned: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			ARMReg reg = regBinReg(RI, I);
			Jit->MOV(reg, 0x2); // Result == 0
			Jit->SetCC(CC_LO); Jit->MOV(reg, 0x8); // Result < 0
			Jit->SetCC(CC_HI); Jit->MOV(reg, 0x4); // Result > 0
			Jit->SetCC();
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}

		case ICmpCRSigned: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			ARMReg reg = regBinReg(RI, I);
			Jit->MOV(reg, 0x2); // Result == 0
			Jit->SetCC(CC_LT); Jit->MOV(reg, 0x8); // Result < 0
			Jit->SetCC(CC_GT); Jit->MOV(reg, 0x4); // Result > 0
			Jit->SetCC();
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Int3:
			Jit->BKPT(0x321);
			break;
		case Tramp: break;
		case Nop: break;
		default:
			PanicAlert("Unknown JIT instruction; aborting!");
			ibuild->WriteToFile(0);
			exit(1);
		}
	}
	for (unsigned i = 0; i < MAX_NUMBER_OF_REGS; i++) {
		if (RI.regs[i]) {
			// Start a game in Burnout 2 to get this. Or animal crossing.
			PanicAlert("Incomplete cleanup! (regs)");
			exit(1);
		}
		if (RI.fregs[i]) {
			PanicAlert("Incomplete cleanup! (fregs)");
			exit(1);
		}
	}

	Jit->WriteExit(exitAddress);
	Jit->BKPT(0x111);

}
void JitArmIL::WriteCode(u32 exitAddress) {
	DoWriteCode(&ibuild, this, exitAddress);
}
