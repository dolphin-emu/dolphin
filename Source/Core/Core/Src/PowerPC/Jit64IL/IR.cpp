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

Update on perf:
I've been doing a bit more tweaking for a small perf improvement (in the 
range of 5-10%).  That said, it's getting to the point where I'm simply
not seeing potential for improvements to codegen, at least for long,
straightforward blocks.  For one long block that's at the top of my samples,
I've managed to get the bloat% (number of instructions compared to PPC
equivalent) down to 225%, and I can't really see it going down much further.
It looks like the most promising paths to further improvement for pure
integer code are more aggresively combining blocks and dead condition
register elimination, which should be very helpful for small blocks.

TODO (in no particular order):
JIT for misc remaining FP instructions
JIT for bcctrx
Misc optimizations for FP instructions
Inter-block dead register elimination; this seems likely to have large
	performance benefits, although I'm not completely sure.
Inter-block inlining; also likely to have large performance benefits.
	The tricky parts are deciding which blocks to inline, and that the
	IR can't really deal with branches whose destination is in the
	the middle of a generated block.
Specialized slw/srw/sraw; I think there are some tricks that could
	have a non-trivial effect, and there are significantly shorter
	implementations for 64-bit involving abusing 64-bit shifts.
64-bit compat (it should only be a few tweaks to register allocation and
	the load/store code)
Scheduling to reduce register pressure: PowerPC	compilers like to push
	uses far away from definitions, but it's rather unfriendly to modern
	x86 processors, which are short on registers and extremely good at
	instruction reordering.
Common subexpression elimination
Optimize load/store of sum using complex addressing (partially implemented)
Loop optimizations (loop-carried registers, LICM)
Fold register loads into arithmetic operations
Code refactoring/cleanup
Investigate performance of the JIT itself; this doesn't affect
	framerates significantly, but it does take a visible amount
	of time for a complicated piece of code like a video decoder
	to compile.
Fix profiled loads/stores to work safely.  On 32-bit, one solution is to
	use a spare segment register, and expand the backpatch solution
	to work in all the relevant situations.  On 64-bit, the existing
	fast memory solution should basically work.  An alternative
	would be to figure out a heuristic for what loads actually
	vary their "type", and special-case them.

*/

#ifdef _MSC_VER
#pragma warning(disable:4146)   // unary minus operator applied to unsigned type, result still unsigned
#endif

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

InstLoc IRBuilder::EmitBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, unsigned extra) {
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
	InstList.push_back(Opcode | (backOp1 << 8) | (backOp2 << 16) | (extra << 24));
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

unsigned IRBuilder::ComputeKnownZeroBits(InstLoc I) {
	switch (getOpcode(*I)) {
	case Load8:
		return 0xFFFFFF00;
	case Or:
		return ComputeKnownZeroBits(getOp1(I)) &
		       ComputeKnownZeroBits(getOp2(I));
	case And:
		return ComputeKnownZeroBits(getOp1(I)) |
		       ComputeKnownZeroBits(getOp2(I));
	case Shl:
		if (isImm(*getOp2(I))) {
			unsigned samt = GetImmValue(getOp2(I)) & 31;
			return (ComputeKnownZeroBits(getOp1(I)) << samt) |
			        ~(-1U << samt);
		}
		return 0;
	case Shrl:
		if (isImm(*getOp2(I))) {
			unsigned samt = GetImmValue(getOp2(I)) & 31;
			return (ComputeKnownZeroBits(getOp1(I)) >> samt) |
			        ~(-1U >> samt);
		}
		return 0;
	case Rol:
		if (isImm(*getOp2(I))) {
			return _rotl(ComputeKnownZeroBits(getOp1(I)),
			             GetImmValue(getOp2(I)));
		}
	default:
		return 0;
	}
}

InstLoc IRBuilder::FoldZeroOp(unsigned Opcode, unsigned extra) {
	if (Opcode == LoadGReg) {
		// Reg load folding: if we already loaded the value,
		// load it again
		if (!GRegCache[extra])
			GRegCache[extra] = EmitZeroOp(LoadGReg, extra);
		return GRegCache[extra];
	}
	if (Opcode == LoadFReg) {
		// Reg load folding: if we already loaded the value,
		// load it again
		if (!FRegCache[extra])
			FRegCache[extra] = EmitZeroOp(LoadFReg, extra);
		return FRegCache[extra];
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
	if (Opcode == LoadCTR) {
		if (!CTRCache)
			CTRCache = EmitZeroOp(LoadCTR, extra);
		return CTRCache;
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
	if (Opcode == StoreFReg) {
		FRegCache[extra] = Op1;
		if (FRegCacheStore[extra]) {
			*FRegCacheStore[extra] = 0;
		}
		FRegCacheStore[extra] = EmitUOp(StoreFReg, Op1, extra);
		return FRegCacheStore[extra];
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
	if (Opcode == StoreCTR) {
		CTRCache = Op1;
		if (CTRCacheStore) {
			*CTRCacheStore = 0;
		}
		CTRCacheStore = EmitUOp(StoreCTR, Op1, extra);
		return CTRCacheStore;
	}
	if (Opcode == CompactMRegToPacked) {
		if (getOpcode(*Op1) == ExpandPackedToMReg)
			return getOp1(Op1);
	}
	if (Opcode == DoubleToSingle) {
		if (getOpcode(*Op1) == DupSingleToMReg)
			return getOp1(Op1);
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

InstLoc IRBuilder::FoldSub(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op2)) {
		return FoldAdd(Op1, EmitIntConst(-GetImmValue(Op2)));
	}
	return EmitBiOp(Sub, Op1, Op2);
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
		if (!(~ComputeKnownZeroBits(Op1) & ~GetImmValue(Op2))) {
			return Op1;
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

InstLoc IRBuilder::FoldBranchCond(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (GetImmValue(Op1))
			return EmitBranchUncond(Op2);
		return 0;
	}
	if (getOpcode(*Op1) == And &&
	    isImm(*getOp2(Op1)) &&
	    getOpcode(*getOp1(Op1)) == ICmpCRSigned) {
		unsigned branchValue = GetImmValue(getOp2(Op1));
		if (branchValue == 2)
			return FoldBranchCond(EmitICmpEq(getOp1(getOp1(Op1)),
					      getOp2(getOp1(Op1))), Op2);
		if (branchValue == 4)
			return FoldBranchCond(EmitICmpSgt(getOp1(getOp1(Op1)),
					      getOp2(getOp1(Op1))), Op2);
		if (branchValue == 8)
			return FoldBranchCond(EmitICmpSlt(getOp1(getOp1(Op1)),
					      getOp2(getOp1(Op1))), Op2);
	}
	if (getOpcode(*Op1) == Xor &&
	    isImm(*getOp2(Op1))) {
		InstLoc XOp1 = getOp1(Op1);
		unsigned branchValue = GetImmValue(getOp2(Op1));
		if (getOpcode(*XOp1) == And &&
		    isImm(*getOp2(XOp1)) &&
		    getOpcode(*getOp1(XOp1)) == ICmpCRSigned) {
			unsigned innerBranchValue = 
				GetImmValue(getOp2(XOp1));
			if (branchValue == innerBranchValue) {
				if (branchValue == 2)
					return FoldBranchCond(EmitICmpNe(getOp1(getOp1(XOp1)),
						      getOp2(getOp1(XOp1))), Op2);
				if (branchValue == 4)
					return FoldBranchCond(EmitICmpSle(getOp1(getOp1(XOp1)),
						      getOp2(getOp1(XOp1))), Op2);
				if (branchValue == 8)
					return FoldBranchCond(EmitICmpSge(getOp1(getOp1(XOp1)),
						      getOp2(getOp1(XOp1))), Op2);
			}
		}
	}
	return EmitBiOp(BranchCond, Op1, Op2);
}

InstLoc IRBuilder::FoldICmp(unsigned Opcode, InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2)) {
			unsigned result = 0;
			switch (Opcode) {
			case ICmpEq:
				result = GetImmValue(Op1) == GetImmValue(Op2);
				break;
			case ICmpNe:
				result = GetImmValue(Op1) != GetImmValue(Op2);
				break;
			case ICmpUgt:
				result = GetImmValue(Op1) > GetImmValue(Op2);
				break;
			case ICmpUlt:
				result = GetImmValue(Op1) < GetImmValue(Op2);
				break;
			case ICmpUge:
				result = GetImmValue(Op1) >= GetImmValue(Op2);
				break;
			case ICmpUle:
				result = GetImmValue(Op1) <= GetImmValue(Op2);
				break;
			case ICmpSgt:
				result = (signed)GetImmValue(Op1) >
					 (signed)GetImmValue(Op2);
				break;
			case ICmpSlt:
				result = (signed)GetImmValue(Op1) <
					 (signed)GetImmValue(Op2);
				break;
			case ICmpSge:
				result = (signed)GetImmValue(Op1) >=
					 (signed)GetImmValue(Op2);
				break;
			case ICmpSle:
				result = (signed)GetImmValue(Op1) <=
					 (signed)GetImmValue(Op2);
				break;
			}
			return EmitIntConst(result);
		}
		switch (Opcode) {
		case ICmpEq:
			return FoldICmp(ICmpEq, Op2, Op1);
		case ICmpNe:
			return FoldICmp(ICmpNe, Op2, Op1);
		case ICmpUlt:
			return FoldICmp(ICmpUgt, Op2, Op1);
		case ICmpUgt:
			return FoldICmp(ICmpUlt, Op2, Op1);
		case ICmpUle:
			return FoldICmp(ICmpUge, Op2, Op1);
		case ICmpUge:
			return FoldICmp(ICmpUle, Op2, Op1);
		case ICmpSlt:
			return FoldICmp(ICmpSgt, Op2, Op1);
		case ICmpSgt:
			return FoldICmp(ICmpSlt, Op2, Op1);
		case ICmpSle:
			return FoldICmp(ICmpSge, Op2, Op1);
		case ICmpSge:
			return FoldICmp(ICmpSle, Op2, Op1);
		}
	}
	return EmitBiOp(Opcode, Op1, Op2);
}

InstLoc IRBuilder::FoldInterpreterFallback(InstLoc Op1, InstLoc Op2) {
	for (unsigned i = 0; i < 32; i++) {
		GRegCache[i] = 0;
		GRegCacheStore[i] = 0;
		FRegCache[i] = 0;
		FRegCacheStore[i] = 0;
	}
	CarryCache = 0;
	CarryCacheStore = 0;
	for (unsigned i = 0; i < 8; i++) {
		CRCache[i] = 0;
		CRCacheStore[i] = 0;
	}
	CTRCache = 0;
	CTRCacheStore = 0;	
	return EmitBiOp(InterpreterFallback, Op1, Op2);
}

InstLoc IRBuilder::FoldDoubleBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2) {
	if (getOpcode(*Op1) == InsertDoubleInMReg) {
		return FoldDoubleBiOp(Opcode, getOp1(Op1), Op2);
	}

	if (getOpcode(*Op2) == InsertDoubleInMReg) {
		return FoldDoubleBiOp(Opcode, Op1, getOp1(Op2));
	}

	return EmitBiOp(Opcode, Op1, Op2);
}

InstLoc IRBuilder::FoldBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, unsigned extra) {
	switch (Opcode) {
		case Add: return FoldAdd(Op1, Op2);
		case Sub: return FoldSub(Op1, Op2);
		case And: return FoldAnd(Op1, Op2);
		case Or: return FoldOr(Op1, Op2);
		case Xor: return FoldXor(Op1, Op2);
		case Shl: return FoldShl(Op1, Op2);
		case Shrl: return FoldShrl(Op1, Op2);
		case Rol: return FoldRol(Op1, Op2);
		case BranchCond: return FoldBranchCond(Op1, Op2);
		case ICmpEq: case ICmpNe:
		case ICmpUgt: case ICmpUlt: case ICmpUge: case ICmpUle:
		case ICmpSgt: case ICmpSlt: case ICmpSge: case ICmpSle:
			return FoldICmp(Opcode, Op1, Op2);
		case InterpreterFallback: return FoldInterpreterFallback(Op1, Op2);
		case FDMul: case FDAdd: case FDSub: return FoldDoubleBiOp(Opcode, Op1, Op2);
		default: return EmitBiOp(Opcode, Op1, Op2, extra);
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
	InstLoc fregs[16];
	unsigned numSpills;
	unsigned numFSpills;
	bool MakeProfile;
	bool UseProfile;
	unsigned numProfiledLoads;
	unsigned exitNumber;

	RegInfo(Jit64* j, InstLoc f, unsigned insts) : Jit(j), FirstI(f), IInfo(insts) {
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
static X64Reg RegAllocOrder[] = {EDI, ESI, EBP, EBX, EDX, EAX};
static unsigned RegAllocSize = sizeof(RegAllocOrder) / sizeof(X64Reg);
static X64Reg FRegAllocOrder[] = {XMM2, XMM3, XMM4, XMM5, XMM6, XMM7};
static unsigned FRegAllocSize = sizeof(FRegAllocOrder) / sizeof(X64Reg);

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
	regSpill(RI, EDX);
	regSpill(RI, ECX);
	regSpill(RI, EAX);
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
			   void (Jit64::*op)(int, const OpArg&,
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
			    void (Jit64::*op)(X64Reg, OpArg)) {
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

static OpArg regBuildMemAddress(RegInfo& RI, InstLoc I, InstLoc AI,
				unsigned OpNum,	unsigned Size, X64Reg* dest,
				bool Profiled,
				unsigned ProfileOffset = 0) {
	if (isImm(*AI)) {
		unsigned addr = RI.Build->GetImmValue(AI);	
		if (Memory::IsRAMAddress(addr)) {
#ifdef _M_IX86
			// 32-bit
			if (dest)
				*dest = regFindFreeReg(RI);
			if (Profiled) 
				return M((void*)((u32)Memory::base + (addr & Memory::MEMVIEW32_MASK)));
			return M((void*)addr);
#else
			// 64-bit
			if (Profiled) 
				return M((void*)((u32)Memory::base + addr));
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
		return MDisp(baseReg, (u32)Memory::base + offset + ProfileOffset);
	}
	return MDisp(baseReg, offset);
}

static void regEmitMemLoad(RegInfo& RI, InstLoc I, unsigned Size) {
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
	RI.Jit->TEST(32, R(ECX), Imm32(0x0C000000));
	FixupBranch argh = RI.Jit->J_CC(CC_Z);
	if (reg != EAX)
		RI.Jit->PUSH(32, R(EAX));
	switch (Size)
	{
	case 32: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U32, 1), ECX); break;
	case 16: RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U16, 1), ECX); break;
	case 8:  RI.Jit->ABI_CallFunctionR(thunks.ProtectFunction((void *)&Memory::Read_U8, 1), ECX);  break;
	}
	if (reg != EAX) {
		RI.Jit->MOV(32, R(reg), R(EAX));
		RI.Jit->POP(32, R(EAX));
	}
	FixupBranch arg2 = RI.Jit->J();
	RI.Jit->SetJumpTarget(argh);
	RI.Jit->UnsafeLoadRegToReg(ECX, reg, Size, 0, false);
	RI.Jit->SetJumpTarget(arg2);
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
			RI.Jit->MOV(Size, MDisp(EAX, (u32)GPFifo::m_gatherPipe), R(ECX));
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
	RI.Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
	(RI.Jit->*op)(32, R(reg), R(ECX));
	RI.regs[reg] = I;
	regNormalRegClear(RI, I);
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
	RI.MakeProfile = false;//!RI.UseProfile;
	// Pass to compute liveness
	ibuild->StartBackPass();
	for (unsigned int index = RI.IInfo.size() - 1; index != -1U; --index) {
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
		case BlockEnd:
		case BlockStart:
		case InterpreterFallback:
		case SystemCall:
		case RFIExit:
		case InterpreterBranch:
		case IdleLoop:
		case ShortIdleLoop:
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
			regEmitBinInst(RI, I, &Jit64::AND, true);
			break;
		}
		case Xor: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::XOR, true);
			break;
		}
		case Sub: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::SUB);
			break;
		}
		case Or: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::OR, true);
			break;
		}
		case Add: {
			if (!thisUsed) break;
			regEmitBinInst(RI, I, &Jit64::ADD, true);
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
			unsigned quantreg = *I >> 16;
			Jit->MOVZX(32, 16, EAX, M(((char *)&PowerPC::ppcState.spr[SPR_GQR0 + quantreg]) + 2));
			Jit->MOVZX(32, 8, EDX, R(AL));
			// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32]!
			Jit->SHL(32, R(EDX), Imm8(2));
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MDisp(EDX, (u32)asm_routines.pairedLoadQuantized));
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
			unsigned quantreg = *I >> 24;
			Jit->MOVZX(32, 16, EAX, M(&PowerPC::ppcState.spr[SPR_GQR0 + quantreg]));
			Jit->MOVZX(32, 8, EDX, R(AL));
			// FIXME: Fix ModR/M encoding to allow [EDX*4+disp32]!
			Jit->SHL(32, R(EDX), Imm8(2));
			Jit->MOV(32, R(ECX), regLocForInst(RI, getOp2(I)));
			Jit->MOVAPD(XMM0, fregLocForInst(RI, getOp1(I)));
			Jit->CALLptr(MDisp(EDX, (u32)asm_routines.pairedStoreQuantized));
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
			fregEmitBinInst(RI, I, &Jit64::MULSS);
			break;
		}
		case FSAdd: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::ADDSS);
			break;
		}
		case FSSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::SUBSS);
			break;
		}
		case FDMul: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::MULSD);
			break;
		}
		case FDAdd: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::ADDSD);
			break;
		}
		case FDSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::SUBSD);
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
			fregEmitBinInst(RI, I, &Jit64::ADDPS);
			break;
		}
		case FPMul: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::MULPS);
			break;
		}
		case FPSub: {
			if (!thisUsed) break;
			fregEmitBinInst(RI, I, &Jit64::SUBPS);
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
		case IdleLoop: {
			unsigned IdleParam = ibuild->GetImmValue(getOp1(I));
			unsigned InstLoc = ibuild->GetImmValue(getOp2(I));
			Jit->ABI_CallFunctionC((void *)&PowerPC::OnIdle, IdleParam);
			Jit->MOV(32, M(&PC), Imm32(InstLoc + 12));
			Jit->JMP(asm_routines.testExceptions, true);
			break;
		}
		case ShortIdleLoop: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->ABI_CallFunction((void *)&CoreTiming::Idle);
			Jit->MOV(32, M(&PC), Imm32(InstLoc));
			Jit->JMP(asm_routines.testExceptions, true);
			break;
		}
		case SystemCall: {
			unsigned InstLoc = ibuild->GetImmValue(getOp1(I));
			Jit->Cleanup();
			Jit->OR(32, M(&PowerPC::ppcState.Exceptions), Imm32(EXCEPTION_SYSCALL));
			Jit->MOV(32, M(&PC), Imm32(InstLoc + 4));
			Jit->JMP(asm_routines.testExceptions, true);
			break;
		}
		case InterpreterBranch: {
			Jit->MOV(32, R(EAX), M(&NPC));
			Jit->WriteExitDestInEAX(0);
			break;
		}
		case RFIExit: {
			// Bits SRR1[0, 5-9, 16-23, 25-27, 30-31] are placed
			// into the corresponding bits of the MSR.
			// MSR[13] is set to 0.
			const u32 mask = 0x87C0FF73;
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

void Jit64::WriteCode() {
	DoWriteCode(&ibuild, this, false);
}

void ProfiledReJit() {
	u8* x = (u8*)jit.GetCodePtr();
	jit.SetCodePtr(jit.js.rewriteStart);
	DoWriteCode(&jit.ibuild, &jit, true);
	jit.js.curBlock->codeSize = jit.GetCodePtr() - jit.js.rewriteStart;
	jit.SetCodePtr(x);
}
