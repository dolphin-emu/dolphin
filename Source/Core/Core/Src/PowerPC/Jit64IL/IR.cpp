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
#include "../../Core.h"
using namespace Gen;

namespace IREmitter {

InstLoc IRBuilder::EmitZeroOp(unsigned Opcode, unsigned extra = 0) {
	InstLoc curIndex = &InstList[InstList.size()];
	InstList.push_back(Opcode | (extra << 8));
	return curIndex;
}

InstLoc IRBuilder::EmitUOp(unsigned Opcode, InstLoc Op1, unsigned extra) {
	InstLoc curIndex = &InstList[InstList.size()];
	unsigned backOp1 = (s32)(curIndex - 1 - Op1);
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
	unsigned backOp1 = (s32)(curIndex - 1 - Op1);
	if (backOp1 >= 255) {
		InstList.push_back(Tramp | backOp1 << 8);
		backOp1 = 0;
		curIndex++;
	}
	unsigned backOp2 = (s32)(curIndex - 1 - Op2);
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
InstLoc IRBuilder::EmitTriOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, InstLoc Op3) {
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
	InstList.push_back(Opcode | (backOp1 << 8) | (backOp2 << 16) | (backOp3 << 24));
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
	if (Opcode == LoadFRegDENToZero) {
		FRegCacheStore[extra] = 0; // prevent previous store operation from zapping
		FRegCache[extra] = EmitZeroOp(LoadFRegDENToZero, extra);
		return FRegCache[extra];
	}
	if (Opcode == LoadCarry) {
		if (!CarryCache)
			CarryCache = EmitZeroOp(LoadCarry, extra);
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
		if (getOpcode(*Op1) >= FDMul && getOpcode(*Op1) <= FDSub) {
			InstLoc OOp1 = getOp1(Op1), OOp2 = getOp2(Op1);
			if (getOpcode(*OOp1) == DupSingleToMReg &&
			    getOpcode(*OOp2) == DupSingleToMReg) {
				if (getOpcode(*Op1) == FDMul) {
					return FoldBiOp(FSMul, getOp1(OOp1), getOp2(OOp2));
				} else if (getOpcode(*Op1) == FDAdd) {
					return FoldBiOp(FSAdd, getOp1(OOp1), getOp2(OOp2));
				} else if (getOpcode(*Op1) == FDSub) {
					return FoldBiOp(FSSub, getOp1(OOp1), getOp2(OOp2));
				}
			}
		}
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

InstLoc IRBuilder::FoldIdleBranch(InstLoc Op1, InstLoc Op2) {
	return EmitBiOp(
		IdleBranch,
		EmitICmpEq(getOp1(getOp1(Op1)),
		getOp2(getOp1(Op1))), Op2
		);
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

InstLoc IRBuilder::FoldICmpCRSigned(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2)) {
			int c1 = (int)GetImmValue(Op1),
			    c2 = (int)GetImmValue(Op2),
			    result;
			if (c1 == c2)
				result = 2;
			else if (c1 > c2)
				result = 4;
			else
				result = 8;
			return EmitIntConst(result);
		}
	}
	return EmitBiOp(ICmpCRSigned, Op1, Op2);
}

InstLoc IRBuilder::FoldICmpCRUnsigned(InstLoc Op1, InstLoc Op2) {
	if (isImm(*Op1)) {
		if (isImm(*Op2)) {
			unsigned int c1 = GetImmValue(Op1),
			             c2 = GetImmValue(Op2),
			             result;
			if (c1 == c2)
				result = 2;
			else if (c1 > c2)
				result = 4;
			else
				result = 8;
			return EmitIntConst(result);
		}
	}
	return EmitBiOp(ICmpCRUnsigned, Op1, Op2);
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
		case IdleBranch: return FoldIdleBranch(Op1, Op2);
		case ICmpEq: case ICmpNe:
		case ICmpUgt: case ICmpUlt: case ICmpUge: case ICmpUle:
		case ICmpSgt: case ICmpSlt: case ICmpSge: case ICmpSle:
			return FoldICmp(Opcode, Op1, Op2);
		case ICmpCRSigned: return FoldICmpCRSigned(Op1, Op2);
		case ICmpCRUnsigned: return FoldICmpCRUnsigned(Op1, Op2);
		case InterpreterFallback: return FoldInterpreterFallback(Op1, Op2);
		case FDMul: case FDAdd: case FDSub: return FoldDoubleBiOp(Opcode, Op1, Op2);
		default: return EmitBiOp(Opcode, Op1, Op2, extra);
	}
}

InstLoc IRBuilder::EmitIntConst(unsigned value) {
	InstLoc curIndex = &InstList[InstList.size()];
	InstList.push_back(CInt32 | ((unsigned int)ConstList.size() << 8));
	ConstList.push_back(value);
	return curIndex;
}

unsigned IRBuilder::GetImmValue(InstLoc I) {
	return ConstList[*I >> 8];
}

}
