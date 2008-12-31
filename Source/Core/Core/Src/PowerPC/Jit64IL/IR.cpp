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

/*

IR implementation comments:
This file implements code generation for a new IR-based JIT. The idea of
the IR is that as much as possible, it strips away the complexities
of the PPC instruction set into a simpler instruction set.  In its current
form, the semantics are very simple: each instruction does its calculation
and performs its side effects in order.  For an instruction with a result,
the instruction also represents the returned value.  This works quite
simply because jumps within a block are not allowed.

The IR treats loads and stores to PPC registers as separate steps from actual
calculations.  This allows the instruction set to be significantly simpler,
because one PPC instruction can be mapped to multiple IR instructions.  It
also allows optimizing out dead register stores: this reduces register
pressure and allows dead code elimination to completely remove instructions
which produce unused values, or the carry flag of srawx.

The actual IR representation uses a few tricks I picked up from nanojit:
each instruction is a single 32-bit integer, the operands are 8-bit offsets
back from the current instruction, and there's a special Tramp instruction
to reference operands that are too far away to reference otherwise.

The first step of code generation is producing the IR; this is roughly
equivalent to all of code generation in the previous code. In addition
to storing the IR, some optimizations occur in this step: the primary
optimizations are that redundant register loads/stores are eliminated,
and constant-folding is done.

The second step is a quick pass over the IL to figure out liveness: this
information is used both for dead code elimination and to find the last
use of an instruction, which is allowed to destroy the value.

The third step is the actual code generation: this just goes through each
instruction and generates code.  Dead code elimination works in this step,
by simply skipping unused instructions.  The register allocator is a dumb,
greedy allocator: at the moment, it's really a bit too dumb, but it's
actually not as bad as it looks: unless a block is relatively long, spills
are rarely needed.  ECX is used as a scratch register: requiring a scratch
register isn't ideal, but the register allocator is too dumb to handle
instructions that need a specific register at the moment.

In addition to the optimizations that are deeply tied to the IR,
I've implemented one additional trick: fast memory for 32-bit machines.
This works off of the observation that loads and stores can be classified
at runtime: any particular load instruction will always load similar addresses,
and any store will store to similar addresses.  Using this observation, every
block is JIT-ed twice: the first time, the block includes extra code to 
instrument the loads.  Then, at the end of the block, it jumps back into the JIT
to recompile itself.  The second recompilation looks at the address of each load
and store, and bakes the information into the generated code.  This allows removing
the overhead for both the mask and the branch normally required for loads on 32-bit
machines.  This optimization isn't completely safe: it depends on a guarantee which
isn't made by the PPC instruction set.  That said, it's reliable enough that games
work without any fallback, and it's a large performance boost.  Also, if it turns
out it isn't completely reliable, we can use a solution using segments which is
similar to the 64-bit fast memory implementation.

The speed with this JIT is better than I expected, but not at much as I hoped...
on the test I've been working on (which bounded by JIT performance and doesn't
use any floating-point), it's roughly 25% faster than the current JIT, with the
edge over the current JIT mostly due to the fast memory optimization.

TODO (in no particular order):
Floating-point JIT (both paired and unpaired): currently falls back
	to the interpreter
Improve register allocator to deal with long live intervals.
Optimize conditions for conditional branches.
Inter-block dead register elimination, especially for CR0.
Inter-block inlining.
Track down a few correctness bugs.
Known zero bits: eliminate unneeded AND instructions for rlwinm/rlwimi
Implement a select instruction
64-bit compat (it should only be a few tweaks to register allocation and
	the load/store code)
Scheduling to reduce register pressure: PowerPC	compilers like to push
	uses far away from definitions, but it's rather unfriendly to modern
	x86 processors, which are short on registers and extremely good at
	instruction reordering.
Common subexpression elimination
Optimize load of sum using complex addressing
Implement idle-skipping

*/

#include "IR.h"
#include "../PPCTables.h"
#include "../../CoreTiming.h"
#include "Thunk.h"
#include "../../HW/Memmap.h"
#include "JitAsm.h"
#include "Jit.h"
#include "../../HW/GPFifo.h"
using namespace Gen;

namespace IREmitter {

InstLoc IRBuilder::EmitZeroOp(unsigned Opcode, unsigned extra = 0) {
	InstLoc curIndex = &InstList[InstList.size()];
	InstList.push_back(Opcode | (extra << 8));
	return curIndex;
}

InstLoc IRBuilder::EmitUOp(unsigned Opcode, InstLoc Op1, unsigned extra) {
	InstLoc curIndex = &InstList[InstList.size()];
	unsigned backOp1 = curIndex - 1 - Op1;
	if (backOp1 >= 256) {
		InstList.push_back(Tramp | backOp1 << 8);
		backOp1 = 0;
		curIndex++;
	}
	InstList.push_back(Opcode | (backOp1 << 8) | (extra << 16));
	return curIndex;
}

InstLoc IRBuilder::EmitBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2) {
	InstLoc curIndex = &InstList[InstList.size()];
	unsigned backOp1 = curIndex - 1 - Op1;
	if (backOp1 >= 255) {
		InstList.push_back(Tramp | backOp1 << 8);
		backOp1 = 0;
		curIndex++;
	}
	unsigned backOp2 = curIndex - 1 - Op2;
	if (backOp2 >= 256) {
		InstList.push_back(Tramp | backOp2 << 8);
		backOp2 = 0;
		backOp1++;
		curIndex++;
	}
	InstList.push_back(Opcode | backOp1 << 8 | backOp2 << 16);
	return curIndex;
}

#if 0
InstLoc IRBuilder::EmitTriOp(unsigned Opcode, InstLoc Op1, InstLoc Op2,
			   InstLoc Op3) {
	InstLoc curIndex = &InstList[InstList.size()];
	unsigned backOp1 = curIndex - 1 - Op1;
	if (backOp1 >= 254) {
		InstList.push_back(Tramp | backOp1 << 8);
		backOp1 = 0;
		curIndex++;
	}
	unsigned backOp2 = curIndex - 1 - Op2;
	if (backOp2 >= 255) {
		InstList.push_back((Tramp | backOp2 << 8));
		backOp2 = 0;
		backOp1++;
		curIndex++;
	}
	unsigned backOp3 = curIndex - 1 - Op3;
	if (backOp3 >= 256) {
		InstList.push_back(Tramp | (backOp3 << 8));
		backOp3 = 0;
		backOp2++;
		backOp1++;
		curIndex++;
	}
	InstList.push_back(Opcode | (backOp1 << 8) | (backOp2 << 16) |
				    (backOp3 << 24));
	return curIndex;
}
#endif

InstLoc IRBuilder::FoldZeroOp(unsigned Opcode, unsigned extra) {
	if (Opcode == LoadGReg) {
		// Reg load folding: if we already loaded the value,
		// load it again
		if (!GRegCache[extra])
			GRegCache[extra] = EmitZeroOp(LoadGReg, extra);
		return GRegCache[extra];
	}
	if (Opcode == LoadCarry) {
		if (!CarryCache)
			CarryCache = EmitZeroOp(LoadGReg, extra);
		return CarryCache;
	}
	if (Opcode == LoadCR) {
		if (!CRCache[extra])
			CRCache[extra] = EmitZeroOp(LoadCR, extra);
		return CRCache[extra];
	}

	return EmitZeroOp(Opcode, extra);
}

InstLoc IRBuilder::FoldUOp(unsigned Opcode, InstLoc Op1, unsigned extra) {
	if (Opcode == StoreGReg) {
		// Reg store folding: save the value for load folding.
		// If there's a previous store, zap it because it's dead.
		GRegCache[extra] = Op1;
		if (GRegCacheStore[extra]) {
			*GRegCacheStore[extra] = 0;
		}
		GRegCacheStore[extra] = EmitUOp(StoreGReg, Op1, extra);
		return GRegCacheStore[extra];
	}
	if (Opcode == StoreCarry) {
		CarryCache = Op1;
		if (CarryCacheStore) {
			*CarryCacheStore = 0;
		}
		CarryCacheStore = EmitUOp(StoreCarry, Op1, extra);
		return CarryCacheStore;
	}
	if (Opcode == StoreCR) {
		CRCache[extra] = Op1;
		if (CRCacheStore[extra]) {
			*CRCacheStore[extra] = 0;
		}
		CRCacheStore[extra] = EmitUOp(StoreCR, Op1, extra);
		return CRCacheStore[extra];
	}

	return EmitUOp(Opcode, Op1, extra);
}

InstLoc IRBuilder::FoldAdd(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2))
			return EmitIntConst(GetImmValue(Op1) +
					    GetImmValue(Op2));
		return FoldAdd(Op2, Op1);
	}
	if (isImm(*Op2)) {
		if (!GetImmValue(Op2)) return Op1;
		if (getOpcode(*Op1) == Add && isImm(*getOp2(Op1))) {
			unsigned RHS = GetImmValue(Op2) +
				       GetImmValue(getOp2(Op1));
			return FoldAdd(getOp1(Op1), EmitIntConst(RHS));
		}
	}
	return EmitBiOp(Add, Op1, Op2);
}

InstLoc IRBuilder::FoldAnd(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2))
			return EmitIntConst(GetImmValue(Op1) &
					    GetImmValue(Op2));
		return FoldAnd(Op2, Op1);
	}
	if (isImm(*Op2)) {
		if (!GetImmValue(Op2)) return EmitIntConst(0);
		if (GetImmValue(Op2) == -1U) return Op1;
		if (getOpcode(*Op1) == And && isImm(*getOp2(Op1))) {
			unsigned RHS = GetImmValue(Op2) &
				       GetImmValue(getOp2(Op1));
			return FoldAnd(getOp1(Op1), EmitIntConst(RHS));
		} else if (getOpcode(*Op1) == Rol && isImm(*getOp2(Op1))) {
			unsigned shiftMask1 = -1U << (GetImmValue(getOp2(Op1)) & 31);
			if (GetImmValue(Op2) == shiftMask1)
				return FoldShl(getOp1(Op1), getOp2(Op1));
			unsigned shiftAmt2 = ((32 - GetImmValue(getOp2(Op1))) & 31);
			unsigned shiftMask2 = -1U >> shiftAmt2;
			if (GetImmValue(Op2) == shiftMask2) {
				return FoldShrl(getOp1(Op1), EmitIntConst(shiftAmt2));
			}
		}
	}
	if (Op1 == Op2) return Op1;

	return EmitBiOp(And, Op1, Op2);
}

InstLoc IRBuilder::FoldOr(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2))
			return EmitIntConst(GetImmValue(Op1) |
					    GetImmValue(Op2));
		return FoldOr(Op2, Op1);
	}
	if (isImm(*Op2)) {
		if (!GetImmValue(Op2)) return Op1;
		if (GetImmValue(Op2) == -1U) return EmitIntConst(-1U);
		if (getOpcode(*Op1) == Or && isImm(*getOp2(Op1))) {
			unsigned RHS = GetImmValue(Op2) |
				       GetImmValue(getOp2(Op1));
			return FoldOr(getOp1(Op1), EmitIntConst(RHS));
		}
	}
	if (Op1 == Op2) return Op1;

	return EmitBiOp(Or, Op1, Op2);
}

InstLoc IRBuilder::FoldXor(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2))
			return EmitIntConst(GetImmValue(Op1) ^
					    GetImmValue(Op2));
		return FoldXor(Op2, Op1);
	}
	if (isImm(*Op2)) {
		if (!GetImmValue(Op2)) return Op1;
		if (getOpcode(*Op1) == Xor && isImm(*getOp2(Op1))) {
			unsigned RHS = GetImmValue(Op2) ^
				       GetImmValue(getOp2(Op1));
			return FoldXor(getOp1(Op1), EmitIntConst(RHS));
		}
	}
	if (Op1 == Op2) return Op1;

	return EmitBiOp(Xor, Op1, Op2);
}

InstLoc IRBuilder::FoldShl(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op2)) {
		if (isImm(*Op1))
			return EmitIntConst(GetImmValue(Op1) << (GetImmValue(Op2) & 31));
	}
	return EmitBiOp(Shl, Op1, Op2);
}

InstLoc IRBuilder::FoldShrl(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op2)) {
		if (isImm(*Op1))
			return EmitIntConst(GetImmValue(Op1) >> (GetImmValue(Op2) & 31));
	}
	return EmitBiOp(Shrl, Op1, Op2);
}

InstLoc IRBuilder::FoldRol(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op2)) {
		if (isImm(*Op1))
			return EmitIntConst(_rotl(GetImmValue(Op1),
					          GetImmValue(Op2)));
		if (!(GetImmValue(Op2) & 31)) return Op1;
	}
	return EmitBiOp(Rol, Op1, Op2);
}

InstLoc IRBuilder::FoldInterpreterFallback(InstLoc Op1, InstLoc Op2) {
	for (unsigned i = 0; i < 32; i++) {
		GRegCache[i] = 0;
		GRegCacheStore[i] = 0;
	}
	CarryCache = 0;
	CarryCacheStore = 0;
	for (unsigned i = 0; i < 8; i++) {
		CRCache[i] = 0;
		CRCacheStore[i] = 0;
	}
	return EmitBiOp(InterpreterFallback, Op1, Op2);
}

InstLoc IRBuilder::FoldBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2) {
	switch (Opcode) {
		case Add: return FoldAdd(Op1, Op2);
		case And: return FoldAnd(Op1, Op2);
		case Or: return FoldOr(Op1, Op2);
		case Xor: return FoldXor(Op1, Op2);
		case Shl: return FoldShl(Op1, Op2);
		case Shrl: return FoldShrl(Op1, Op2);
		case Rol: return FoldRol(Op1, Op2);
		case InterpreterFallback: return FoldInterpreterFallback(Op1, Op2);
		default: return EmitBiOp(Opcode, Op1, Op2);
	}
}

InstLoc IRBuilder::EmitIntConst(unsigned value) {
	InstLoc curIndex = &InstList[InstList.size()];
	InstList.push_back(CInt32 | (ConstList.size() << 8));
	ConstList.push_back(value);
	return curIndex;
}

unsigned IRBuilder::GetImmValue(InstLoc I) {
	return ConstList[*I >> 8];
}

}

using namespace IREmitter;

/*
Actual codegen is a backward pass followed by a forward pass.

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

struct RegInfo {
	Jit64* Jit;
	IRBuilder* Build;
	InstLoc FirstI;
	std::vector<unsigned> IInfo;
	InstLoc regs[16];
	unsigned numSpills;
	bool MakeProfile;
	bool UseProfile;
	unsigned numProfiledLoads;
	unsigned exitNumber;

	RegInfo(Jit64* j, InstLoc f, unsigned insts) : Jit(j), FirstI(f), IInfo(insts) {
		for (unsigned i = 0; i < 16; i++)
			regs[i] = 0;
		numSpills = 0;
		numProfiledLoads = 0;
		exitNumber = 0;
		MakeProfile = UseProfile = false;
	}
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

static X64Reg regFindFreeReg(RegInfo& RI) {
	if (RI.regs[EDI] == 0) return EDI;
	if (RI.regs[ESI] == 0) return ESI;
	if (RI.regs[EBP] == 0) return EBP;
	if (RI.regs[EBX] == 0) return EBX;
	if (RI.regs[EDX] == 0) return EDX;
	if (RI.regs[EAX] == 0) return EAX;
	// ECX is scratch; never allocate it!
	regSpill(RI, EDI);
	return EDI;
}

static OpArg regLocForInst(RegInfo& RI, InstLoc I) {
	if (RI.regs[EDI] == I) return R(EDI);
	if (RI.regs[ESI] == I) return R(ESI);
	if (RI.regs[EBP] == I) return R(EBP);
	if (RI.regs[EBX] == I) return R(EBX);
	if (RI.regs[EDX] == I) return R(EDX);
	if (RI.regs[EAX] == I) return R(EAX);
	if (RI.regs[ECX] == I) return R(ECX);

	if (regGetSpill(RI, I) == 0)
		PanicAlert("Retrieving unknown spill slot?!");
	return regLocForSlot(RI, regGetSpill(RI, I));
}

static void regClearInst(RegInfo& RI, InstLoc I) {
	if (RI.regs[EDI] == I) {
		RI.regs[EDI] = 0;
	}
	if (RI.regs[ESI] == I) {
		RI.regs[ESI] = 0;
	}
	if (RI.regs[EBP] == I) {
		RI.regs[EBP] = 0;
	}
	if (RI.regs[EBX] == I) {
		RI.regs[EBX] = 0;
	}
	if (RI.regs[EDX] == I) {
		RI.regs[EDX] = 0;
	}
	if (RI.regs[EAX] == I) {
		RI.regs[EAX] = 0;
	}
	if (RI.regs[ECX] == I) {
		RI.regs[ECX] = 0;
	}
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

static void regSpillCallerSaved(RegInfo& RI) {
	regSpill(RI, EDX);
	regSpill(RI, ECX);
	regSpill(RI, EAX);
}

static X64Reg regBinLHSReg(RegInfo& RI, InstLoc I) {
	if (RI.IInfo[I - RI.FirstI] & 4) {
		return regEnsureInReg(RI, getOp1(I));
	}
	X64Reg reg = regFindFreeReg(RI);
	RI.Jit->MOV(32, R(reg), regLocForInst(RI, getOp1(I)));
	return reg;
}

static void regEmitBinInst(RegInfo& RI, InstLoc I,
			   void (Jit64::*op)(int, const OpArg&,
			                     const OpArg&)) {
	X64Reg reg = regBinLHSReg(RI, I);
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		if (RHS + 128 < 256) {
			(RI.Jit->*op)(32, R(reg), Imm8(RHS));
		} else {
			(RI.Jit->*op)(32, R(reg), Imm32(RHS));
		}
	} else {
		(RI.Jit->*op)(32, R(reg), regLocForInst(RI, getOp2(I)));
	}
	RI.regs[reg] = I;
}

static void regEmitMemLoad(RegInfo& RI, InstLoc I, unsigned Size) {
	X64Reg reg = regBinLHSReg(RI, I);
	if (RI.UseProfile) {
		unsigned curLoad = ProfiledLoads[RI.numProfiledLoads++];
		if (!(curLoad & 0x0C000000)) {
			if (regReadUse(RI, I)) {
				unsigned addr = (u32)Memory::base - (curLoad & 0xC0000000);
				RI.Jit->MOVZX(32, Size, reg, MDisp(reg, addr));
				RI.Jit->BSWAP(Size, reg);
				RI.regs[reg] = I;
			}
			return;
		}
	}
	if (RI.MakeProfile) {
		RI.Jit->MOV(32, M(&ProfiledLoads[RI.numProfiledLoads++]), R(reg));
	}
	RI.Jit->TEST(32, R(reg), Imm32(0x0C000000));
	FixupBranch argh = RI.Jit->J_CC(CC_Z);
	if (reg != EAX)
		RI.Jit->PUSH(32, R(EAX));
	switch (Size)
	{
	case 32: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), reg); break;
	case 16: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16, 1), reg); break;
	case 8:  RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8, 1), reg);  break;
	}
	if (reg != EAX) {
		RI.Jit->MOV(32, R(reg), R(EAX));
		RI.Jit->POP(32, R(EAX));
	}
	FixupBranch arg2 = RI.Jit->J();
	RI.Jit->SetJumpTarget(argh);
	RI.Jit->UnsafeLoadRegToReg(reg, reg, Size, 0, false);
	RI.Jit->SetJumpTarget(arg2);
	if (regReadUse(RI, I))
		RI.regs[reg] = I;
}

static void regEmitMemStore(RegInfo& RI, InstLoc I, unsigned Size) {
	if (RI.UseProfile) {
		unsigned curStore = ProfiledLoads[RI.numProfiledLoads++];
		if (!(curStore & 0x0C000000)) {
			X64Reg reg = regEnsureInReg(RI, getOp2(I));
			RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			RI.Jit->BSWAP(Size, ECX);
			unsigned addr = (u32)Memory::base - (curStore & 0xC0000000);
			RI.Jit->MOV(Size, MDisp(reg, addr), R(ECX));
			return;
		} else if ((curStore & 0xFFFFF000) == 0xCC008000) {
			regSpill(RI, EAX);
			RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			RI.Jit->BSWAP(Size, ECX);
			RI.Jit->MOV(32, R(EAX), M(&GPFifo::m_gatherPipeCount));
			RI.Jit->MOV(Size, MDisp(EAX, (u32)GPFifo::m_gatherPipe), R(ECX));
			RI.Jit->ADD(32, R(EAX), Imm8(Size >> 3));
			RI.Jit->MOV(32, M(&GPFifo::m_gatherPipeCount), R(EAX));
			RI.Jit->js.fifoBytesThisBlock += Size >> 3;
			return;
		}
	}
	regSpill(RI, EAX);
	RI.Jit->MOV(32, R(EAX), regLocForInst(RI, getOp1(I)));
	RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
	if (RI.MakeProfile) {
		RI.Jit->MOV(32, M(&ProfiledLoads[RI.numProfiledLoads++]), R(ECX));
	}
	RI.Jit->SafeWriteRegToReg(EAX, ECX, Size, 0);
}

static void regEmitShiftInst(RegInfo& RI, InstLoc I,
			     void (Jit64::*op)(int, OpArg, OpArg))
{
	X64Reg reg = regBinLHSReg(RI, I);
	if (isImm(*getOp2(I))) {
		unsigned RHS = RI.Build->GetImmValue(getOp2(I));
		(RI.Jit->*op)(32, R(reg), Imm8(RHS));
		RI.regs[reg] = I;
		return;
	}
	// FIXME: prevent regBinLHSReg from finding ecx!
	RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
	(RI.Jit->*op)(32, R(reg), R(ECX));
	RI.regs[reg] = I;
}

static void regStoreInstToConstLoc(RegInfo& RI, unsigned width, InstLoc I,
				   void* loc) {
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
		RI.Jit->JMP(asm_routines.doReJit, true);
		return;
	}
	if (isImm(*dest)) {
		RI.Jit->WriteExit(RI.Build->GetImmValue(dest), RI.exitNumber++);
	} else {
		RI.Jit->MOV(32, R(EAX), regLocForInst(RI, dest));
		RI.Jit->WriteExitDestInEAX(RI.exitNumber++);
	}
}

static void DoWriteCode(IRBuilder* ibuild, Jit64* Jit, bool UseProfile) {
	//printf("Writing block: %x\n", js.blockStart);
	RegInfo RI(Jit, ibuild->getFirstInst(), ibuild->getNumInsts());
	RI.Build = ibuild;
	RI.UseProfile = UseProfile;
	RI.MakeProfile = !RI.UseProfile;
	// Pass to compute liveness
	// Note that despite this marking, we never materialize immediates;
	// on x86, they almost always fold into the instruction, and it's at
	// best a code-size reduction in the cases where they don't.
	ibuild->StartBackPass();
	for (unsigned index = RI.IInfo.size() - 1; index != -1U; --index) {
		InstLoc I = ibuild->ReadBackward();
		unsigned op = getOpcode(*I);
		bool thisUsed = regReadUse(RI, I);
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
		case BlockEnd:
		case BlockStart:
		case InterpreterFallback:
			// No liveness effects
			break;
		case Tramp:
			if (thisUsed)
				regMarkUse(RI, I, I - 1 - (*I >> 8), 1);
			break;
		case SExt8:
		case SExt16:
		case BSwap32:
		case BSwap16:
			if (thisUsed)
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case Load8:
		case Load16:
		case Load32:
		case StoreGReg:
		case StoreCR:
		case StoreLink:
		case StoreCarry:
		case StoreCTR:
		case StoreMSR:
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
		case ICmpUgt:
			if (thisUsed) {
				regMarkUse(RI, I, getOp1(I), 1);
				if (!isImm(*getOp2(I)))
					regMarkUse(RI, I, getOp2(I), 2);
			}
			break;
		case Store8:
		case Store16:
		case Store32:
			regMarkUse(RI, I, getOp1(I), 1);
			regMarkUse(RI, I, getOp2(I), 2);
			break;
		case BranchUncond:
			if (!isImm(*getOp1(I)))
				regMarkUse(RI, I, getOp1(I), 1);
			break;
		case BranchCond:
			regMarkUse(RI, I, getOp1(I), 1);
			if (!isImm(*getOp2(I)))
				regMarkUse(RI, I, getOp2(I), 2);
			break;
		}
	}

	ibuild->StartForwardPass();
	for (unsigned i = 0; i != RI.IInfo.size(); i++) {
		InstLoc I = ibuild->ReadForward();
		bool thisUsed = regReadUse(RI, I);
		switch (getOpcode(*I)) {
		case InterpreterFallback: {
			unsigned InstCode = ibuild->GetImmValue(getOp1(I));
			unsigned InstLoc = ibuild->GetImmValue(getOp2(I));
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
		case StoreGReg: {
			unsigned ppcreg = *I >> 16;
			regStoreInstToConstLoc(RI, 32, getOp1(I),
					       &PowerPC::ppcState.gpr[ppcreg]);
			break;
		}
		case StoreCR: {
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			unsigned ppcreg = *I >> 16;
			// CAUTION: uses 8-bit reg!
			Jit->MOV(8, M(&PowerPC::ppcState.cr_fast[ppcreg]), R(ECX));
			break;
		}
		case StoreLink: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &LR);
			break;
		}
		case StoreCTR: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &CTR);
			break;
		}
		case StoreMSR: {
			regStoreInstToConstLoc(RI, 32, getOp1(I), &MSR);
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
			break;
		}
		case SExt16: {
			if (!thisUsed) break;
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOVSX(32, 16, reg, regLocForInst(RI, getOp1(I)));
			RI.regs[reg] = I;
			break;
		}
		case And: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::AND);
			break;
		}
		case Xor: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::XOR);
			break;
		}
		case Sub: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::SUB);
			break;
		}
		case Or: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::OR);
			break;
		}
		case Add: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::ADD);
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
			break;
		}
		case Rol: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &Jit64::ROL);
			break;
		}
		case Shl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &Jit64::SHL);
			break;
		}
		case Shrl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &Jit64::SHR);
			break;
		}
		case Sarl: {
			if (!thisUsed) break;
			regEmitShiftInst(RI, I, &Jit64::SAR);
			break;
		}
		case ICmpEq: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			Jit->SETcc(CC_Z, R(ECX)); // Caution: SETCC uses 8-bit regs!
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOVZX(32, 8, reg, R(ECX));
			RI.regs[reg] = I;
			break;
		}
		case ICmpUgt: {
			if (!thisUsed) break;
			regEmitCmp(RI, I);
			Jit->SETcc(CC_A, R(ECX)); // Caution: SETCC uses 8-bit regs!
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOVZX(32, 8, reg, R(ECX));
			RI.regs[reg] = I;
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
		case BranchCond: {
			Jit->CMP(32, regLocForInst(RI, getOp1(I)), Imm8(0));
			FixupBranch cont = Jit->J_CC(CC_NZ);
			regWriteExit(RI, getOp2(I));
			Jit->SetJumpTarget(cont);
			break;
		}
		case BranchUncond: {
			regWriteExit(RI, getOp1(I));
			break;
		}
		case Tramp: {
			if (!thisUsed) break;
			// FIXME: Optimize!
			InstLoc Op = I - 1 - (*I >> 8);
			X64Reg reg = regFindFreeReg(RI);
			Jit->MOV(32, R(reg), regLocForInst(RI, Op));
			RI.regs[reg] = I;
			if (RI.IInfo[I - RI.FirstI] & 4)
				regClearInst(RI, Op);
			break;
		}
		case Nop: break;
		default:
			PanicAlert("Unknown JIT instruction; aborting!");
			exit(1);
		}
		if (getOpcode(*I) != Tramp) {
			if (RI.IInfo[I - RI.FirstI] & 4)
				regClearInst(RI, getOp1(I));
			if (RI.IInfo[I - RI.FirstI] & 8)
				regClearInst(RI, getOp2(I));
		}
	}
	for (unsigned i = 0; i < 8; i++) {
		if (RI.regs[i]) {
			PanicAlert("Incomplete cleanup!");
			exit(1);
		}
	}

	printf("Block: %x, numspills %d\n", Jit->js.blockStart, RI.numSpills);

	Jit->MOV(32, R(EAX), M(&NPC));
	Jit->WriteRfiExitDestInEAX();
	Jit->UD2();
}

void Jit64::WriteCode() {
	DoWriteCode(&ibuild, this, false);
}

void ProfiledReJit() {
	u8* x = (u8*)jit.GetCodePtr();
	jit.SetCodePtr(jit.js.normalEntry);
	DoWriteCode(&jit.ibuild, &jit, true);
	jit.js.curBlock->codeSize = jit.GetCodePtr() - jit.js.normalEntry;
	jit.SetCodePtr(x);
}
