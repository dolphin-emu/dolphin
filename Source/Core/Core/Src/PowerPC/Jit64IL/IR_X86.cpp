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

The register allocation is just a simple forward greedy allocator.
*/

#ifdef _MSC_VER
#pragma warning(disable:4146)   // unary minus operator applied to unsigned type, result still unsigned
#endif

#include "IR.h"
#include "../PPCTables.h"
#include "../../CoreTiming.h"
#include "Thunk.h"
#include "../../HW/Memmap.h"
#include "JitILAsm.h"
#include "JitIL.h"
#include "../../HW/GPFifo.h"
#include "../../Core.h"
#include "x64Emitter.h"

using namespace IREmitter;
using namespace Gen;

struct RegInfo {
	JitIL *Jit;
	IRBuilder* Build;
	InstLoc FirstI;
	std::vector<unsigned> IInfo;
	InstLoc regs[16];
	InstLoc fregs[16];
	unsigned numSpills;
	unsigned numFSpills;
	bool MakeProfile;
	bool UseProfile;
	unsigned numProfiledLoads;
	unsigned exitNumber;

	RegInfo(JitIL* j, InstLoc f, unsigned insts) : Jit(j), FirstI(f), IInfo(insts) {
		for (unsigned i = 0; i < 16; i++) {
			regs[i] = 0;
			fregs[i] = 0;
		}
		numSpills = 0;
		numFSpills = 0;
		numProfiledLoads = 0;
		exitNumber = 0;
		MakeProfile = UseProfile = false;
	}

	private:
		RegInfo(RegInfo&); // DO NOT IMPLEMENT
};

static void regMarkUse(RegInfo& R, InstLoc I, InstLoc Op, unsigned OpNum) {
	unsigned& info = R.IInfo[Op - R.FirstI];
	if (info == 0) R.IInfo[I - R.FirstI] |= 1 << (OpNum + 1);
	if (info < 2) info++;
}

static unsigned regReadUse(RegInfo& R, InstLoc I) {
	return R.IInfo[I - R.FirstI] & 3;
}

static unsigned SlotSet[1000];
static unsigned ProfiledLoads[1000];
static u8 GC_ALIGNED16(FSlotSet[16*1000]);

static OpArg regLocForSlot(RegInfo& RI, unsigned slot) {
	return M(&SlotSet[slot - 1]);
}

static unsigned regCreateSpill(RegInfo& RI, InstLoc I) {
	unsigned newSpill = ++RI.numSpills;
	RI.IInfo[I - RI.FirstI] |= newSpill << 16;
	return newSpill;
}

static unsigned regGetSpill(RegInfo& RI, InstLoc I) {
	return RI.IInfo[I - RI.FirstI] >> 16;
}

static void regSpill(RegInfo& RI, X64Reg reg) {
	if (!RI.regs[reg]) return;
	unsigned slot = regGetSpill(RI, RI.regs[reg]);
	if (!slot) {
		slot = regCreateSpill(RI, RI.regs[reg]);
		RI.Jit->MOV(32, regLocForSlot(RI, slot), R(reg));
	}
	RI.regs[reg] = 0;
}

static OpArg fregLocForSlot(RegInfo& RI, unsigned slot) {
	return M(&FSlotSet[slot*16]);
}

// Used for accessing the top half of a spilled double
static OpArg fregLocForSlotPlusFour(RegInfo& RI, unsigned slot) {
	return M(&FSlotSet[slot*16+4]);
}

static unsigned fregCreateSpill(RegInfo& RI, InstLoc I) {
	unsigned newSpill = ++RI.numFSpills;
	RI.IInfo[I - RI.FirstI] |= newSpill << 16;
	return newSpill;
}

static unsigned fregGetSpill(RegInfo& RI, InstLoc I) {
	return RI.IInfo[I - RI.FirstI] >> 16;
}

static void fregSpill(RegInfo& RI, X64Reg reg) {
	if (!RI.fregs[reg]) return;
	unsigned slot = fregGetSpill(RI, RI.fregs[reg]);
	if (!slot) {
		slot = fregCreateSpill(RI, RI.fregs[reg]);
		RI.Jit->MOVAPD(fregLocForSlot(RI, slot), reg);
	}
	RI.fregs[reg] = 0;
}

// ECX is scratch, so we don't allocate it
#ifdef _M_X64

// 64-bit - calling conventions differ between linux & windows, so...
#ifdef _WIN32
static const X64Reg RegAllocOrder[] = {RSI, RDI, R12, R13, R14, R8, R9, R10, R11};
#else
static const X64Reg RegAllocOrder[] = {RBP, R12, R13, R14, R8, R9, R10, R11};
#endif
static const int RegAllocSize = sizeof(RegAllocOrder) / sizeof(X64Reg);
static const X64Reg FRegAllocOrder[] = {XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5};
static const int FRegAllocSize = sizeof(FRegAllocOrder) / sizeof(X64Reg);

#else

// 32-bit
static const X64Reg RegAllocOrder[] = {EDI, ESI, EBP, EBX, EDX, EAX};
static const int RegAllocSize = sizeof(RegAllocOrder) / sizeof(X64Reg);
static const X64Reg FRegAllocOrder[] = {XMM2, XMM3, XMM4, XMM5, XMM6, XMM7};
static const int FRegAllocSize = sizeof(FRegAllocOrder) / sizeof(X64Reg);

#endif

static X64Reg regFindFreeReg(RegInfo& RI) {
	for (unsigned i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == 0)
			return RegAllocOrder[i];

	static unsigned nextReg = 0;
	X64Reg reg = RegAllocOrder[nextReg++ % RegAllocSize];
	regSpill(RI, reg);
	return reg;
}

static X64Reg fregFindFreeReg(RegInfo& RI) {
	for (unsigned i = 0; i < FRegAllocSize; i++)
		if (RI.fregs[FRegAllocOrder[i]] == 0)
			return FRegAllocOrder[i];
	static unsigned nextReg = 0;
	X64Reg reg = FRegAllocOrder[nextReg++ % FRegAllocSize];
	fregSpill(RI, reg);
	return reg;
}

static OpArg regLocForInst(RegInfo& RI, InstLoc I) {
	for (unsigned i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == I)
			return R(RegAllocOrder[i]);

	if (regGetSpill(RI, I) == 0)
		PanicAlert("Retrieving unknown spill slot?!");
	return regLocForSlot(RI, regGetSpill(RI, I));
}

static OpArg fregLocForInst(RegInfo& RI, InstLoc I) {
	for (unsigned i = 0; i < FRegAllocSize; i++)
		if (RI.fregs[FRegAllocOrder[i]] == I)
			return R(FRegAllocOrder[i]);

	if (fregGetSpill(RI, I) == 0)
		PanicAlert("Retrieving unknown spill slot?!");
	return fregLocForSlot(RI, fregGetSpill(RI, I));
}

static void regClearInst(RegInfo& RI, InstLoc I) {
	for (unsigned i = 0; i < RegAllocSize; i++)
		if (RI.regs[RegAllocOrder[i]] == I)
			RI.regs[RegAllocOrder[i]] = 0;
}

static void fregClearInst(RegInfo& RI, InstLoc I) {
	for (unsigned i = 0; i < FRegAllocSize; i++)
		if (RI.fregs[FRegAllocOrder[i]] == I)
			RI.fregs[FRegAllocOrder[i]] = 0;
}

static X64Reg regEnsureInReg(RegInfo& RI, InstLoc I) {
	OpArg loc = regLocForInst(RI, I);
	if (!loc.IsSimpleReg()) {
		X64Reg newReg = regFindFreeReg(RI);
		RI.Jit->MOV(32, R(newReg), loc);
		loc = R(newReg);
	}
	return loc.GetSimpleReg();
}

static X64Reg fregEnsureInReg(RegInfo& RI, InstLoc I) {
	OpArg loc = fregLocForInst(RI, I);
	if (!loc.IsSimpleReg()) {
		X64Reg newReg = fregFindFreeReg(RI);
		RI.Jit->MOVAPD(newReg, loc);
		loc = R(newReg);
	}
	return loc.GetSimpleReg();
}

static void regSpillCallerSaved(RegInfo& RI) {
#ifdef _M_IX86
	// 32-bit
	regSpill(RI, EDX);
	regSpill(RI, ECX);
	regSpill(RI, EAX);
#else
	// 64-bit
	regSpill(RI, RCX);
	regSpill(RI, RDX);
	regSpill(RI, RSI); 
	regSpill(RI, RDI);
	regSpill(RI, R8);
	regSpill(RI, R9);
	regSpill(RI, R10);
	regSpill(RI, R11);
#endif
}

static X64Reg regUReg(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4 &&
	    regLocForInst(RI, getOp1(I)).IsSimpleReg()) {
		return regLocForInst(RI, getOp1(I)).GetSimpleReg();
	}
	X64Reg reg = regFindFreeReg(RI);
	return reg;
}

static X64Reg regBinLHSReg(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4) {
		return regEnsureInReg(RI, getOp1(I));
	}
	X64Reg reg = regFindFreeReg(RI);
	RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	return reg;
}

static void regNormalRegClear(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4)
		regClearInst(RI, getOp1(I));
	if (RI.IInfo[I - RI.FirstI] & 8)
		regClearInst(RI, getOp2(I));
}

static void fregNormalRegClear(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4)
		fregClearInst(RI, getOp1(I));
	if (RI.IInfo[I - RI.FirstI] & 8)
		fregClearInst(RI, getOp2(I));
}

static void regEmitBinInst(RegInfo& RI, InstLoc I,
			   void (JitIL::*op)(int, const OpArg&,
			                     const OpArg&),
			   bool commutable = false) {
	X64Reg reg;
	bool commuted = false;
	if (RI.IInfo[I - RI.FirstI] & 4) {
		reg = regEnsureInReg(RI, getOp1(I));
	} else if (commutable && (RI.IInfo[I - RI.FirstI] & 8)) {
		reg = regEnsureInReg(RI, getOp2(I));
		commuted = true;
	} else {
		reg = regFindFreeReg(RI);
		RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	}
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		if (RHS + 128 < 256) {
			(RI.Jit->*op)(32, R(reg), Imm8(RHS));
		} else {
			(RI.Jit->*op)(32, R(reg), Imm32(RHS));
		}
	} else if (commuted) {
		(RI.Jit->*op)(32, R(reg), regLocForInst(RI, getOp1(I)));
	} else {
		(RI.Jit->*op)(32, R(reg), regLocForInst(RI, getOp2(I)));
	}
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void fregEmitBinInst(RegInfo& RI, InstLoc I,
			    void (JitIL::*op)(X64Reg, OpArg)) {
	X64Reg reg;
	if (RI.IInfo[I - RI.FirstI] & 4) {
		reg = fregEnsureInReg(RI, getOp1(I));
	} else {
		reg = fregFindFreeReg(RI);
		RI.Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
	}
	(RI.Jit->*op)(reg, fregLocForInst(RI, getOp2(I)));
	RI.fregs[reg] = I;
	fregNormalRegClear(RI, I);
}

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

static void regClearDeadMemAddress(RegInfo& RI, InstLoc I, InstLoc AI, unsigned OpNum) {
	if (!(RI.IInfo[I - RI.FirstI] & (2 << OpNum)))
		return;
	if (isImm(*AI)) {
		unsigned addr = RI.Build->GetImmValue(AI);	
		if (Memory::IsRAMAddress(addr)) {
			return;
		}
	}
	InstLoc AddrBase;
	if (getOpcode(*AI) == Add && isImm(*getOp2(AI))) {
		AddrBase = getOp1(AI);
	} else {
		AddrBase = AI;
	}
	regClearInst(RI, AddrBase);
}

// in 64-bit build, this returns a completely bizarre address sometimes!
static OpArg regBuildMemAddress(RegInfo& RI, InstLoc I, InstLoc AI,
				unsigned OpNum,	unsigned Size, X64Reg* dest,
				bool Profiled,
				unsigned ProfileOffset = 0) {
	if (isImm(*AI)) {
		unsigned addr = RI.Build->GetImmValue(AI);	
		if (Memory::IsRAMAddress(addr)) {
			if (dest)
				*dest = regFindFreeReg(RI);
#ifdef _M_IX86
			// 32-bit
			if (Profiled) 
				return M((void*)((u8*)Memory::base + (addr & Memory::MEMVIEW32_MASK)));
			return M((void*)addr);
#else
			// 64-bit
			if (Profiled) {
				RI.Jit->LEA(32, EAX, M((void*)addr));
				return MComplex(RBX, EAX, SCALE_1, 0);
			}
			return M((void*)addr);
#endif
		}
	}
	unsigned offset;
	InstLoc AddrBase;
	if (getOpcode(*AI) == Add && isImm(*getOp2(AI))) {
		offset = RI.Build->GetImmValue(getOp2(AI));
		AddrBase = getOp1(AI);
	} else {
		offset = 0;
		AddrBase = AI;
	}
	X64Reg baseReg;
	// Ok, this stuff needs a comment or three :P -ector
	if (RI.IInfo[I - RI.FirstI] & (2 << OpNum)) {
		baseReg = regEnsureInReg(RI, AddrBase);
		regClearInst(RI, AddrBase);
		if (dest)
			*dest = baseReg;
	} else if (dest) {
		X64Reg reg = regFindFreeReg(RI);
		if (!regLocForInst(RI, AddrBase).IsSimpleReg()) {
			RI.Jit->MOV(32, R(reg), regLocForInst(RI, AddrBase));
			baseReg = reg;
		} else {
			baseReg = regLocForInst(RI, AddrBase).GetSimpleReg();
		}
		*dest = reg;
	} else {
		baseReg = regEnsureInReg(RI, AddrBase);
	}

	if (Profiled) {
		// (Profiled mode isn't the default, at least for the moment)
#ifdef _M_IX86
		return MDisp(baseReg, (u32)Memory::base + offset + ProfileOffset);
#else
		RI.Jit->LEA(32, EAX, MDisp(baseReg, offset));
		return MComplex(RBX, EAX, SCALE_1, 0);
#endif
	}
	return MDisp(baseReg, offset);
}

static void regEmitMemLoad(RegInfo& RI, InstLoc I, unsigned Size) {
	bool win32 = false;
	if (RI.UseProfile) {
		unsigned curLoad = ProfiledLoads[RI.numProfiledLoads++];
		if (!(curLoad & 0x0C000000)) {
			X64Reg reg;
			OpArg addr = regBuildMemAddress(RI, I, getOp1(I), 1,
							Size, &reg, true,
							-(curLoad & 0xC0000000));
			RI.Jit->MOVZX(32, Size, reg, addr);
			RI.Jit->BSWAP(Size, reg);
			if (regReadUse(RI, I))
				RI.regs[reg] = I;
			return;
		} 
	}
	X64Reg reg;
	OpArg addr = regBuildMemAddress(RI, I, getOp1(I), 1, Size, &reg, false);
	RI.Jit->LEA(32, ECX, addr);
	if (RI.MakeProfile) {
		RI.Jit->MOV(32, M(&ProfiledLoads[RI.numProfiledLoads++]), R(ECX));
	}
#ifdef _M_IX86
	win32 = true;
#endif
	FixupBranch argh;
	if (!(win32 && Core::GetStartupParameter().iTLBHack == 1))
	{
		RI.Jit->TEST(32, R(ECX), Imm32(0x0C000000));
		argh = RI.Jit->J_CC(CC_Z);
	}

	// Slow safe read using Memory::Read_Ux routines
#ifdef _M_IX86  // we don't allocate EAX on x64 so no reason to save it.
	if (reg != EAX) {
		RI.Jit->PUSH(32, R(EAX));
	}
#endif
	switch (Size)
	{
	case 32: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), ECX); break;
	case 16: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16_ZX, 1), ECX); break;
	case 8:  RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8_ZX, 1), ECX);  break;
	}
	if (reg != EAX) {
		RI.Jit->MOV(32, R(reg), R(EAX));
#ifdef _M_IX86
		RI.Jit->POP(32, R(EAX));
#endif
	}
	if (!(win32 && Core::GetStartupParameter().iTLBHack == 1))
	{
		FixupBranch arg2 = RI.Jit->J();
	// Fast unsafe read using memory pointer EBX
		RI.Jit->SetJumpTarget(argh);
		RI.Jit->UnsafeLoadRegToReg(ECX, reg, Size, 0, false);
		RI.Jit->SetJumpTarget(arg2);
	}
	if (regReadUse(RI, I))
		RI.regs[reg] = I;
}

static OpArg regSwappedImmForConst(RegInfo& RI, InstLoc I, unsigned Size) {
	unsigned imm = RI.Build->GetImmValue(I);
	if (Size == 32) {
		imm = Common::swap32(imm);
		return Imm32(imm);
	} else if (Size == 16) {
		imm = Common::swap16(imm);
		return Imm16(imm);
	} else {
		return Imm8(imm);
	}
}

static OpArg regImmForConst(RegInfo& RI, InstLoc I, unsigned Size) {
	unsigned imm = RI.Build->GetImmValue(I);
	if (Size == 32) {
		return Imm32(imm);
	} else if (Size == 16) {
		return Imm16(imm);
	} else {
		return Imm8(imm);
	}
}

static void regEmitMemStore(RegInfo& RI, InstLoc I, unsigned Size) {
	if (RI.UseProfile) {
		unsigned curStore = ProfiledLoads[RI.numProfiledLoads++];
		if (!(curStore & 0x0C000000)) {
			OpArg addr = regBuildMemAddress(RI, I, getOp2(I), 2,
							Size, 0, true,
							-(curStore & 0xC0000000));
			if (isImm(*getOp1(I))) {
				RI.Jit->MOV(Size, addr, regSwappedImmForConst(RI, getOp1(I), Size));
			} else {
				RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
				RI.Jit->BSWAP(Size, ECX);
				RI.Jit->MOV(Size, addr, R(ECX));
			}
			if (RI.IInfo[I - RI.FirstI] & 4)
				regClearInst(RI, getOp1(I));
			return;
		} else if ((curStore & 0xFFFFF000) == 0xCC008000) {
			regSpill(RI, EAX);
			if (isImm(*getOp1(I))) {
				RI.Jit->MOV(Size, R(ECX), regSwappedImmForConst(RI, getOp1(I), Size));
			} else { 
				RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
				RI.Jit->BSWAP(Size, ECX);
			}
			RI.Jit->MOV(32, R(EAX), M(&GPFifo::m_gatherPipeCount));
			RI.Jit->MOV(Size, MDisp(EAX, (u32)(u64)GPFifo::m_gatherPipe), R(ECX));
			RI.Jit->ADD(32, R(EAX), Imm8(Size >> 3));
			RI.Jit->MOV(32, M(&GPFifo::m_gatherPipeCount), R(EAX));
			RI.Jit->js.fifoBytesThisBlock += Size >> 3;
			if (RI.IInfo[I - RI.FirstI] & 4)
				regClearInst(RI, getOp1(I));
			regClearDeadMemAddress(RI, I, getOp2(I), 2);
			return;
		}
	}
	OpArg addr = regBuildMemAddress(RI, I, getOp2(I), 2, Size, 0, false);
	RI.Jit->LEA(32, ECX, addr);
	regSpill(RI, EAX);
	if (isImm(*getOp1(I))) {
		RI.Jit->MOV(Size, R(EAX), regImmForConst(RI, getOp1(I), Size));
	} else {
		RI.Jit->MOV(32, R(EAX), regLocForInst(RI, getOp1(I)));
	}
	if (RI.MakeProfile) {
		RI.Jit->MOV(32, M(&ProfiledLoads[RI.numProfiledLoads++]), R(ECX));
	}
	RI.Jit->SafeWriteRegToReg(EAX, ECX, Size, 0);
	if (RI.IInfo[I - RI.FirstI] & 4)
		regClearInst(RI, getOp1(I));
}

static void regEmitShiftInst(RegInfo& RI, InstLoc I, void (JitIL::*op)(int, OpArg, OpArg))
{
	X64Reg reg = regBinLHSReg(RI, I);
	if (isImm(*getOp2(I))) {
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

static void regStoreInstToConstLoc(RegInfo& RI, unsigned width, InstLoc I, void* loc) {
	if (width != 32) {
		PanicAlert("Not implemented!");
		return;
	}
	if (isImm(*I)) {
		RI.Jit->MOV(32, M(loc), Imm32(RI.Build->GetImmValue(I)));
		return;
	}
	X64Reg reg = regEnsureInReg(RI, I);
	RI.Jit->MOV(32, M(loc), R(reg));
}

static void regEmitCmp(RegInfo& RI, InstLoc I) {
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		RI.Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm32(RHS));
	} else {
		X64Reg reg = regEnsureInReg(RI, getOp1(I));
		RI.Jit->CMP(32, R(reg), regLocForInst(RI, getOp2(I)));
	}
}

static void regEmitICmpInst(RegInfo& RI, InstLoc I, CCFlags flag) {
	regEmitCmp(RI, I);
	RI.Jit->SETcc(flag, R(ECX)); // Caution: SETCC uses 8-bit regs!
	X64Reg reg = regFindFreeReg(RI);
	RI.Jit->MOVZX(32, 8, reg, R(ECX));
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
}

static void regWriteExit(RegInfo& RI, InstLoc dest) {
	if (RI.MakeProfile) {
		if (isImm(*dest)) {
			RI.Jit->MOV(32, M(&PC), Imm32(RI.Build->GetImmValue(dest)));
		} else {
			RI.Jit->MOV(32, R(EAX), regLocForInst(RI, dest));
			RI.Jit->MOV(32, M(&PC), R(EAX));
		}
		RI.Jit->Cleanup();
		RI.Jit->SUB(32, M(&CoreTiming::downcount), Imm32(RI.Jit->js.downcountAmount));
		RI.Jit->JMP(((JitIL *)jit)->asm_routines.doReJit, true);
		return;
	}
	if (isImm(*dest)) {
		RI.Jit->WriteExit(RI.Build->GetImmValue(dest), RI.exitNumber++);
	} else {
		if (!regLocForInst(RI, dest).IsSimpleReg(EAX))
			RI.Jit->MOV(32, R(EAX), regLocForInst(RI, dest));
		RI.Jit->WriteExitDestInEAX(RI.exitNumber++);
	}
}

static void DoWriteCode(IRBuilder* ibuild, JitIL* Jit, bool UseProfile, bool MakeProfile) {
	//printf("Writing block: %x\n", js.blockStart);
	RegInfo RI(Jit, ibuild->getFirstInst(), ibuild->getNumInsts());
	RI.Build = ibuild;
	RI.UseProfile = UseProfile;
	RI.MakeProfile = MakeProfile;
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
		case InterpreterFallback:
		case SystemCall:
		case RFIExit:
		case InterpreterBranch:		
		case ShortIdleLoop:
		case Int3:
		case Tramp:
			// No liveness effects
			break;
		case SExt8:
		case SExt16:
		case BSwap32:
		case BSwap16:
		case Cntlzw:
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
		case FSRSqrt:
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
		switch (getOpcode(*I)) {
		case InterpreterFallback: {
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
		case LoadGReg: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOV(32, R(reg), M(&PowerPC::ppcState.gpr[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCR: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOVZX(32, 8, reg, M(&PowerPC::ppcState.cr_fast[ppcreg]));
			RI.regs[reg] = I;
			break;
		}
		case LoadCTR: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&CTR));
			RI.regs[reg] = I;
			break;
		}
		case LoadLink: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&LR));
			RI.regs[reg] = I;
			break;
		}
		case LoadMSR: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&MSR));
			RI.regs[reg] = I;
			break;
		}
		case LoadGQR: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			unsigned gqr = *I >> 8;
			Jit->MOV(32, R(reg), M(&GQR(gqr)));
			RI.regs[reg] = I;
			break;
		}
		case LoadCarry: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), M(&PowerPC::ppcState.spr[SPR_XER]));
			Jit->SHR(32, R(reg), Imm8(29));
			Jit->AND(32, R(reg), Imm8(1));
			RI.regs[reg] = I;
			break;
		}
		case StoreGReg: {
			unsigned ppcreg = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I),
					       &PowerPC::ppcState.gpr[ppcreg]);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCR: {
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			unsigned ppcreg = *I >> 16;
			// CAUTION: uses 8-bit reg!
			Jit->MOV(8, M(&PowerPC::ppcState.cr_fast[ppcreg]), R(ECX));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreLink: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &LR);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCTR: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &CTR);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreMSR: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &MSR);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreGQR: {
			unsigned gqr = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I), &GQR(gqr));
			regNormalRegClear(RI, I);
			break;
		}
		case StoreSRR: {
			unsigned srr = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I),
					&PowerPC::ppcState.spr[SPR_SRR0+srr]);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreCarry: {
			Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm8(0));
			FixupBranch nocarry = Jit->J_CC(CC_Z);
			Jit->JitSetCA();
			FixupBranch cont = Jit->J();
			Jit->SetJumpTarget(nocarry);
			Jit->JitClearCA();
			Jit->SetJumpTarget(cont);
			regNormalRegClear(RI, I);
			break;
		}
		case StoreFPRF: {
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->AND(32, R(ECX), Imm8(0x1F));
			Jit->SHL(32, R(ECX), Imm8(12));
			Jit->AND(32, M(&FPSCR), Imm32(~(0x1F << 12)));
			Jit->OR(32, M(&FPSCR), R(ECX));
			regNormalRegClear(RI, I);
			break;
		}
		case Load8: {
			regEmitMemLoad(RI, I, 8);
			break;
		}
		case Load16: {
			regEmitMemLoad(RI, I, 16);
			break;
		}
		case Load32: {
			regEmitMemLoad(RI, I, 32);
			break;
		}
		case Store8: {
			regEmitMemStore(RI, I, 8);
			break;
		}
		case Store16: {
			regEmitMemStore(RI, I, 16);
			break;
		}
		case Store32: {
			regEmitMemStore(RI, I, 32);
			break;
		}
		case SExt8: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->MOVSX(32, 8, reg, R(ECX));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case SExt16: {
			if (!thisUsed) break;
			X64Reg reg = regUReg(RI, I);
			Jit->MOVSX(32, 16, reg, regLocForInst(RI, getOp1(I)));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Cntlzw: {
			if (!thisUsed) break;
			X64Reg reg = regUReg(RI, I);
			Jit->MOV(32, R(ECX), Imm32(63));
			Jit->BSR(32, reg, regLocForInst(RI, getOp1(I)));
			Jit->CMOVcc(32, reg, R(ECX), CC_Z);
			Jit->XOR(32, R(reg), Imm8(31));
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case And: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitIL::AND, true);
			break;
		}
		case Xor: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitIL::XOR, true);
			break;
		}
		case Sub: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitIL::SUB);
			break;
		}
		case Or: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitIL::OR, true);
			break;
		}
		case Add: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &JitIL::ADD, true);
			break;
		}
		case Mul: {
			if (!thisUsed) break;
			// FIXME: Use three-address capability of IMUL!
			X64Reg reg = regBinLHSReg(RI, I);
			if (isImm(*getOp2(I))) {
				unsigned RHS = RI.Build->GetImmValue(getOp2(I));
				if (RHS + 128 < 256) {
					Jit->IMUL(32, reg, Imm8(RHS));
				} else {
					Jit->IMUL(32, reg, Imm32(RHS));
				}
			} else {
				Jit->IMUL(32, reg, regLocForInst(RI, getOp2(I)));
			}
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case Rol: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitIL::ROL);
			break;
		}
		case Shl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitIL::SHL);
			break;
		}
		case Shrl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitIL::SHR);
			break;
		}
		case Sarl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &JitIL::SAR);
			break;
		}
		case ICmpEq: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_E);
			break;
		}
		case ICmpNe: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_NE);
			break;
		}
		case ICmpUgt: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_A);
			break;
		}
		case ICmpUlt: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_B);
			break;
		}
		case ICmpUge: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_AE);
			break;
		}
		case ICmpUle: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_BE);
			break;
		}
		case ICmpSgt: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_G);
			break;
		}
		case ICmpSlt: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_L);
			break;
		}
		case ICmpSge: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_GE);
			break;
		}
		case ICmpSle: {
			if (!thisUsed) break;
			regEmitICmpInst(RI, I, CC_LE);
			break;
		}
		case ICmpCRUnsigned: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			X64Reg reg = regFindFreeReg(RI);
			FixupBranch pLesser  = Jit->J_CC(CC_B);
			FixupBranch pGreater = Jit->J_CC(CC_A);
			Jit->MOV(32, R(reg), Imm32(0x2)); // _x86Reg == 0
			FixupBranch continue1 = Jit->J();
			Jit->SetJumpTarget(pGreater);
			Jit->MOV(32, R(reg), Imm32(0x4)); // _x86Reg > 0
			FixupBranch continue2 = Jit->J();
			Jit->SetJumpTarget(pLesser);
			Jit->MOV(32, R(reg), Imm32(0x8)); // _x86Reg < 0
			Jit->SetJumpTarget(continue1);
			Jit->SetJumpTarget(continue2);
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case ICmpCRSigned: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			X64Reg reg = regFindFreeReg(RI);
			FixupBranch pLesser  = Jit->J_CC(CC_L);
			FixupBranch pGreater = Jit->J_CC(CC_G);
			Jit->MOV(32, R(reg), Imm32(0x2)); // _x86Reg == 0
			FixupBranch continue1 = Jit->J();
			Jit->SetJumpTarget(pGreater);
			Jit->MOV(32, R(reg), Imm32(0x4)); // _x86Reg > 0
			FixupBranch continue2 = Jit->J();
			Jit->SetJumpTarget(pLesser);
			Jit->MOV(32, R(reg), Imm32(0x8)); // _x86Reg < 0
			Jit->SetJumpTarget(continue1);
			Jit->SetJumpTarget(continue2);
			RI.regs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadSingle: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			RI.Jit->UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
			Jit->MOVD_xmm(reg, R(ECX));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadDouble: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->ADD(32, R(ECX), Imm8(4));
			RI.Jit->UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
			Jit->MOVD_xmm(reg, R(ECX));
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			RI.Jit->UnsafeLoadRegToReg(ECX, ECX, 32, 0, false);
			Jit->MOVD_xmm(XMM0, R(ECX));
			Jit->PUNPCKLDQ(reg, R(XMM0));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case LoadPaired: {
			if (!thisUsed) break;
			regSpill(RI, EAX);
			regSpill(RI, EDX);
			X64Reg reg = fregFindFreeReg(RI);
			unsigned int quantreg = *I >> 16;
			Jit->MOVZX(32, 16, EAX, M(((char *)&GQR(quantreg)) + 2));
			Jit->MOVZX(32, 8, EDX, R(AL));
			// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32]! (MComplex can do this, no?)
#ifdef _M_IX86
			Jit->SHL(32, R(EDX), Imm8(2));
#else
			Jit->SHL(32, R(EDX), Imm8(3));
#endif
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MDisp(EDX, (u32)(u64)(((JitIL *)jit)->asm_routines.pairedLoadQuantized)));
			Jit->MOVAPD(reg, R(XMM0));
			RI.fregs[reg] = I;
			regNormalRegClear(RI, I);
			break;
		}
		case StoreSingle: {
			regSpill(RI, EAX);
			if (fregLocForInst(RI, getOp1(I)).IsSimpleReg()) {
				Jit->MOVD_xmm(R(EAX), fregLocForInst(RI, getOp1(I)).GetSimpleReg());
			} else {
				Jit->MOV(32, R(EAX), fregLocForInst(RI, getOp1(I)));
			}
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			RI.Jit->SafeWriteRegToReg(EAX, ECX, 32, 0);
			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case StoreDouble: {
			regSpill(RI, EAX);
			// FIXME: Use 64-bit where possible
			// FIXME: Use unsafe write with pshufb where possible
			unsigned fspill = fregGetSpill(RI, getOp1(I));
			if (!fspill) {
				// Force the value to spill, so we can use
				// memory operations to load it
				fspill = fregCreateSpill(RI, getOp1(I));
				X64Reg reg = fregLocForInst(RI, getOp1(I)).GetSimpleReg();
				RI.Jit->MOVAPD(fregLocForSlot(RI, fspill), reg);
			}
			Jit->MOV(32, R(EAX), fregLocForSlotPlusFour(RI, fspill));
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			RI.Jit->SafeWriteRegToReg(EAX, ECX, 32, 0);
			Jit->MOV(32, R(EAX), fregLocForSlot(RI, fspill));
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			RI.Jit->SafeWriteRegToReg(EAX, ECX, 32, 4);
			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case StorePaired: {
			regSpill(RI, EAX);
			regSpill(RI, EDX);
			u32 quantreg = *I >> 24;
			Jit->MOVZX(32, 16, EAX, M(&PowerPC::ppcState.spr[SPR_GQR0 + quantreg]));
			Jit->MOVZX(32, 8, EDX, R(AL));
			// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32]!
#ifdef _M_IX86
			Jit->SHL(32, R(EDX), Imm8(2));
#else
			Jit->SHL(32, R(EDX), Imm8(3));
#endif
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MDisp(EDX, (u32)(u64)(((JitIL *)jit)->asm_routines.pairedStoreQuantized)));
			if (RI.IInfo[I - RI.FirstI] & 4)
				fregClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}
		case DupSingleToMReg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->CVTSS2SD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->MOVDDUP(reg, R(reg));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case InsertDoubleInMReg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp2(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->MOVSD(reg, R(XMM0));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case ExpandPackedToMReg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->CVTPS2PD(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case CompactMRegToPacked: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->CVTPD2PS(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FSNeg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			static const u32 GC_ALIGNED16(ssSignBits[4]) = 
				{0x80000000};
			Jit->PXOR(reg, M((void*)&ssSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FDNeg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			static const u64 GC_ALIGNED16(ssSignBits[2]) = 
				{0x8000000000000000ULL};
			Jit->PXOR(reg, M((void*)&ssSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPNeg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			static const u32 GC_ALIGNED16(psSignBits[4]) = 
				{0x80000000, 0x80000000};
			Jit->PXOR(reg, M((void*)&psSignBits));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPDup0: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->PUNPCKLDQ(reg, R(reg));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPDup1: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->SHUFPS(reg, R(reg), 0xE5);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case LoadFReg: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			unsigned ppcreg = *I >> 8;
			Jit->MOVAPD(reg, M(&PowerPC::ppcState.ps[ppcreg]));
			RI.fregs[reg] = I;
			break;
		}	    
		case LoadFRegDENToZero: {
			if (!thisUsed) break;
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
		case StoreFReg: {
			unsigned ppcreg = *I >> 16;
			Jit->MOVAPD(M(&PowerPC::ppcState.ps[ppcreg]),
				      fregEnsureInReg(RI, getOp1(I)));
			fregNormalRegClear(RI, I);
			break;
		}
		case DoubleToSingle: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->CVTSD2SS(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FSMul: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::MULSS);
			break;
		}
		case FSAdd: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::ADDSS);
			break;
		}
		case FSSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::SUBSS);
			break;
		}
		case FSRSqrt: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->RSQRTSS(reg, fregLocForInst(RI, getOp1(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FDMul: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::MULSD);
			break;
		}
		case FDAdd: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::ADDSD);
			break;
		}
		case FDSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::SUBSD);
			break;
		}
		case FDCmpCR: {
			X64Reg destreg = regFindFreeReg(RI);
			// TODO: Add case for NaN (CC_P)
			Jit->MOVSD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->UCOMISD(XMM0, fregLocForInst(RI, getOp2(I)));
			FixupBranch pNan     = Jit->J_CC(CC_P);
			FixupBranch pEqual   = Jit->J_CC(CC_Z);
			FixupBranch pLesser  = Jit->J_CC(CC_C);
			// Greater
			Jit->MOV(32, R(destreg), Imm32(0x4));
			FixupBranch continue1 = Jit->J();
			// NaN
			Jit->SetJumpTarget(pNan);
			Jit->MOV(32, R(destreg), Imm32(0x1));
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
		case FPAdd: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::ADDPS);
			break;
		}
		case FPMul: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::MULPS);
			break;
		}
		case FPSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &JitIL::SUBPS);
			break;
		}
		case FPMerge00: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->PUNPCKLDQ(reg, fregLocForInst(RI, getOp2(I)));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge01: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			// Note reversed operands!
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp2(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->MOVSS(reg, R(XMM0));
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge10: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp2(I)));
			Jit->MOVSS(reg, R(XMM0));
			Jit->SHUFPS(reg, R(reg), 0xF1);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case FPMerge11: {
			if (!thisUsed) break;
			X64Reg reg = fregFindFreeReg(RI);
			Jit->MOVAPD(reg, fregLocForInst(RI, getOp1(I)));
			Jit->PUNPCKLDQ(reg, fregLocForInst(RI, getOp2(I)));
			Jit->SHUFPD(reg, R(reg), 0x1);
			RI.fregs[reg] = I;
			fregNormalRegClear(RI, I);
			break;
		}
		case CInt32:
		case CInt16: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), Imm32(ibuild->GetImmValue(I)));
			RI.regs[reg] = I;
			break;
		}
		case BlockStart:
		case BlockEnd:
			break;

		case IdleBranch: {			
			Jit->CMP(32, regLocForInst(RI, getOp1(getOp1(I))),
					 Imm32(RI.Build->GetImmValue(getOp2(getOp1(I)))));			
			FixupBranch cont = Jit->J_CC(CC_NE);

			RI.Jit->Cleanup(); // is it needed?			
			Jit->ABI_CallFunction((void *)&PowerPC::OnIdleIL);
			
			Jit->MOV(32, M(&PC), Imm32(ibuild->GetImmValue( getOp2(I) )));
			Jit->JMP(((JitIL *)jit)->asm_routines.testExceptions, true);

			Jit->SetJumpTarget(cont);
			if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(getOp1(I)));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
			break;
		}

		case BranchCond: {
			if (isICmp(*getOp1(I)) &&
			    isImm(*getOp2(getOp1(I)))) {
				Jit->CMP(32, regLocForInst(RI, getOp1(getOp1(I))),
					 Imm32(RI.Build->GetImmValue(getOp2(getOp1(I)))));
				CCFlags flag;
				switch (getOpcode(*getOp1(I))) {
					case ICmpEq: flag = CC_NE; break;
					case ICmpNe: flag = CC_E; break;
					case ICmpUgt: flag = CC_BE; break;
					case ICmpUlt: flag = CC_AE; break;
					case ICmpUge: flag = CC_L; break;
					case ICmpUle: flag = CC_A; break;
					case ICmpSgt: flag = CC_LE; break;
					case ICmpSlt: flag = CC_GE; break;
					case ICmpSge: flag = CC_L; break;
					case ICmpSle: flag = CC_G; break;
				}
				FixupBranch cont = Jit->J_CC(flag);
				regWriteExit(RI, getOp2(I));
				Jit->SetJumpTarget(cont);
				if (RI.IInfo[I - RI.FirstI] & 4)
					regClearInst(RI, getOp1(getOp1(I)));
			} else {
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
		case BranchUncond: {
			regWriteExit(RI, getOp1(I));
			regNormalRegClear(RI, I);
			break;
		}		
		case ShortIdleLoop: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->ABI_CallFunction((void *)&CoreTiming::Idle);
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->JMP(((JitIL *)jit)->asm_routines.testExceptions, true);
			break;
		}
		case SystemCall: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->Cleanup();
			Jit->OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_SYSCALL));
			Jit->MOV(32, M(&PC), Imm32(InstLoc + 4));
			Jit->JMP(((JitIL *)jit)->asm_routines.testExceptions, true);
			break;
		}
		case InterpreterBranch: {
			Jit->MOV(32, R(EAX), M(&NPC));
			Jit->WriteExitDestInEAX(0);
			break;
		}
		case RFIExit: {
			// See Interpreter rfi for details
			const u32 mask = 0x87C0FFFF;
			// MSR = (MSR & ~mask) | (SRR1 & mask);
			Jit->MOV(32, R(EAX), M(&MSR));
			Jit->MOV(32, R(ECX), M(&SRR1));
			Jit->AND(32, R(EAX), Imm32(~mask));
			Jit->AND(32, R(ECX), Imm32(mask));
			Jit->OR(32, R(EAX), R(ECX));       
			// MSR &= 0xFFFDFFFF; //TODO: VERIFY
			Jit->AND(32, R(EAX), Imm32(0xFFFDFFFF));
			Jit->MOV(32, M(&MSR), R(EAX));
			// NPC = SRR0; 
			Jit->MOV(32, R(EAX), M(&SRR0));
			Jit->WriteRfiExitDestInEAX();
			break;
		}
		case Int3: {
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

	for (unsigned i = 0; i < 8; i++) {
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

	//if (!RI.MakeProfile && RI.numSpills)
	//	printf("Block: %x, numspills %d\n", Jit->js.blockStart, RI.numSpills);

	Jit->UD2();
}

void JitIL::WriteCode() {
	DoWriteCode(&ibuild, this, false, Core::GetStartupParameter().bJITProfiledReJIT);
}

void ProfiledReJit() {
	jit->SetCodePtr(jit->js.rewriteStart);
	DoWriteCode(&((JitIL *)jit)->ibuild, (JitIL *)jit, true, false);
	jit->js.curBlock->codeSize = (int)(jit->GetCodePtr() - jit->js.rewriteStart);
	jit->GetBlockCache()->FinalizeBlock(jit->js.curBlock->blockNum, jit->jo.enableBlocklink, jit->js.curBlock->normalEntry);
}