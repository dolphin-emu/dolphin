// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

/*
For a more general explanation of the IR, see IR.cpp.

X86 codegen is a backward pass followed by a forward pass.

The first pass to actually doing codegen is a liveness analysis pass.
Liveness is important for two reasons: one, it lets us do dead code
elimination, which results both from earlier folding, PPC
instructions with unused parts like srawx, and just random strangeness.
The other bit is that is allows us to identify the last instruction to
use a value: this is absolutely essential for register allocation
because it the allocator needs to be able to free unused registers.
In addition, this allows eliminating redundant mov instructions in a lot
of cases.

The register allocation is linear scan allocation.
*/

#ifdef _MSC_VER
#pragma warning(disable:4146)   // unary minus operator applied to unsigned type, result still unsigned
#endif

#include <algorithm>

#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/Jit64IL/JitIL.h"

using namespace IREmitter;
using namespace Gen;

static const unsigned int MAX_NUMBER_OF_REGS = 16;

struct RegInfo
{
	JitIL *Jit;
	IRBuilder* Build;
	InstLoc FirstI;
	std::vector<unsigned> IInfo;
	std::vector<InstLoc> lastUsed;
	InstLoc regs[MAX_NUMBER_OF_REGS];
	InstLoc fregs[MAX_NUMBER_OF_REGS];
	unsigned numSpills;
	unsigned numFSpills;
	unsigned exitNumber;

	RegInfo(JitIL* j, InstLoc f, unsigned insts) : Jit(j), FirstI(f), IInfo(insts), lastUsed(insts)
	{
		for (unsigned i = 0; i < MAX_NUMBER_OF_REGS; i++)
		{
			regs[i] = nullptr;
			fregs[i] = nullptr;
		}
		numSpills = 0;
		numFSpills = 0;
		exitNumber = 0;
	}

	private:
		RegInfo(RegInfo&); // DO NOT IMPLEMENT
};

static u32 regsInUse(RegInfo& R)
{
	u32 result = 0;
	for (unsigned i = 0; i < MAX_NUMBER_OF_REGS; i++)
	{
		if (R.regs[i] != nullptr)
			result |= (1 << i);
		if (R.fregs[i] != nullptr)
			result |= (1 << (16 + i));
	}
	return result;
}

static void regMarkUse(RegInfo& R, InstLoc I, InstLoc Op, unsigned OpNum)
{
	unsigned& info = R.IInfo[Op - R.FirstI];

	if (info == 0)
		R.IInfo[I - R.FirstI] |= 1 << (OpNum + 1);

	if (info < 2)
		info++;

	R.lastUsed[Op - R.FirstI] = std::max(R.lastUsed[Op - R.FirstI], I);
}

static unsigned regReadUse(RegInfo& R, InstLoc I)
{
	return R.IInfo[I - R.FirstI] & 3;
}

static u64 SlotSet[1000];
static u8 GC_ALIGNED16(FSlotSet[16*1000]);

static OpArg regLocForSlot(RegInfo& RI, unsigned slot)
{
	return M(&SlotSet[slot - 1]);
}

static unsigned regCreateSpill(RegInfo& RI, InstLoc I)
{
	unsigned newSpill = ++RI.numSpills;
	RI.IInfo[I - RI.FirstI] |= newSpill << 16;
	return newSpill;
}

static unsigned regGetSpill(RegInfo& RI, InstLoc I)
{
	return RI.IInfo[I - RI.FirstI] >> 16;
}

static void regSpill(RegInfo& RI, X64Reg reg)
{
	if (!RI.regs[reg])
		return;

	unsigned slot = regGetSpill(RI, RI.regs[reg]);
	if (!slot)
	{
		slot = regCreateSpill(RI, RI.regs[reg]);
		RI.Jit->MOV(64, regLocForSlot(RI, slot), R(reg));
	}

	RI.regs[reg] = nullptr;
}

static OpArg fregLocForSlot(RegInfo& RI, unsigned slot)
{
	return M(&FSlotSet[slot*16]);
}

static unsigned fregCreateSpill(RegInfo& RI, InstLoc I)
{
	unsigned newSpill = ++RI.numFSpills;
	RI.IInfo[I - RI.FirstI] |= newSpill << 16;
	return newSpill;
}

static unsigned fregGetSpill(RegInfo& RI, InstLoc I)
{
	return RI.IInfo[I - RI.FirstI] >> 16;
}

static void fregSpill(RegInfo& RI, X64Reg reg)
{
	if (!RI.fregs[reg])
		return;

	unsigned slot = fregGetSpill(RI, RI.fregs[reg]);
	if (!slot)
	{
		slot = fregCreateSpill(RI, RI.fregs[reg]);
		RI.Jit->MOVAPD(fregLocForSlot(RI, slot), reg);
	}

	RI.fregs[reg] = nullptr;
}

// ECX is scratch, so we don't allocate it

// 64-bit - calling conventions differ between linux & windows, so...
#ifdef _WIN32
static const X64Reg RegAllocOrder[] = {RSI, RDI, R12, R13, R14, R8, R9, R10, R11};
#else
static const X64Reg RegAllocOrder[] = {RBP, R12, R13, R14, R8, R9, R10, R11};
#endif
static const int RegAllocSize = sizeof(RegAllocOrder) / sizeof(X64Reg);
static const X64Reg FRegAllocOrder[] = {XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5};
static const int FRegAllocSize = sizeof(FRegAllocOrder) / sizeof(X64Reg);

static X64Reg regFindFreeReg(RegInfo& RI)
{
	for (auto& reg : RegAllocOrder)
	{
		if (RI.regs[reg] == nullptr)
			return reg;
	}

	int bestIndex = -1;
	InstLoc bestEnd = nullptr;
	for (int i = 0; i < RegAllocSize; ++i)
	{
		const InstLoc start = RI.regs[RegAllocOrder[i]];
		const InstLoc end = RI.lastUsed[start - RI.FirstI];

		if (bestEnd < end)
		{
			bestEnd = end;
			bestIndex = i;
		}
	}

	X64Reg reg = RegAllocOrder[bestIndex];
	regSpill(RI, reg);
	return reg;
}

static X64Reg fregFindFreeReg(RegInfo& RI)
{
	for (auto& reg : FRegAllocOrder)
	{
		if (RI.fregs[reg] == nullptr)
			return reg;
	}

	int bestIndex = -1;
	InstLoc bestEnd = nullptr;
	for (int i = 0; i < FRegAllocSize; ++i)
	{
		const InstLoc start = RI.fregs[FRegAllocOrder[i]];
		const InstLoc end = RI.lastUsed[start - RI.FirstI];

		if (bestEnd < end)
		{
			bestEnd = end;
			bestIndex = i;
		}
	}

	X64Reg reg = FRegAllocOrder[bestIndex];
	fregSpill(RI, reg);
	return reg;
}

static OpArg regLocForInst(RegInfo& RI, InstLoc I)
{
	for (auto& reg : RegAllocOrder)
	{
		if (RI.regs[reg] == I)
			return R(reg);
	}

	unsigned slot = regGetSpill(RI, I);
	if (!slot)
		PanicAlert("Retrieving unknown spill slot?!");
	return regLocForSlot(RI, slot);
}

static OpArg fregLocForInst(RegInfo& RI, InstLoc I)
{
	for (auto& reg : FRegAllocOrder)
	{
		if (RI.fregs[reg] == I)
			return R(reg);
	}

	unsigned slot = fregGetSpill(RI, I);
	if (!slot)
		PanicAlert("Retrieving unknown spill slot?!");
	return fregLocForSlot(RI, slot);
}

static void regClearInst(RegInfo& RI, InstLoc I)
{
	for (auto& reg : RegAllocOrder)
	{
		if (RI.regs[reg] == I)
			RI.regs[reg] = nullptr;
	}
}

static void fregClearInst(RegInfo& RI, InstLoc I)
{
	for (auto& reg : FRegAllocOrder)
	{
		if (RI.fregs[reg] == I)
			RI.fregs[reg] = nullptr;
	}
}

static X64Reg regEnsureInReg(RegInfo& RI, InstLoc I)
{
	OpArg loc = regLocForInst(RI, I);

	if (!loc.IsSimpleReg())
	{
		X64Reg newReg = regFindFreeReg(RI);
		RI.Jit->MOV(32, R(newReg), loc);
		loc = R(newReg);
	}

	return loc.GetSimpleReg();
}

static X64Reg fregEnsureInReg(RegInfo& RI, InstLoc I)
{
	OpArg loc = fregLocForInst(RI, I);

	if (!loc.IsSimpleReg())
	{
		X64Reg newReg = fregFindFreeReg(RI);
		RI.Jit->MOVAPD(newReg, loc);
		loc = R(newReg);
	}

	return loc.GetSimpleReg();
}

static void regSpillCallerSaved(RegInfo& RI)
{
	regSpill(RI, RCX);
	regSpill(RI, RDX);
	regSpill(RI, RSI);
	regSpill(RI, RDI);
	regSpill(RI, R8);
	regSpill(RI, R9);
	regSpill(RI, R10);
	regSpill(RI, R11);
}

static X64Reg regUReg(RegInfo& RI, InstLoc I)
{
	const OpArg loc = regLocForInst(RI, getOp1(I));

	if ((RI.IInfo[I - RI.FirstI] & 4) && loc.IsSimpleReg())
	{
		return loc.GetSimpleReg();
	}

	return regFindFreeReg(RI);
}

// Recycle the register if the lifetime of op1 register ends at I.
static X64Reg fregURegWithoutMov(RegInfo& RI, InstLoc I)
{
	const OpArg loc = fregLocForInst(RI, getOp1(I));

	if ((RI.IInfo[I - RI.FirstI] & 4) && loc.IsSimpleReg())
	{
		return loc.GetSimpleReg();
	}

	return fregFindFreeReg(RI);
}

static X64Reg fregURegWithMov(RegInfo& RI, InstLoc I)
{
	const OpArg loc = fregLocForInst(RI, getOp1(I));

	if ((RI.IInfo[I - RI.FirstI] & 4) && loc.IsSimpleReg())
	{
		return loc.GetSimpleReg();
	}

	X64Reg reg = fregFindFreeReg(RI);
	RI.Jit->MOVAPD(reg, loc);
	return reg;
}

// Recycle the register if the lifetime of op1 register ends at I.
static X64Reg fregBinLHSRegWithMov(RegInfo& RI, InstLoc I)
{
	const OpArg loc = fregLocForInst(RI, getOp1(I));

	if ((RI.IInfo[I - RI.FirstI] & 4) && loc.IsSimpleReg())
	{
		return loc.GetSimpleReg();
	}

	X64Reg reg = fregFindFreeReg(RI);
	RI.Jit->MOVAPD(reg, loc);
	return reg;
}

// Recycle the register if the lifetime of op2 register ends at I.
static X64Reg fregBinRHSRegWithMov(RegInfo& RI, InstLoc I)
{
	const OpArg loc = fregLocForInst(RI, getOp2(I));

	if ((RI.IInfo[I - RI.FirstI] & 8) && loc.IsSimpleReg())
	{
		return loc.GetSimpleReg();
	}

	X64Reg reg = fregFindFreeReg(RI);
	RI.Jit->MOVAPD(reg, loc);
	return reg;
}

// If the lifetime of the register used by an operand ends at I,
// return the register. Otherwise return a free register.
static X64Reg regBinReg(RegInfo& RI, InstLoc I)
{
	// FIXME: When regLocForInst() is extracted as a local variable,
	//        "Retrieving unknown spill slot?!" is shown.
	if ((RI.IInfo[I - RI.FirstI] & 4) && regLocForInst(RI, getOp1(I)).IsSimpleReg())
	{
		return regLocForInst(RI, getOp1(I)).GetSimpleReg();
	}
	else if ((RI.IInfo[I - RI.FirstI] & 8) && regLocForInst(RI, getOp2(I)).IsSimpleReg())
	{
		return regLocForInst(RI, getOp2(I)).GetSimpleReg();
	}

	return regFindFreeReg(RI);
}

static X64Reg regBinLHSReg(RegInfo& RI, InstLoc I)
{
	if (RI.IInfo[I - RI.FirstI] & 4)
	{
		return regEnsureInReg(RI, getOp1(I));
	}

	X64Reg reg = regFindFreeReg(RI);
	RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	return reg;
}

static void regNormalRegClear(RegInfo& RI, InstLoc I)
{
	if (RI.IInfo[I - RI.FirstI] & 4)
		regClearInst(RI, getOp1(I));
	if (RI.IInfo[I - RI.FirstI] & 8)
		regClearInst(RI, getOp2(I));
}

static void fregNormalRegClear(RegInfo& RI, InstLoc I)
{
	if (RI.IInfo[I - RI.FirstI] & 4)
		fregClearInst(RI, getOp1(I));
	if (RI.IInfo[I - RI.FirstI] & 8)
		fregClearInst(RI, getOp2(I));
}

static void regEmitBinInst(RegInfo& RI, InstLoc I,
                           void (JitIL::*op)(int, const OpArg&, const OpArg&),
                           bool commutable = false)
{
	X64Reg reg;
	bool commuted = false;
	if (RI.IInfo[I - RI.FirstI] & 4)
	{
		reg = regEnsureInReg(RI, getOp1(I));
	}
	else if (commutable && (RI.IInfo[I - RI.FirstI] & 8))
	{
		reg = regEnsureInReg(RI, getOp2(I));
		commuted = true;
	}
	else
	{
		reg = regFindFreeReg(RI);
		RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	}

	if (isImm(*getOp2(I)))
	{
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		if (RHS + 128 < 256)
		{
			(RI.Jit->*op)(32, R(reg), Imm8(RHS));
		}
		else
		{
			(RI.Jit->*op)(32, R(reg), Imm32(RHS));
		}
	}
	else if (commuted)
	{
		(RI.Jit->*op)(32, R(reg), regLocForInst(RI, getOp1(I)));
	}
	else
	{
		(RI.Jit->*op)(32, R(reg), regLocForInst(RI, getOp2(I)));
	}

	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void fregEmitBinInst(RegInfo& RI, InstLoc I, void (JitIL::*op)(X64Reg, OpArg))
{
	X64Reg reg;

	if (RI.IInfo[I - RI.FirstI] & 4)
	{
		reg = fregEnsureInReg(RI, getOp1(I));
	}
	else
	{
		reg = fregFindFreeReg(RI);
		RI.Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
	}

	(RI.Jit->*op)(reg, fregLocForInst(RI, getOp2(I)));
	RI.fregs[reg] = I;
	fregNormalRegClear(RI, I);
}

// Mark and calculation routines for profiled load/store addresses
// Could be extended to unprofiled addresses.
static void regMarkMemAddress(RegInfo& RI, InstLoc I, InstLoc AI, unsigned OpNum)
{
	if (isImm(*AI))
	{
		unsigned addr = RI.Build->GetImmValue(AI);
		if (Memory::IsRAMAddress(addr))
			return;
	}

	if (getOpcode(*AI) == Add && isImm(*getOp2(AI)))
	{
		regMarkUse(RI, I, getOp1(AI), OpNum);
		return;
	}

	regMarkUse(RI, I, AI, OpNum);
}

// in 64-bit build, this returns a completely bizarre address sometimes!
static std::pair<OpArg, u32> regBuildMemAddress(RegInfo& RI, InstLoc I, InstLoc AI,
                                                unsigned OpNum, unsigned Size, X64Reg* dest)
{
	if (isImm(*AI))
	{
		unsigned addr = RI.Build->GetImmValue(AI);
		if (Memory::IsRAMAddress(addr))
		{
			if (dest)
				*dest = regFindFreeReg(RI);

			return std::make_pair(Imm32(addr), 0);
		}
	}

	unsigned offset;
	InstLoc AddrBase;
	if (getOpcode(*AI) == Add && isImm(*getOp2(AI)))
	{
		offset = RI.Build->GetImmValue(getOp2(AI));
		AddrBase = getOp1(AI);
	}
	else
	{
		offset = 0;
		AddrBase = AI;
	}

	X64Reg baseReg;
	// Ok, this stuff needs a comment or three :P -ector
	if (RI.IInfo[I - RI.FirstI] & (2 << OpNum))
	{
		baseReg = regEnsureInReg(RI, AddrBase);
		regClearInst(RI, AddrBase);
		if (dest)
			*dest = baseReg;
	}
	else if (dest)
	{
		X64Reg reg = regFindFreeReg(RI);
		const OpArg loc = regLocForInst(RI, AddrBase);
		if (!loc.IsSimpleReg())
		{
			RI.Jit->MOV(32, R(reg), loc);
			baseReg = reg;
		}
		else
		{
			baseReg = loc.GetSimpleReg();
		}
		*dest = reg;
	}
	else
	{
		baseReg = regEnsureInReg(RI, AddrBase);
	}

	return std::make_pair(R(baseReg), offset);
}

static void regEmitMemLoad(RegInfo& RI, InstLoc I, unsigned Size)
{
	X64Reg reg;
	auto info = regBuildMemAddress(RI, I, getOp1(I), 1, Size, &reg);

	RI.Jit->SafeLoadToReg(reg, info.first, Size, info.second, regsInUse(RI), false, EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);
	if (regReadUse(RI, I))
		RI.regs[reg] = I;
}

static OpArg regImmForConst(RegInfo& RI, InstLoc I, unsigned Size)
{
	unsigned imm = RI.Build->GetImmValue(I);

	if (Size == 32)
	{
		return Imm32(imm);
	}
	else if (Size == 16)
	{
		return Imm16(imm);
	}
	else
	{
		return Imm8(imm);
	}
}

static void regEmitMemStore(RegInfo& RI, InstLoc I, unsigned Size)
{
	auto info = regBuildMemAddress(RI, I, getOp2(I), 2, Size, nullptr);
	if (info.first.IsImm())
		RI.Jit->MOV(32, R(ECX), info.first);
	else
		RI.Jit->LEA(32, ECX, MDisp(info.first.GetSimpleReg(), info.second));

	regSpill(RI, EAX);

	if (isImm(*getOp1(I)))
	{
		RI.Jit->MOV(Size, R(EAX), regImmForConst(RI, getOp1(I), Size));
	}
	else
	{
		RI.Jit->MOV(32, R(EAX), regLocForInst(RI, getOp1(I)));
	}

	RI.Jit->SafeWriteRegToReg(EAX, ECX, Size, 0, regsInUse(RI), EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);
	if (RI.IInfo[I - RI.FirstI] & 4)
		regClearInst(RI, getOp1(I));
}

static void regEmitShiftInst(RegInfo& RI, InstLoc I, void (JitIL::*op)(int, OpArg, OpArg))
{
	X64Reg reg = regBinLHSReg(RI, I);

	if (isImm(*getOp2(I)))
	{
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		(RI.Jit->*op)(32, R(reg), Imm8(RHS));
		RI.regs[reg] = I;
		return;
	}

	RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
	(RI.Jit->*op)(32, R(reg), R(ECX));
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void regStoreInstToConstLoc(RegInfo& RI, unsigned width, InstLoc I, void* loc)
{
	if (width != 32)
	{
		PanicAlert("Not implemented!");
		return;
	}

	if (isImm(*I))
	{
		RI.Jit->MOV(32, M(loc), Imm32(RI.Build->GetImmValue(I)));
		return;
	}

	X64Reg reg = regEnsureInReg(RI, I);
	RI.Jit->MOV(32, M(loc), R(reg));
}

static void regEmitCmp(RegInfo& RI, InstLoc I)
{
	if (isImm(*getOp2(I)))
	{
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		RI.Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm32(RHS));
	}
	else
	{
		X64Reg reg = regEnsureInReg(RI, getOp1(I));
		RI.Jit->CMP(32, R(reg), regLocForInst(RI, getOp2(I)));
	}
}

static void regEmitICmpInst(RegInfo& RI, InstLoc I, CCFlags flag)
{
	regEmitCmp(RI, I);
	RI.Jit->SETcc(flag, R(ECX)); // Caution: SETCC uses 8-bit regs!
	X64Reg reg = regBinReg(RI, I);
	RI.Jit->MOVZX(32, 8, reg, R(ECX));
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void regEmitICmpCRInst(RegInfo& RI, InstLoc I)
{
	bool signed_compare = getOpcode(*I) == ICmpCRSigned;
	X64Reg reg;

	if (RI.IInfo[I - RI.FirstI] & 4)
	{
		reg = regEnsureInReg(RI, getOp1(I));
		if (signed_compare)
			RI.Jit->MOVSX(64, 32, reg, R(reg));
	}
	else
	{
		reg = regFindFreeReg(RI);
		if (signed_compare)
			RI.Jit->MOVSX(64, 32, reg, regLocForInst(RI, getOp1(I)));
		else
			RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	}

	if (isImm(*getOp2(I)))
	{
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		if (!signed_compare && (RHS & 0x80000000U))
		{
			RI.Jit->MOV(32, R(EAX), Imm32(RHS));
			RI.Jit->SUB(64, R(reg), R(RAX));
		}
		else if (RHS)
		{
			RI.Jit->SUB(64, R(reg), Imm32(RHS));
		}
	}
	else
	{
		if (signed_compare)
			RI.Jit->MOVSX(64, 32, RAX, regLocForInst(RI, getOp2(I)));
		else
			RI.Jit->MOV(32, R(EAX), regLocForInst(RI, getOp2(I)));
		RI.Jit->SUB(64, R(reg), R(RAX));
	}

	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void regWriteExit(RegInfo& RI, InstLoc dest)
{
	if (isImm(*dest))
	{
		RI.exitNumber++;
		RI.Jit->WriteExit(RI.Build->GetImmValue(dest));
	}
	else
	{
		RI.Jit->WriteExitDestInOpArg(regLocForInst(RI, dest));
	}
}

// Helper function to check floating point exceptions
static double GC_ALIGNED16(isSNANTemp[2][2]);
static bool checkIsSNAN()
{
	return MathUtil::IsSNAN(isSNANTemp[0][0]) || MathUtil::IsSNAN(isSNANTemp[1][0]);
}

static void DoWriteCode(IRBuilder* ibuild, JitIL* Jit, u32 exitAddress)
{
	//printf("Writing block: %x\n", js.blockStart);
	RegInfo RI(Jit, ibuild->getFirstInst(), ibuild->getNumInsts());
	RI.Build = ibuild;

	// Pass to compute liveness
	ibuild->StartBackPass();
	for (unsigned int index = (unsigned int)RI.IInfo.size() - 1; index != -1U; --index)
	{
		InstLoc I = ibuild->ReadBackward();
		unsigned int op = getOpcode(*I);
		bool thisUsed = regReadUse(RI, I) ? true : false;

		switch (op)
		{
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
		case ConvertFromFastCR:
		case ConvertToFastCR:
		case FastCRSOSet:
		case FastCREQSet:
		case FastCRGTSet:
		case FastCRLTSet:
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
			if (thisUsed)
			{
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
		case BranchCond:
		{
			if (isICmp(*getOp1(I)))
			{
				regMarkUse(RI, I, getOp1(getOp1(I)), 1);
				if (!isImm(*getOp2(getOp1(I))))
					regMarkUse(RI, I, getOp2(getOp1(I)), 2);
			}
			else
			{
				regMarkUse(RI, I, getOp1(I), 1);
			}
			if (!isImm(*getOp2(I)))
				regMarkUse(RI, I, getOp2(I), 2);
			break;
		}
		}
	}

	ibuild->StartForwardPass();
	for (unsigned i = 0; i != RI.IInfo.size(); i++)
	{
		InstLoc I = ibuild->ReadForward();
		bool thisUsed = regReadUse(RI, I) ? true : false;
		if (thisUsed)
		{
			// Needed for IR Writer
			ibuild->SetMarkUsed(I);
		}

		switch (getOpcode(*I))
		{
		case FallBackToInterpreter:
		{
			unsigned InstCode = ibuild->GetImmValue(getOp1(I));
			unsigned InstLoc = ibuild->GetImmValue(getOp2(I));
			// There really shouldn't be anything live across an
			// interpreter call at the moment, but optimizing interpreter
			// calls isn't completely out of the question...
			regSpillCallerSaved(RI);
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->MOV(32, M(&NPC), Imm32(InstLoc+4));
			Jit->ABI_CallFunctionC((void*)GetInterpreterOp(InstCode),
					       InstCode);
			break;
		}
		case LoadGReg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOV(32, R(reg), M(&PowerPC::ppcState.gpr[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCR:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOV(64, R(reg), M(&PowerPC::ppcState.cr_val[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCTR:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&CTR));
			RI.regs[reg] = I;
			break;
		}
		case LoadLink:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&LR));
			RI.regs[reg] = I;
			break;
		}
		case LoadMSR:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&MSR));
			RI.regs[reg] = I;
			break;
		}
		case LoadGQR:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			unsigned gqr = *I >> 8;
			Jit->MOV(32, R(reg), M(&GQR(gqr)));
			RI.regs[reg] = I;
			break;
		}
		case LoadCarry:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&PowerPC::ppcState.spr[SPR_XER]));
			Jit->SHR(32, R(reg), Imm8(29));
			Jit->AND(32, R(reg), Imm8(1));
			RI.regs[reg] = I;
			break;
		}
		case StoreGReg:
		{
			unsigned ppcreg = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I),
					       &PowerPC::ppcState.gpr[ppcreg]);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCR:
		{
			X64Reg reg = regEnsureInReg(RI, getOp1(I));
			unsigned ppcreg = *I >> 16;
			Jit->MOV(64, M(&PowerPC::ppcState.cr_val[ppcreg]), R(reg));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreLink:
		{
			regStoreInstToConstLoc(RI, 32, getOp1(I), &LR);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCTR:
		{
			regStoreInstToConstLoc(RI, 32, getOp1(I), &CTR);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreMSR:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp2(I));
			regStoreInstToConstLoc(RI, 32, getOp1(I), &MSR);
			regNormalRegClear(RI, I);

			// If some exceptions are pending and EE are now enabled, force checking
			// external exceptions when going out of mtmsr in order to execute delayed
			// interrupts as soon as possible.
			Jit->MOV(32, R(EAX), M(&MSR));
			Jit->TEST(32, R(EAX), Imm32(0x8000));
			FixupBranch eeDisabled = Jit->J_CC(CC_Z);

			Jit->MOV(32, R(EAX), M((void*)&PowerPC::ppcState.Exceptions));
			Jit->TEST(32, R(EAX), R(EAX));
			FixupBranch noExceptionsPending = Jit->J_CC(CC_Z);

			Jit->MOV(32, M(&PC), Imm32(InstLoc + 4));
			Jit->WriteExceptionExit(); // TODO: Implement WriteExternalExceptionExit for JitIL

			Jit->SetJumpTarget(eeDisabled);
			Jit->SetJumpTarget(noExceptionsPending);
			break;
		}
		case StoreGQR:
		{
			unsigned gqr = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I), &GQR(gqr));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreSRR:
		{
			unsigned srr = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I),
					&PowerPC::ppcState.spr[SPR_SRR0+srr]);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCarry:
		{
			Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm8(0));
			FixupBranch nocarry = Jit->J_CC(CC_Z);
			Jit->JitSetCA();
			FixupBranch cont = Jit->J();
			Jit->SetJumpTarget(nocarry);
			Jit->JitClearCAOV(false);
			Jit->SetJumpTarget(cont);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreFPRF:
		{
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->AND(32, R(ECX), Imm8(0x1F));
			Jit->SHL(32, R(ECX), Imm8(12));
			Jit->AND(32, M(&FPSCR), Imm32(~(0x1F << 12)));
			Jit->OR(32, M(&FPSCR), R(ECX));
			regNormalRegClear(RI, I);
			break;
		}
		case Load8:
		{
			regEmitMemLoad(RI, I, 8);
			break;
		}
		case Load16:
		{
			regEmitMemLoad(RI, I, 16);
			break;
		}
		case Load32:
		{
			regEmitMemLoad(RI, I, 32);
			break;
		}
		case Store8:
		{
			regEmitMemStore(RI, I, 8);
			break;
		}
		case Store16:
		{
			regEmitMemStore(RI, I, 16);
			break;
		}
		case Store32:
		{
			regEmitMemStore(RI, I, 32);
			break;
		}
		case SExt8:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->MOVSX(32, 8, reg, R(ECX));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case SExt16:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->MOVSX(32, 16, reg, regLocForInst(RI, getOp1(I)));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Cntlzw:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->MOV(32, R(ECX), Imm32(63));
			Jit->BSR(32, reg, regLocForInst(RI, getOp1(I)));
			Jit->CMOVcc(32, reg, R(ECX), CC_Z);
			Jit->XOR(32, R(reg), Imm8(31));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Not:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regBinLHSReg(RI, I);
			Jit->NOT(32, R(reg));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case And:
		{
			if (!thisUsed)
				break;

			regEmitBinInst(RI, I, &JitIL::AND, true);
			break;
		}
		case Xor:
		{
			if (!thisUsed)
				break;

			regEmitBinInst(RI, I, &JitIL::XOR, true);
			break;
		}
		case Sub:
		{
			if (!thisUsed)
				break;

			regEmitBinInst(RI, I, &JitIL::SUB);
			break;
		}
		case Or:
		{
			if (!thisUsed)
				break;

			regEmitBinInst(RI, I, &JitIL::OR, true);
			break;
		}
		case Add:
		{
			if (!thisUsed)
				break;

			regEmitBinInst(RI, I, &JitIL::ADD, true);
			break;
		}
		case Mul:
		{
			if (!thisUsed)
				break;

			// FIXME: Use three-address capability of IMUL!
			X64Reg reg = regBinLHSReg(RI, I);
			if (isImm(*getOp2(I)))
			{
				unsigned RHS = RI.Build->GetImmValue(getOp2(I));
				if (RHS + 128 < 256)
					Jit->IMUL(32, reg, Imm8(RHS));
				else
					Jit->IMUL(32, reg, Imm32(RHS));
			}
			else
			{
				Jit->IMUL(32, reg, regLocForInst(RI, getOp2(I)));
			}
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case MulHighUnsigned:
		{
			if (!thisUsed)
				break;

			regSpill(RI, EAX);
			regSpill(RI, EDX);
			X64Reg reg = regBinReg(RI, I);
			if (isImm(*getOp2(I)))
			{
				unsigned RHS = RI.Build->GetImmValue(getOp2(I));
				Jit->MOV(32, R(EAX), Imm32(RHS));
			}
			else
			{
				Jit->MOV(32, R(EAX), regLocForInst(RI, getOp2(I)));
			}
			Jit->MUL(32, regLocForInst(RI, getOp1(I)));
			Jit->MOV(32, R(reg), R(EDX));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Rol:
		{
			if (!thisUsed)
				break;

			regEmitShiftInst(RI, I, &JitIL::ROL);
			break;
		}
		case Shl:
		{
			if (!thisUsed)
				break;

			regEmitShiftInst(RI, I, &JitIL::SHL);
			break;
		}
		case Shrl:
		{
			if (!thisUsed)
				break;

			regEmitShiftInst(RI, I, &JitIL::SHR);
			break;
		}
		case Sarl:
		{
			if (!thisUsed)
				break;

			regEmitShiftInst(RI, I, &JitIL::SAR);
			break;
		}
		case ICmpEq:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_E);
			break;
		}
		case ICmpNe:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_NE);
			break;
		}
		case ICmpUgt:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_A);
			break;
		}
		case ICmpUlt:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_B);
			break;
		}
		case ICmpUge:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_AE);
			break;
		}
		case ICmpUle:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_BE);
			break;
		}
		case ICmpSgt:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_G);
			break;
		}
		case ICmpSlt:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_L);
			break;
		}
		case ICmpSge:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_GE);
			break;
		}
		case ICmpSle:
		{
			if (!thisUsed)
				break;

			regEmitICmpInst(RI, I, CC_LE);
			break;
		}
		case ICmpCRUnsigned:
		{
			if (!thisUsed)
				break;

			regEmitICmpCRInst(RI, I);
			break;
		}
		case ICmpCRSigned:
		{
			if (!thisUsed)
				break;

			regEmitICmpCRInst(RI, I);
			break;
		}
		case ConvertFromFastCR:
		{
			if (!thisUsed)
				break;

			X64Reg cr_val = regUReg(RI, I);
			Jit->MOV(64, R(cr_val), regLocForInst(RI, getOp1(I)));

			Jit->XOR(32, R(EAX), R(EAX));

			// SO: Bit 61 set.
			Jit->MOV(64, R(RCX), R(cr_val));
			Jit->SHR(64, R(RCX), Imm8(61));
			Jit->AND(32, R(ECX), Imm8(1));
			Jit->OR(32, R(EAX), R(ECX));

			// EQ: Bits 31-0 == 0.
			Jit->XOR(32, R(ECX), R(ECX));
			Jit->TEST(32, R(cr_val), R(cr_val));
			Jit->SETcc(CC_Z, R(ECX));
			Jit->SHL(32, R(ECX), Imm8(1));
			Jit->OR(32, R(EAX), R(ECX));

			// GT: Value > 0.
			Jit->XOR(32, R(ECX), R(ECX));
			Jit->TEST(64, R(cr_val), R(cr_val));
			Jit->SETcc(CC_G, R(ECX));
			Jit->SHL(32, R(ECX), Imm8(2));
			Jit->OR(32, R(EAX), R(ECX));

			// LT: Bit 62 set.
			Jit->MOV(64, R(ECX), R(cr_val));
			Jit->SHR(64, R(ECX), Imm8(62 - 3));
			Jit->AND(32, R(ECX), Imm8(0x8));
			Jit->OR(32, R(EAX), R(ECX));

			Jit->MOV(32, R(cr_val), R(EAX));
			RI.regs[cr_val] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case ConvertToFastCR:
		{
			if (!thisUsed)
				break;

			X64Reg cr_val = regUReg(RI, I);
			Jit->MOV(64, R(cr_val), regLocForInst(RI, getOp1(I)));

			Jit->MOV(64, R(RCX), Imm64(1ull << 32));

			// SO
			Jit->MOV(64, R(RAX), R(cr_val));
			Jit->SHL(64, R(RAX), Imm8(63));
			Jit->SHR(64, R(RAX), Imm8(63 - 61));
			Jit->OR(64, R(RCX), R(RAX));

			// EQ
			Jit->MOV(64, R(RAX), R(cr_val));
			Jit->NOT(64, R(RAX));
			Jit->AND(64, R(RAX), Imm8(CR_EQ));
			Jit->OR(64, R(RCX), R(RAX));

			// GT
			Jit->MOV(64, R(RAX), R(cr_val));
			Jit->NOT(64, R(RAX));
			Jit->AND(64, R(RAX), Imm8(CR_GT));
			Jit->SHL(64, R(RAX), Imm8(63 - 2));
			Jit->OR(64, R(RCX), R(RAX));

			// LT
			Jit->MOV(64, R(RAX), R(cr_val));
			Jit->AND(64, R(RAX), Imm8(CR_LT));
			Jit->SHL(64, R(RAX), Imm8(62 - 3));
			Jit->OR(64, R(RCX), R(RAX));

			Jit->MOV(64, R(cr_val), R(RCX));

			RI.regs[cr_val] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case FastCRSOSet:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->MOV(64, R(RAX), Imm64(1ull << 61));
			Jit->TEST(64, regLocForInst(RI, getOp1(I)), R(RAX));
			Jit->SETcc(CC_NZ, R(AL));
			Jit->MOVZX(32, 8, reg, R(AL));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case FastCREQSet:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm32(0));
			Jit->SETcc(CC_Z, R(AL));
			Jit->MOVZX(32, 8, reg, R(AL));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case FastCRGTSet:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->CMP(64, regLocForInst(RI, getOp1(I)), Imm8(0));
			Jit->SETcc(CC_G, R(AL));
			Jit->MOVZX(32, 8, reg, R(AL));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case FastCRLTSet:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regUReg(RI, I);
			Jit->MOV(64, R(RAX), Imm64(1ull << 62));
			Jit->TEST(64, regLocForInst(RI, getOp1(I)), R(RAX));
			Jit->SETcc(CC_NZ, R(AL));
			Jit->MOVZX(32, 8, reg, R(AL));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadSingle:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			RI.Jit->SafeLoadToReg(ECX, R(ECX), 32, 0, regsInUse(RI), false, EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);
			Jit->MOVD_xmm(reg, R(ECX));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadDouble:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregFindFreeReg(RI);
			const OpArg loc = regLocForInst(RI, getOp1(I));
			Jit->MOV(32, R(ECX), loc);
			RI.Jit->SafeLoadToReg(RCX, R(ECX), 64, 0, regsInUse(RI), false, EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);
			Jit->MOVQ_xmm(reg, R(RCX));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadPaired:
		{
			if (!thisUsed)
				break;

			regSpill(RI, EAX);
			regSpill(RI, EDX);
			X64Reg reg = fregFindFreeReg(RI);
			// The lower 3 bits is for GQR index. The next 1 bit is for inst.W
			unsigned int quantreg = (*I >> 16) & 0x7;
			unsigned int w = *I >> 19;
			// Some games (e.g. Dirt 2) incorrectly set the unused bits which breaks the lookup table code.
			// Hence, we need to mask out the unused bits. The layout of the GQR register is
			// UU[SCALE]UUUUU[TYPE] where SCALE is 6 bits and TYPE is 3 bits, so we have to AND with
			// 0b0011111100000111, or 0x3F07.
			Jit->MOV(32, R(EAX), Imm32(0x3F07));
			Jit->AND(32, R(EAX), M(((char *)&GQR(quantreg)) + 2));
			Jit->MOVZX(32, 8, EDX, R(AL));
			Jit->OR(32, R(EDX), Imm8(w << 3));

			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MScaled(EDX, SCALE_8, (u32)(u64)(((JitIL *)jit)->asm_routines.pairedLoadQuantized)));
			Jit->MOVAPD(reg, R(XMM0));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case StoreSingle:
		{
			regSpill(RI, EAX);
			const OpArg loc1 = fregLocForInst(RI, getOp1(I));
			if (loc1.IsSimpleReg())
				Jit->MOVD_xmm(R(EAX), loc1.GetSimpleReg());
			else
				Jit->MOV(32, R(EAX), loc1);

			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			RI.Jit->SafeWriteRegToReg(EAX, ECX, 32, 0, regsInUse(RI), EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);
			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case StoreDouble:
		{
			regSpill(RI, EAX);

			OpArg value = fregLocForInst(RI, getOp1(I));
			OpArg address = regLocForInst(RI, getOp2(I));
			Jit->MOVAPD(XMM0, value);
			Jit->MOVQ_xmm(R(RAX), XMM0);
			Jit->MOV(32, R(ECX), address);
			RI.Jit->SafeWriteRegToReg(RAX, ECX, 64, 0, regsInUse(RI), EmuCodeBlock::SAFE_LOADSTORE_NO_FASTMEM);

			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case StorePaired:
		{
			regSpill(RI, EAX);
			regSpill(RI, EDX);
			u32 quantreg = *I >> 24;
			Jit->MOV(32, R(EAX), Imm32(0x3F07));
			Jit->AND(32, R(EAX), M(&PowerPC::ppcState.spr[SPR_GQR0 + quantreg]));
			Jit->MOVZX(32, 8, EDX, R(AL));

			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MScaled(EDX, SCALE_8, (u32)(u64)(((JitIL *)jit)->asm_routines.pairedStoreQuantized)));
			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case DupSingleToMReg:
		{
			if (!thisUsed) break;

			X64Reg input = fregEnsureInReg(RI, getOp1(I));
			X64Reg output = fregURegWithoutMov(RI, I);
			Jit->ConvertSingleToDouble(output, input);

			RI.fregs[output] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case InsertDoubleInMReg:
		{
			if (!thisUsed) break;
			// r[0] = op1[0]; r[1] = op2[1];

			// TODO: Optimize the case that the register of op1 can be
			//       recycled. (SHUFPD may not be so fast.)
			X64Reg reg = fregBinRHSRegWithMov(RI, I);
			OpArg loc1 = fregLocForInst(RI, getOp1(I));
			if (loc1.IsSimpleReg())
			{
				Jit->MOVSD(reg, loc1);
			}
			else
			{
				// If op1 is in FSlotSet, we have to mov loc1 to XMM0
				// before MOVSD/MOVSS.
				// Because register<->memory transfer with MOVSD/MOVSS
				// clears upper 64/96-bits of the destination register.
				Jit->MOVAPD(XMM0, loc1);
				Jit->MOVSD(reg, R(XMM0));
			}
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case ExpandPackedToMReg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithoutMov(RI, I);
			Jit->CVTPS2PD(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case CompactMRegToPacked:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithoutMov(RI, I);
			Jit->CVTPD2PS(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FSNeg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithMov(RI, I);
			static const u32 GC_ALIGNED16(ssSignBits[4]) = {0x80000000};
			Jit->PXOR(reg, M((void*)&ssSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FDNeg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithMov(RI, I);
			static const u64 GC_ALIGNED16(sdSignBits[2]) = {0x8000000000000000ULL};
			Jit->PXOR(reg, M((void*)&sdSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPNeg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithMov(RI, I);
			static const u32 GC_ALIGNED16(psSignBits[4]) = {0x80000000, 0x80000000};
			Jit->PXOR(reg, M((void*)&psSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPDup0:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithMov(RI, I);
			Jit->PUNPCKLDQ(reg, R(reg));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPDup1:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregURegWithMov(RI, I);
			Jit->SHUFPS(reg, R(reg), 0xE5);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case LoadFReg:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOVAPD(reg, M(&PowerPC::ppcState.ps[ppcreg]));
			RI.fregs[reg] = I;
			break;
		}
		case LoadFRegDENToZero:
		{
			if (!thisUsed)
				break;

			X64Reg reg = fregFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			char *p = (char*)&(PowerPC::ppcState.ps[ppcreg][0]);
			Jit->MOV(32, R(ECX), M(p+4));
			Jit->AND(32, R(ECX), Imm32(0x7ff00000));
			Jit->CMP(32, R(ECX), Imm32(0x38000000));
			FixupBranch ok = Jit->J_CC(CC_AE);
			Jit->AND(32, M(p+4), Imm32(0x80000000));
			Jit->MOV(32, M(p), Imm32(0));
			Jit->SetJumpTarget(ok);
			Jit->MOVAPD(reg, M(&PowerPC::ppcState.ps[ppcreg]));
			RI.fregs[reg] = I;
			break;
		}
		case StoreFReg:
		{
			unsigned ppcreg = *I >> 16;
			Jit->MOVAPD(M(&PowerPC::ppcState.ps[ppcreg]),
				      fregEnsureInReg(RI, getOp1(I)));
			fregNormalRegClear(RI, I);
			break;
		}
		case DoubleToSingle:
		{
			if (!thisUsed)
				break;

			X64Reg input = fregEnsureInReg(RI, getOp1(I));
			X64Reg output = fregURegWithoutMov(RI, I);
			Jit->ConvertDoubleToSingle(output, input);

			RI.fregs[output] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FSMul:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::MULSS);
			break;
		}
		case FSAdd:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::ADDSS);
			break;
		}
		case FSSub:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::SUBSS);
			break;
		}
		case FDMul:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::MULSD);
			break;
		}
		case FDAdd:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::ADDSD);
			break;
		}
		case FDSub:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::SUBSD);
			break;
		}
		case FDCmpCR:
		{
			const u32 ordered = *I >> 24;
			X64Reg destreg = regFindFreeReg(RI);
			// TODO: Remove an extra MOVSD if loc1.IsSimpleReg()
			OpArg loc1 = fregLocForInst(RI, getOp1(I));
			OpArg loc2 = fregLocForInst(RI, getOp2(I));
			Jit->MOVSD(XMM0, loc1);
			Jit->UCOMISD(XMM0, loc2);
			FixupBranch pNan     = Jit->J_CC(CC_P);
			FixupBranch pEqual   = Jit->J_CC(CC_Z);
			FixupBranch pLesser  = Jit->J_CC(CC_C);
			// Greater
			Jit->MOV(32, R(destreg), Imm32(0x4));
			FixupBranch continue1 = Jit->J();
			// NaN
			Jit->SetJumpTarget(pNan);
			Jit->MOV(32, R(destreg), Imm32(0x1));

			static const u32 FPSCR_VE = (u32)1 << (31 - 24);
			static const u32 FPSCR_VXVC = (u32)1 << (31 - 12);
			static const u32 FPSCR_VXSNAN = (u32)1 << (31 - 7);
			static const u32 FPSCR_FX = (u32)1 << (31 - 0);

			if (ordered)
			{
				// fcmpo
				// TODO: Optimize the following code if slow.
				//       SNAN check may not be needed
				//       because it does not happen so much.
				Jit->MOVSD(M(isSNANTemp[0]), XMM0);
				if (loc2.IsSimpleReg())
				{
					Jit->MOVSD(M(isSNANTemp[1]), loc2.GetSimpleReg());
				}
				else
				{
					Jit->MOVSD(XMM0, loc2);
					Jit->MOVSD(M(isSNANTemp[1]), XMM0);
				}
				Jit->ABI_CallFunction((void*)checkIsSNAN);
				Jit->TEST(8, R(EAX), R(EAX));
				FixupBranch ok = Jit->J_CC(CC_Z);
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_FX)); // FPSCR.FX = 1;
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_VXSNAN)); // FPSCR.Hex |= mask;
					Jit->TEST(32, M(&FPSCR), Imm32(FPSCR_VE));
					FixupBranch finish0 = Jit->J_CC(CC_NZ);
						Jit->OR(32, M(&FPSCR), Imm32(FPSCR_VXVC)); // FPSCR.Hex |= mask;
						FixupBranch finish1 = Jit->J();
				Jit->SetJumpTarget(ok);
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_FX)); // FPSCR.FX = 1;
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_VXVC)); // FPSCR.Hex |= mask;
				Jit->SetJumpTarget(finish0);
				Jit->SetJumpTarget(finish1);
			}
			else
			{
				// fcmpu
				// TODO: Optimize the following code if slow
				Jit->MOVSD(M(isSNANTemp[0]), XMM0);
				if (loc2.IsSimpleReg())
				{
					Jit->MOVSD(M(isSNANTemp[1]), loc2.GetSimpleReg());
				}
				else
				{
					Jit->MOVSD(XMM0, loc2);
					Jit->MOVSD(M(isSNANTemp[1]), XMM0);
				}
				Jit->ABI_CallFunction((void*)checkIsSNAN);
				Jit->TEST(8, R(EAX), R(EAX));
				FixupBranch finish = Jit->J_CC(CC_Z);
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_FX)); // FPSCR.FX = 1;
					Jit->OR(32, M(&FPSCR), Imm32(FPSCR_VXVC)); // FPSCR.Hex |= mask;
				Jit->SetJumpTarget(finish);
			}

			FixupBranch continue2 = Jit->J();
			// Equal
			Jit->SetJumpTarget(pEqual);
			Jit->MOV(32, R(destreg), Imm32(0x2));
			FixupBranch continue3 = Jit->J();
			// Less
			Jit->SetJumpTarget(pLesser);
			Jit->MOV(32, R(destreg), Imm32(0x8));
			Jit->SetJumpTarget(continue1);
			Jit->SetJumpTarget(continue2);
			Jit->SetJumpTarget(continue3);
			RI.regs[destreg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPAdd:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::ADDPS);
			break;
		}
		case FPMul:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::MULPS);
			break;
		}
		case FPSub:
		{
			if (!thisUsed)
				break;

			fregEmitBinInst(RI, I, &JitIL::SUBPS);
			break;
		}
		case FPMerge00:
		{
			// r[0] = op1[0]; r[1] = op2[0];
			if (!thisUsed)
				break;

			// TODO: Optimize the case that the register of only op2 can be
			//       recycled.
			X64Reg reg = fregBinLHSRegWithMov(RI, I);
			Jit->PUNPCKLDQ(reg, fregLocForInst(RI, getOp2(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge01:
		{
			// r[0] = op1[0]; r[1] = op2[1];
			if (!thisUsed)
				break;

			// TODO: Optimize the case that the register of only op1 can be
			//       recycled.
			X64Reg reg = fregBinRHSRegWithMov(RI, I);
			OpArg loc1 = fregLocForInst(RI, getOp1(I));
			if (loc1.IsSimpleReg())
			{
				Jit->MOVSS(reg, loc1);
			}
			else
			{
				Jit->MOVAPD(XMM0, loc1);
				Jit->MOVSS(reg, R(XMM0));
			}
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge10:
		{
			// r[0] = op1[1]; r[1] = op2[0];
			if (!thisUsed)
				break;

			// TODO: Optimize the case that the register of only op2 can be
			//       recycled.
			X64Reg reg = fregBinLHSRegWithMov(RI, I);
			OpArg loc2 = fregLocForInst(RI, getOp2(I));
			if (loc2.IsSimpleReg())
			{
				Jit->MOVSS(reg, loc2);
			}
			else
			{
				Jit->MOVAPD(XMM0, loc2);
				Jit->MOVSS(reg, R(XMM0));
			}
			Jit->SHUFPS(reg, R(reg), 0xF1);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge11:
		{
			// r[0] = op1[1]; r[1] = op2[1];
			if (!thisUsed)
				break;

			// TODO: Optimize the case that the register of only op2 can be
			//       recycled.
			X64Reg reg = fregBinLHSRegWithMov(RI, I);
			// TODO: Check whether the following code works
			//       when the op1 is in the FSlotSet
			Jit->PUNPCKLDQ(reg, fregLocForInst(RI, getOp2(I)));
			Jit->SHUFPD(reg, R(reg), 0x1);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case CInt32:
		case CInt16:
		{
			if (!thisUsed)
				break;

			X64Reg reg = regFindFreeReg(RI);
			u64 val = ibuild->GetImmValue64(I);
			if ((u32)val == val)
				Jit->MOV(32, R(reg), Imm32((u32)val));
			else if ((s32)val == (s64)val)
				Jit->MOV(64, R(reg), Imm32((s32)val));
			else
				Jit->MOV(64, R(reg), Imm64(val));
			RI.regs[reg] = I;
			break;
		}
		case BlockStart:
		case BlockEnd:
			break;

		case IdleBranch:
		{
			Jit->CMP(32, regLocForInst(RI, getOp1(getOp1(I))),
					 Imm32(RI.Build->GetImmValue(getOp2(getOp1(I)))));
			FixupBranch cont = Jit->J_CC(CC_NE);

			RI.Jit->Cleanup(); // is it needed?
			Jit->ABI_CallFunction((void *)&PowerPC::OnIdleIL);

			Jit->MOV(32, M(&PC), Imm32(ibuild->GetImmValue( getOp2(I) )));
			Jit->WriteExceptionExit();

			Jit->SetJumpTarget(cont);
			if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(getOp1(I)));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}

		case BranchCond:
		{
			if (isICmp(*getOp1(I)))
			{
				regEmitCmp(RI, getOp1(I));
				CCFlags flag;
				switch (getOpcode(*getOp1(I)))
				{
					case ICmpEq:
						flag = CC_NE;
						break;
					case ICmpNe:
						flag = CC_E;
						break;
					case ICmpUgt:
						flag = CC_BE;
						break;
					case ICmpUlt:
						flag = CC_AE;
						break;
					case ICmpUge:
						flag = CC_B;
						break;
					case ICmpUle:
						flag = CC_A;
						break;
					case ICmpSgt:
						flag = CC_LE;
						break;
					case ICmpSlt:
						flag = CC_GE;
						break;
					case ICmpSge:
						flag = CC_L;
						break;
					case ICmpSle:
						flag = CC_G;
						break;
					default:
						PanicAlert("cmpXX");
						flag = CC_O;
						break;
				}
				FixupBranch cont = Jit->J_CC(flag);
				regWriteExit(RI, getOp2(I));
				Jit->SetJumpTarget(cont);
				if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(getOp1(I)));
				if (RI.IInfo[I - RI.FirstI] & 8)
					regClearInst(RI, getOp2(getOp1(I)));
			}
			else
			{
				Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm8(0));
				FixupBranch cont = Jit->J_CC(CC_Z);
				regWriteExit(RI, getOp2(I));
				Jit->SetJumpTarget(cont);
				if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(I));
			}
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case BranchUncond:
		{
			regWriteExit(RI, getOp1(I));
			regNormalRegClear(RI, I);
			break;
		}
		case ShortIdleLoop:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->ABI_CallFunction((void *)&CoreTiming::Idle);
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->WriteExceptionExit();
			break;
		}
		case SystemCall:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->LOCK();
			Jit->OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_SYSCALL));
			Jit->MOV(32, M(&PC), Imm32(InstLoc + 4));
			Jit->WriteExceptionExit();
			break;
		}
		case InterpreterBranch:
		{
			Jit->MOV(32, R(EAX), M(&NPC));
			Jit->WriteExitDestInOpArg(R(EAX));
			break;
		}
		case RFIExit:
		{
			// See Interpreter rfi for details
			const u32 mask = 0x87C0FFFF;
			// MSR = (MSR & ~mask) | (SRR1 & mask);
			Jit->MOV(32, R(EAX), M(&MSR));
			Jit->MOV(32, R(ECX), M(&SRR1));
			Jit->AND(32, R(EAX), Imm32(~mask));
			Jit->AND(32, R(ECX), Imm32(mask));
			Jit->OR(32, R(EAX), R(ECX));
			// MSR &= 0xFFFBFFFF; // Mask used to clear the bit MSR[13]
			Jit->AND(32, R(EAX), Imm32(0xFFFBFFFF));
			Jit->MOV(32, M(&MSR), R(EAX));
			// NPC = SRR0;
			Jit->MOV(32, R(EAX), M(&SRR0));
			Jit->WriteRfiExitDestInOpArg(R(EAX));
			break;
		}
		case FPExceptionCheck:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			//This instruction uses FPU - needs to add FP exception bailout
			Jit->TEST(32, M(&PowerPC::ppcState.msr), Imm32(1 << 13)); // Test FP enabled bit
			FixupBranch b1 = Jit->J_CC(CC_NZ);

			// If a FPU exception occurs, the exception handler will read
			// from PC.  Update PC with the latest value in case that happens.
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->SUB(32, M(&PowerPC::ppcState.downcount), Imm32(Jit->js.downcountAmount));
			Jit->OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_FPU_UNAVAILABLE));
			Jit->WriteExceptionExit();
			Jit->SetJumpTarget(b1);
			break;
		}
		case DSIExceptionCheck:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_DSI));
			FixupBranch noMemException = Jit->J_CC(CC_Z);

			// If a memory exception occurs, the exception handler will read
			// from PC.  Update PC with the latest value in case that happens.
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->WriteExceptionExit();
			Jit->SetJumpTarget(noMemException);
			break;
		}
		case ISIException:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));

			// Address of instruction could not be translated
			Jit->MOV(32, M(&NPC), Imm32(InstLoc));
			Jit->OR(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_ISI));

			// Remove the invalid instruction from the icache, forcing a recompile
			Jit->MOV(64, R(RAX), ImmPtr(jit->GetBlockCache()->GetICachePtr(InstLoc)));
			Jit->MOV(32, MatR(RAX), Imm32(JIT_ICACHE_INVALID_WORD));
			Jit->WriteExceptionExit();
			break;
		}
		case ExtExceptionCheck:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));

			Jit->TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_ISI | EXCEPTION_PROGRAM | EXCEPTION_SYSCALL | EXCEPTION_FPU_UNAVAILABLE | EXCEPTION_DSI | EXCEPTION_ALIGNMENT));
			FixupBranch clearInt = Jit->J_CC(CC_NZ);
			Jit->TEST(32, M((void *)&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_EXTERNAL_INT));
			FixupBranch noExtException = Jit->J_CC(CC_Z);
			Jit->TEST(32, M((void *)&PowerPC::ppcState.msr), Imm32(0x0008000));
			FixupBranch noExtIntEnable = Jit->J_CC(CC_Z);
			Jit->TEST(32, M((void *)&ProcessorInterface::m_InterruptCause), Imm32(ProcessorInterface::INT_CAUSE_CP | ProcessorInterface::INT_CAUSE_PE_TOKEN | ProcessorInterface::INT_CAUSE_PE_FINISH));
			FixupBranch noCPInt = Jit->J_CC(CC_Z);

			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->WriteExceptionExit();

			Jit->SetJumpTarget(noCPInt);
			Jit->SetJumpTarget(noExtIntEnable);
			Jit->SetJumpTarget(noExtException);
			Jit->SetJumpTarget(clearInt);
			break;
		}
		case BreakPointCheck:
		{
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));

			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->ABI_CallFunction(reinterpret_cast<void *>(&PowerPC::CheckBreakPoints));
			Jit->TEST(32, M((void*)PowerPC::GetStatePtr()), Imm32(0xFFFFFFFF));
			FixupBranch noBreakpoint = Jit->J_CC(CC_Z);
			Jit->WriteExit(InstLoc);
			Jit->SetJumpTarget(noBreakpoint);
			break;
		}
		case Int3:
		{
			Jit->INT3();
			break;
		}
		case Tramp: break;
		case Nop: break;
		default:
			PanicAlert("Unknown JIT instruction; aborting!");
			exit(1);
		}
	}

	for (unsigned i = 0; i < MAX_NUMBER_OF_REGS; i++)
	{
		if (RI.regs[i])
		{
			// Start a game in Burnout 2 to get this. Or animal crossing.
			PanicAlert("Incomplete cleanup! (regs)");
			exit(1);
		}

		if (RI.fregs[i])
		{
			PanicAlert("Incomplete cleanup! (fregs)");
			exit(1);
		}
	}

	Jit->WriteExit(exitAddress);
	Jit->UD2();
}

void JitIL::WriteCode(u32 exitAddress)
{
	DoWriteCode(&ibuild, this, exitAddress);
}
