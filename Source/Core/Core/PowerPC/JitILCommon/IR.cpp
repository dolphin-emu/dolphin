// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
are rarely needed.  EDX is used as a scratch register: requiring a scratch
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
- JIT for misc remaining FP instructions
- JIT for bcctrx
- Misc optimizations for FP instructions
- Inter-block dead register elimination; this seems likely to have large
  performance benefits, although I'm not completely sure.

- Inter-block inlining; also likely to have large performance benefits.
  The tricky parts are deciding which blocks to inline, and that the
  IR can't really deal with branches whose destination is in the
  the middle of a generated block.

- Specialized slw/srw/sraw; I think there are some tricks that could
  have a non-trivial effect, and there are significantly shorter
  implementations for 64-bit involving abusing 64-bit shifts.
  64-bit compat (it should only be a few tweaks to register allocation and the load/store code)

- Scheduling to reduce register pressure: PowerPCcompilers like to push
  uses far away from definitions, but it's rather unfriendly to modern
  x86 processors, which are short on registers and extremely good at instruction reordering.

- Common subexpression elimination
- Optimize load/store of sum using complex addressing (partially implemented)
- Loop optimizations (loop-carried registers, LICM)
- Code refactoring/cleanup

- Investigate performance of the JIT itself; this doesn't affect
  framerates significantly, but it does take a visible amount
  of time for a complicated piece of code like a video decoder to compile.

- Fix profiled loads/stores to work safely.  On 32-bit, one solution is to
  use a spare segment register, and expand the backpatch solution
  to work in all the relevant situations.  On 64-bit, the existing
  fast memory solution should basically work.  An alternative
  would be to figure out a heuristic for what loads actually
  vary their "type", and special-case them.

*/

#ifdef _MSC_VER
#pragma warning(disable:4146)   // unary minus operator applied to unsigned type, result still unsigned
#endif

#include <algorithm>
#include <cinttypes>
#include <ctime>
#include <memory>
#include <set>
#include <string>

#include "Common/FileUtil.h"
#include "Common/StdMakeUnique.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCTables.h"
#include "Core/PowerPC/JitILCommon/IR.h"
using namespace Gen;

namespace IREmitter
{

InstLoc IRBuilder::EmitZeroOp(unsigned Opcode, unsigned extra = 0)
{
	InstLoc curIndex = InstList.data() + InstList.size();
	InstList.push_back(Opcode | (extra << 8));
	MarkUsed.push_back(false);
	return curIndex;
}

InstLoc IRBuilder::EmitUOp(unsigned Opcode, InstLoc Op1, unsigned extra)
{
	InstLoc curIndex = InstList.data() + InstList.size();
	unsigned backOp1 = (s32)(curIndex - 1 - Op1);
	if (backOp1 >= 256)
	{
		InstList.push_back(Tramp | backOp1 << 8);
		MarkUsed.push_back(false);
		backOp1 = 0;
		curIndex++;
	}

	InstList.push_back(Opcode | (backOp1 << 8) | (extra << 16));
	MarkUsed.push_back(false);
	return curIndex;
}

InstLoc IRBuilder::EmitBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, unsigned extra)
{
	InstLoc curIndex = InstList.data() + InstList.size();
	unsigned backOp1 = (s32)(curIndex - 1 - Op1);
	if (backOp1 >= 255)
	{
		InstList.push_back(Tramp | backOp1 << 8);
		MarkUsed.push_back(false);
		backOp1 = 0;
		curIndex++;
	}

	unsigned backOp2 = (s32)(curIndex - 1 - Op2);
	if (backOp2 >= 256)
	{
		InstList.push_back(Tramp | backOp2 << 8);
		MarkUsed.push_back(false);
		backOp2 = 0;
		backOp1++;
		curIndex++;
	}

	InstList.push_back(Opcode | (backOp1 << 8) | (backOp2 << 16) | (extra << 24));
	MarkUsed.push_back(false);
	return curIndex;
}

#if 0
InstLoc IRBuilder::EmitTriOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, InstLoc Op3)
{
	InstLoc curIndex = InstList.data() + InstList.size();
	unsigned backOp1 = curIndex - 1 - Op1;
	if (backOp1 >= 254)
	{
		InstList.push_back(Tramp | backOp1 << 8);
		MarkUsed.push_back(false);
		backOp1 = 0;
		curIndex++;
	}

	unsigned backOp2 = curIndex - 1 - Op2;
	if (backOp2 >= 255)
	{
		InstList.push_back((Tramp | backOp2 << 8));
		MarkUsed.push_back(false);
		backOp2 = 0;
		backOp1++;
		curIndex++;
	}

	unsigned backOp3 = curIndex - 1 - Op3;
	if (backOp3 >= 256)
	{
		InstList.push_back(Tramp | (backOp3 << 8));
		MarkUsed.push_back(false);
		backOp3 = 0;
		backOp2++;
		backOp1++;
		curIndex++;
	}

	InstList.push_back(Opcode | (backOp1 << 8) | (backOp2 << 16) | (backOp3 << 24));
	MarkUsed.push_back(false);
	return curIndex;
}
#endif

unsigned IRBuilder::ComputeKnownZeroBits(InstLoc I) const
{
	switch (getOpcode(*I))
	{
	case Load8:
		return 0xFFFFFF00;
	case Or:
		return ComputeKnownZeroBits(getOp1(I)) &
		       ComputeKnownZeroBits(getOp2(I));
	case And:
		return ComputeKnownZeroBits(getOp1(I)) |
		       ComputeKnownZeroBits(getOp2(I));
	case Shl:
		if (isImm(*getOp2(I)))
		{
			unsigned samt = GetImmValue(getOp2(I)) & 31;
			return (ComputeKnownZeroBits(getOp1(I)) << samt) |
			        ~(-1U << samt);
		}
		return 0;
	case Shrl:
		if (isImm(*getOp2(I)))
		{
			unsigned samt = GetImmValue(getOp2(I)) & 31;
			return (ComputeKnownZeroBits(getOp1(I)) >> samt) |
			        ~(-1U >> samt);
		}
		return 0;
	case Rol:
		if (isImm(*getOp2(I)))
		{
			return _rotl(ComputeKnownZeroBits(getOp1(I)),
			             GetImmValue(getOp2(I)));
		}
	default:
		return 0;
	}
}

InstLoc IRBuilder::FoldZeroOp(unsigned Opcode, unsigned extra)
{
	if (Opcode == LoadGReg)
	{
		// Reg load folding: if we already loaded the value,
		// load it again
		if (!GRegCache[extra])
			GRegCache[extra] = EmitZeroOp(LoadGReg, extra);
		return GRegCache[extra];
	}
	else if (Opcode == LoadFReg)
	{
		// Reg load folding: if we already loaded the value,
		// load it again
		if (!FRegCache[extra])
			FRegCache[extra] = EmitZeroOp(LoadFReg, extra);
		return FRegCache[extra];
	}
	else if (Opcode == LoadFRegDENToZero)
	{
		FRegCacheStore[extra] = nullptr; // prevent previous store operation from zapping
		FRegCache[extra] = EmitZeroOp(LoadFRegDENToZero, extra);
		return FRegCache[extra];
	}
	else if (Opcode == LoadCarry)
	{
		if (!CarryCache)
			CarryCache = EmitZeroOp(LoadCarry, extra);
		return CarryCache;
	}
	else if (Opcode == LoadCR)
	{
		if (!CRCache[extra])
			CRCache[extra] = EmitZeroOp(LoadCR, extra);
		return CRCache[extra];
	}
	else if (Opcode == LoadCTR)
	{
		if (!CTRCache)
			CTRCache = EmitZeroOp(LoadCTR, extra);
		return CTRCache;
	}

	return EmitZeroOp(Opcode, extra);
}

InstLoc IRBuilder::FoldUOp(unsigned Opcode, InstLoc Op1, unsigned extra)
{
	if (Opcode == StoreGReg)
	{
		// Reg store folding: save the value for load folding.
		// If there's a previous store, zap it because it's dead.
		GRegCache[extra] = Op1;
		if (GRegCacheStore[extra])
			*GRegCacheStore[extra] = 0;

		GRegCacheStore[extra] = EmitUOp(StoreGReg, Op1, extra);
		return GRegCacheStore[extra];
	}
	else if (Opcode == StoreFReg)
	{
		FRegCache[extra] = Op1;
		if (FRegCacheStore[extra])
			*FRegCacheStore[extra] = 0;

		FRegCacheStore[extra] = EmitUOp(StoreFReg, Op1, extra);
		return FRegCacheStore[extra];
	}
	else if (Opcode == StoreCarry)
	{
		CarryCache = Op1;
		if (CarryCacheStore)
			*CarryCacheStore = 0;

		CarryCacheStore = EmitUOp(StoreCarry, Op1, extra);
		return CarryCacheStore;
	}
	else if (Opcode == StoreCR)
	{
		CRCache[extra] = Op1;
		if (CRCacheStore[extra])
			*CRCacheStore[extra] = 0;

		CRCacheStore[extra] = EmitUOp(StoreCR, Op1, extra);
		return CRCacheStore[extra];
	}
	else if (Opcode == StoreCTR)
	{
		CTRCache = Op1;
		if (CTRCacheStore)
			*CTRCacheStore = 0;

		CTRCacheStore = EmitUOp(StoreCTR, Op1, extra);
		return CTRCacheStore;
	}
	else if (Opcode == CompactMRegToPacked)
	{
		if (getOpcode(*Op1) == ExpandPackedToMReg)
			return getOp1(Op1);
	}
	else if (Opcode == DoubleToSingle)
	{
		if (getOpcode(*Op1) == DupSingleToMReg)
			return getOp1(Op1);

		if (getOpcode(*Op1) >= FDMul && getOpcode(*Op1) <= FDSub)
		{
			InstLoc OOp1 = getOp1(Op1), OOp2 = getOp2(Op1);
			if (getOpcode(*OOp1) == DupSingleToMReg &&
			    getOpcode(*OOp2) == DupSingleToMReg)
			{
				if (getOpcode(*Op1) == FDMul)
					return FoldBiOp(FSMul, getOp1(OOp1), getOp2(OOp2));
				else if (getOpcode(*Op1) == FDAdd)
					return FoldBiOp(FSAdd, getOp1(OOp1), getOp2(OOp2));
				else if (getOpcode(*Op1) == FDSub)
					return FoldBiOp(FSSub, getOp1(OOp1), getOp2(OOp2));
			}
		}
	}
	else if (Opcode == Not)
	{
		if (getOpcode(*Op1) == Not)
		{
			return getOp1(Op1);
		}
	}
	else if (Opcode == FastCRGTSet)
	{
		if (getOpcode(*Op1) == ICmpCRSigned)
			return EmitICmpSgt(getOp1(Op1), getOp2(Op1));
		if (getOpcode(*Op1) == ICmpCRUnsigned)
			return EmitICmpUgt(getOp1(Op1), getOp2(Op1));
		if (isImm(*Op1))
			return EmitIntConst((s64)GetImmValue64(Op1) > 0);
	}
	else if (Opcode == FastCRLTSet)
	{
		if (getOpcode(*Op1) == ICmpCRSigned)
			return EmitICmpSlt(getOp1(Op1), getOp2(Op1));
		if (getOpcode(*Op1) == ICmpCRUnsigned)
			return EmitICmpUlt(getOp1(Op1), getOp2(Op1));
		if (isImm(*Op1))
			return EmitIntConst(!!(GetImmValue64(Op1) & (1ull << 62)));
	}
	else if (Opcode == FastCREQSet)
	{
		if (getOpcode(*Op1) == ICmpCRSigned || getOpcode(*Op1) == ICmpCRUnsigned)
			return EmitICmpEq(getOp1(Op1), getOp2(Op1));
		if (isImm(*Op1))
			return EmitIntConst((GetImmValue64(Op1) & 0xFFFFFFFFU) == 0);
	}

	return EmitUOp(Opcode, Op1, extra);
}

// Fold Add opcode. Some rules are ported from LLVM
InstLoc IRBuilder::FoldAdd(InstLoc Op1, InstLoc Op2)
{
	simplifyCommutative(Add, Op1, Op2);

	// i0 + i1 => (i0 + i1)
	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) + GetImmValue(Op2));
	}

	// x + 0 => x
	if (isImm(*Op2) && GetImmValue(Op2) == 0)
	{
		return Op1;
	}

	// x + (y - x) --> y
	if (getOpcode(*Op2) == Sub && isSameValue(Op1, getOp2(Op2)))
	{
		return getOp1(Op2);
	}

	// (x - y) + y => x
	if (getOpcode(*Op1) == Sub && isSameValue(getOp2(Op1), Op2))
	{
		return getOp1(Op1);
	}

	if (InstLoc negOp1 = isNeg(Op1))
	{
		//// TODO: Test the folding below
		//// -A + -B  -->  -(A + B)
		//if (InstLoc negOp2 = isNeg(Op2))
		//{
		//	return FoldSub(EmitIntConst(0), FoldAdd(negOp1, negOp2));
		//}

		// -A + B  -->  B - A
		return FoldSub(Op2, negOp1);
	}

	// A + -B  -->  A - B
	if (InstLoc negOp2 = isNeg(Op2))
	{
		return FoldSub(Op1, negOp2);
	}

	// (x * i0) + x => x * (i0 + 1)
	if (getOpcode(*Op1) == Mul && isImm(*getOp2(Op1)) && isSameValue(getOp1(Op1), Op2))
	{
		return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(getOp2(Op1)) + 1));
	}

	//// TODO: Test the folding below
	//// (x * i0) + (x * i1) => x * (i0 + i1)
	//if (getOpcode(*Op1) == Mul && getOpcode(*Op2) == Mul && isSameValue(getOp1(Op1), getOp1(Op2)) && isImm(*getOp2(Op1)) && isImm(*getOp2(Op2)))
	//{
	//	return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(getOp2(Op1)) + GetImmValue(getOp2(Op2))));
	//}

	// x + x * i0 => x * (i0 + 1)
	if (getOpcode(*Op2) == Mul && isImm(*getOp2(Op2)) && isSameValue(Op1, getOp1(Op2)))
	{
		return FoldMul(Op1, EmitIntConst(GetImmValue(getOp2(Op2)) + 1));
	}

	// w * x + y * z => w * (x + z) iff w == y
	if (getOpcode(*Op1) == Mul && getOpcode(*Op2) == Mul)
	{
		InstLoc w = getOp1(Op1);
		InstLoc x = getOp2(Op1);
		InstLoc y = getOp1(Op2);
		InstLoc z = getOp2(Op2);

		if (!isSameValue(w, y))
		{
			if (isSameValue(w, z))
			{
				std::swap(y, z);
			}
			else if (isSameValue(y, x))
			{
				std::swap(w, x);
			}
			else if (isSameValue(x, z))
			{
				std::swap(y, z);
				std::swap(w, x);
			}
		}

		if (isSameValue(w, y))
		{
			return FoldMul(w, FoldAdd(x, z));
		}
	}

	return EmitBiOp(Add, Op1, Op2);
}

// Fold Sub opcode. Some rules are ported from LLVM
InstLoc IRBuilder::FoldSub(InstLoc Op1, InstLoc Op2)
{
	// (x - x) => 0
	if (isSameValue(Op1, Op2))
	{
		return EmitIntConst(0);
	}

	// x - (-A) => x + A
	if (InstLoc negOp2 = isNeg(Op2))
	{
		return FoldAdd(Op1, negOp2);
	}

	// (x - i0) => x + -i0
	if (isImm(*Op2))
	{
		return FoldAdd(Op1, EmitIntConst(-GetImmValue(Op2)));
	}

	if (getOpcode(*Op2) == Add)
	{
		// x - (x + y) => -y
		if (isSameValue(Op1, getOp1(Op2)))
		{
			return FoldSub(EmitIntConst(0), getOp2(Op2));
		}

		// x - (y + x) => -y
		if (isSameValue(Op1, getOp2(Op2)))
		{
			return FoldSub(EmitIntConst(0), getOp1(Op2));
		}

		// i0 - (x + i1) => (i0 - i1) - x
		if (isImm(*Op1) && isImm(*getOp2(Op2)))
		{
			return FoldSub(EmitIntConst(GetImmValue(Op1) - GetImmValue(getOp2(Op2))), getOp1(Op2));
		}
	}

	//// TODO: Test the folding below
	//// 0 - (C << X)  -> (-C << X)
	//if (isImm(*Op1) && GetImmValue(Op1) == 0 && getOpcode(*Op2) == Shl && isImm(*getOp1(Op2)))
	//{
	//	return FoldShl(EmitIntConst(-GetImmValue(getOp1(Op2))), getOp2(Op2));
	//}

	//// TODO: Test the folding below
	//// x - x * i0 = x * (1 - i0)
	//if (getOpcode(*Op2) == Mul && isImm(*getOp2(Op2)) && isSameValue(Op1, getOp1(Op2)))
	//{
	//	return FoldMul(Op1, EmitIntConst(1 - GetImmValue(getOp2(Op2))));
	//}

	if (getOpcode(*Op1) == Add)
	{
		// (x + y) - x => y
		if (isSameValue(getOp1(Op1), Op2))
		{
			return getOp2(Op1);
		}

		// (x + y) - y => x
		if (isSameValue(getOp2(Op1), Op2))
		{
			return getOp1(Op1);
		}
	}

	//if (getOpcode(*Op1) == Sub)
	//{
	//	// TODO: Test the folding below
	//	// (x - y) - x => -y
	//	if (isSameValue(getOp1(Op1), Op2))
	//	{
	//		return FoldSub(EmitIntConst(0), getOp2(Op1));
	//	}
	//}

	if (getOpcode(*Op1) == Mul)
	{
		// x * i0 - x => x * (i0 - 1)
		if (isImm(*getOp2(Op1)) && isSameValue(getOp1(Op1), Op2))
		{
			return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(getOp2(Op1)) - 1));
		}

		//// TODO: Test the folding below
		//// x * i0 - x * i1 => x * (i0 - i1)
		//if (getOpcode(*Op2) == Mul && isSameValue(getOp1(Op1), getOp1(Op2)) && isImm(*getOp2(Op1)) && isImm(*getOp2(Op2)))
		//{
		//	return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(getOp2(Op1)) + GetImmValue(getOp2(Op2))));
		//}
	}

	// (x + i0) - (y + i1) => (x - y) + (i0 - i1)
	if (getOpcode(*Op1) == Add && getOpcode(*Op2) == Add && isImm(*getOp2(Op1)) && isImm(*getOp2(Op2)))
	{
		return FoldAdd(FoldSub(getOp1(Op1), getOp1(Op2)), EmitIntConst(GetImmValue(getOp2(Op1)) - GetImmValue(getOp2(Op2))));
	}

	// w * x - y * z => w * (x - z) iff w == y
	if (getOpcode(*Op1) == Mul && getOpcode(*Op2) == Mul)
	{
		InstLoc w = getOp1(Op1);
		InstLoc x = getOp2(Op1);
		InstLoc y = getOp1(Op2);
		InstLoc z = getOp2(Op2);

		if (!isSameValue(w, y))
		{
			if (isSameValue(w, z))
			{
				std::swap(y, z);
			}
			else if (isSameValue(y, x))
			{
				std::swap(w, x);
			}
			else if (isSameValue(x, z))
			{
				std::swap(y, z);
				std::swap(w, x);
			}
		}

		if (isSameValue(w, y))
		{
			return FoldMul(w, FoldSub(x, z));
		}
	}

	return EmitBiOp(Sub, Op1, Op2);
}

// Fold Mul opcode. Some rules are ported from LLVM
InstLoc IRBuilder::FoldMul(InstLoc Op1, InstLoc Op2)
{
	simplifyCommutative(Mul, Op1, Op2);

	// i0 * i1 => (i0 * i1)
	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) * GetImmValue(Op2));
	}

	// (x << i0) * i1 => x * (i1 << i0)
	if (getOpcode(*Op1) == Shl && isImm(*getOp2(Op1)) && isImm(*Op2))
	{
		return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(Op2) << GetImmValue(getOp2(Op1))));
	}

	if (isImm(*Op2))
	{
		const unsigned imm = GetImmValue(Op2);

		// x * 0 => 0
		if (imm == 0)
		{
			return EmitIntConst(0);
		}

		// x * -1 => 0 - x
		if (imm == -1U)
		{
			return FoldSub(EmitIntConst(0), Op1);
		}

		for (unsigned i0 = 0; i0 < 30; ++i0)
		{
			// x * (1 << i0) => x << i0
			// One "shl" is faster than one "imul".
			if (imm == (1U << i0))
			{
				return FoldShl(Op1, EmitIntConst(i0));
			}
		}
	}

	// (x + i0) * i1 => x * i1 + i0 * i1
	// The later format can be folded by other rules, again.
	if (getOpcode(*Op1) == Add && isImm(*getOp2(Op1)) && isImm(*Op2))
	{
		return FoldAdd(FoldMul(getOp1(Op1), Op2), EmitIntConst(GetImmValue(getOp2(Op1)) * GetImmValue(Op2)));
	}

	//// TODO: Test the folding below
	//// -X * -Y => X * Y
	//if (InstLoc negOp1 = isNeg(Op1))
	//{
	//	if (InstLoc negOp2 = isNeg(Op2))
	//	{
	//		return FoldMul(negOp1, negOp2);
	//	}
	//}

	//// TODO: Test the folding below
	//// x * (1 << y) => x << y
	//if (getOpcode(*Op2) == Shl && isImm(*getOp1(Op2)) && GetImmValue(getOp1(Op2)) == 1)
	//{
	//	return FoldShl(Op1, getOp2(Op2));
	//}

	//// TODO: Test the folding below
	//// (1 << y) * x => x << y
	//if (getOpcode(*Op1) == Shl && isImm(*getOp1(Op1)) && GetImmValue(getOp1(Op1)) == 1)
	//{
	//	return FoldShl(Op2, getOp2(Op1));
	//}

	// x * y (where y is 0 or 1) => (0 - y) & x
	if (ComputeKnownZeroBits(Op2) == -2U)
	{
		return FoldAnd(FoldSub(EmitIntConst(0), Op2), Op1);
	}

	// x * y (where y is 0 or 1) => (0 - x) & y
	if (ComputeKnownZeroBits(Op1) == -2U)
	{
		return FoldAnd(FoldSub(EmitIntConst(0), Op1), Op2);
	}

	return EmitBiOp(Mul, Op1, Op2);
}

InstLoc IRBuilder::FoldMulHighUnsigned(InstLoc Op1, InstLoc Op2)
{
	// (i0 * i1) >> 32
	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst((u32)(((u64)GetImmValue(Op1) * (u64)GetImmValue(Op2)) >> 32));
	}

	if (isImm(*Op1) && !isImm(*Op2))
	{
		return FoldMulHighUnsigned(Op2, Op1);
	}

	if (isImm(*Op2))
	{
		const unsigned imm = GetImmValue(Op2);

		// (x * 0) >> 32 => 0
		if (imm == 0)
		{
			return EmitIntConst(0);
		}

		for (unsigned i0 = 0; i0 < 30; ++i0)
		{
			// (x * (1 << i0)) => x >> (32 - i0)
			// One "shl" is faster than one "imul".
			if (imm == (1U << i0))
			{
				return FoldShrl(Op1, EmitIntConst(32 - i0));
			}
		}
	}

	return EmitBiOp(MulHighUnsigned, Op1, Op2);
}

InstLoc IRBuilder::FoldAnd(InstLoc Op1, InstLoc Op2)
{
	simplifyCommutative(And, Op1, Op2);

	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) & GetImmValue(Op2));
	}

	if (isImm(*Op2))
	{
		if (!GetImmValue(Op2))
			return EmitIntConst(0);

		if (GetImmValue(Op2) == -1U)
			return Op1;

		if (getOpcode(*Op1) == And && isImm(*getOp2(Op1)))
		{
			unsigned RHS = GetImmValue(Op2) & GetImmValue(getOp2(Op1));
			return FoldAnd(getOp1(Op1), EmitIntConst(RHS));
		}
		else if (getOpcode(*Op1) == Rol && isImm(*getOp2(Op1)))
		{
			unsigned shiftMask1 = -1U << (GetImmValue(getOp2(Op1)) & 31);

			if (GetImmValue(Op2) == shiftMask1)
				return FoldShl(getOp1(Op1), getOp2(Op1));

			unsigned shiftAmt2 = ((32 - GetImmValue(getOp2(Op1))) & 31);
			unsigned shiftMask2 = -1U >> shiftAmt2;

			if (GetImmValue(Op2) == shiftMask2)
			{
				return FoldShrl(getOp1(Op1), EmitIntConst(shiftAmt2));
			}
		}

		if (!(~ComputeKnownZeroBits(Op1) & ~GetImmValue(Op2)))
		{
			return Op1;
		}

		//if (getOpcode(*Op1) == Xor || getOpcode(*Op1) == Or)
		//{
		//	// TODO: Test the folding below
		//	// (x op y) & z => (x & z) op y if (y & z) == 0
		//	if ((~ComputeKnownZeroBits(getOp2(Op1)) & ~ComputeKnownZeroBits(Op2)) == 0)
		//	{
		//		return FoldBiOp(getOpcode(*Op1), FoldAnd(getOp1(Op1), Op2), getOp2(Op1));
		//	}

		//	// TODO: Test the folding below
		//	// (x op y) & z => (y & z) op x if (x & z) == 0
		//	if ((~ComputeKnownZeroBits(getOp1(Op1)) & ~ComputeKnownZeroBits(Op2)) == 0)
		//	{
		//		return FoldBiOp(getOpcode(*Op1), FoldAnd(getOp2(Op1), Op2), getOp1(Op1));
		//	}
		//}
	}

	//// TODO: Test the folding below
	//// (x >> z) & (y >> z) => (x & y) >> z
	//if (getOpcode(*Op1) == Shrl && getOpcode(*Op2) == Shrl && isSameValue(getOp2(Op1), getOp2(Op2)))
	//{
	//	return FoldShl(FoldAnd(getOp1(Op1), getOp2(Op1)), getOp2(Op1));
	//}

	//// TODO: Test the folding below
	//// ((A | N) + B) & AndRHS -> (A + B) & AndRHS iff N&AndRHS == 0
	//// ((A ^ N) + B) & AndRHS -> (A + B) & AndRHS iff N&AndRHS == 0
	//// ((A | N) - B) & AndRHS -> (A - B) & AndRHS iff N&AndRHS == 0
	//// ((A ^ N) - B) & AndRHS -> (A - B) & AndRHS iff N&AndRHS == 0
	//if ((getOpcode(*Op1) == Add || getOpcode(*Op1) == Sub) &&
	//    (getOpcode(*getOp1(Op1)) == Or || getOpcode(*getOp1(Op1)) == Xor))
	//{
	//	const InstLoc A = getOp1(getOp1(Op1));
	//	const InstLoc N = getOp2(getOp1(Op1));
	//	const InstLoc B = getOp2(Op1);
	//	const InstLoc AndRHS = Op2;
	//	if ((~ComputeKnownZeroBits(N) & ~ComputeKnownZeroBits(AndRHS)) == 0)
	//	{
	//		return FoldAnd(FoldBiOp(getOpcode(*Op1), A, B), AndRHS);
	//	}
	//}

	//// TODO: Test the folding below
	//// (~A & ~B) == (~(A | B)) - De Morgan's Law
	//if (InstLoc notOp1 = isNot(Op1))
	//{
	//	if (InstLoc notOp2 = isNot(Op2))
	//	{
	//		return FoldXor(EmitIntConst(-1U), FoldOr(notOp1, notOp2));
	//	}
	//}

	//// TODO: Test the folding below
	//// (X^C)|Y -> (X|Y)^C iff Y&C == 0
	//if (getOpcode(*Op1) == Xor && isImm(*getOp2(Op1)) && (~ComputeKnownZeroBits(Op2) & GetImmValue(getOp2(Op1))) == 0)
	//{
	//	return FoldXor(FoldOr(getOp1(Op1), Op2), getOp2(Op1));
	//}

	if (Op1 == Op2)
		return Op1;

	return EmitBiOp(And, Op1, Op2);
}

InstLoc IRBuilder::FoldOr(InstLoc Op1, InstLoc Op2)
{
	simplifyCommutative(Or, Op1, Op2);

	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) | GetImmValue(Op2));
	}

	if (isImm(*Op2))
	{
		if (!GetImmValue(Op2))
			return Op1;

		if (GetImmValue(Op2) == -1U)
			return EmitIntConst(-1U);

		if (getOpcode(*Op1) == Or && isImm(*getOp2(Op1)))
		{
			unsigned RHS = GetImmValue(Op2) | GetImmValue(getOp2(Op1));

			return FoldOr(getOp1(Op1), EmitIntConst(RHS));
		}

		// (X & C1) | C2 --> (X | C2) & (C1|C2)
		// iff (C1 & C2) == 0.
		if (getOpcode(*Op1) == And && isImm(*getOp2(Op1)) && (GetImmValue(getOp2(Op1)) & GetImmValue(Op2)) == 0)
		{
			return FoldAnd(FoldOr(getOp1(Op1), Op2), EmitIntConst(GetImmValue(getOp2(Op1)) | GetImmValue(Op2)));
		}

		// (X ^ C1) | C2 --> (X | C2) ^ (C1&~C2)
		if (getOpcode(*Op1) == Xor && isImm(*getOp2(Op1)) && isImm(*Op2))
		{
			return FoldXor(FoldOr(getOp1(Op1), Op2), EmitIntConst(GetImmValue(getOp2(Op1)) & ~GetImmValue(Op2)));
		}
	}

	// (~A | ~B) == (~(A & B)) - De Morgan's Law
	if (getOpcode(*Op1) == Not && getOpcode(*Op2) == Not)
	{
		return EmitNot(FoldAnd(getOp1(Op1), getOp1(Op2)));
	}

	if (Op1 == Op2)
		return Op1;

	return EmitBiOp(Or, Op1, Op2);
}

static unsigned ICmpInverseOp(unsigned op)
{
	switch (op)
	{
	case ICmpEq:
		return ICmpNe;
	case ICmpNe:
		return ICmpEq;
	case ICmpUlt:
		return ICmpUge;
	case ICmpUgt:
		return ICmpUle;
	case ICmpUle:
		return ICmpUgt;
	case ICmpUge:
		return ICmpUlt;
	case ICmpSlt:
		return ICmpSge;
	case ICmpSgt:
		return ICmpSle;
	case ICmpSle:
		return ICmpSgt;
	case ICmpSge:
		return ICmpSlt;
	default:
		PanicAlert("Bad opcode");
		return Nop;
	}
}

InstLoc IRBuilder::FoldXor(InstLoc Op1, InstLoc Op2)
{
	simplifyCommutative(Xor, Op1, Op2);

	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) ^ GetImmValue(Op2));
	}

	if (isImm(*Op2))
	{
		if (!GetImmValue(Op2))
			return Op1;

		if (GetImmValue(Op2) == 0xFFFFFFFFU)
		{
			return EmitNot(Op1);
		}

		if (getOpcode(*Op1) == Xor && isImm(*getOp2(Op1)))
		{
			unsigned RHS = GetImmValue(Op2) ^
				       GetImmValue(getOp2(Op1));
			return FoldXor(getOp1(Op1), EmitIntConst(RHS));
		}

		if (isICmp(getOpcode(*Op1)) && GetImmValue(Op2) == 1)
		{
			return FoldBiOp(ICmpInverseOp(getOpcode(*Op1)), getOp1(Op1), getOp2(Op1));
		}
	}

	if (Op1 == Op2)
		return EmitIntConst(0);

	return EmitBiOp(Xor, Op1, Op2);
}

InstLoc IRBuilder::FoldShl(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op2))
	{
		// Shl x 0 => x
		if (!GetImmValue(Op2))
		{
			return Op1;
		}

		if (isImm(*Op1))
			return EmitIntConst(GetImmValue(Op1) << (GetImmValue(Op2) & 31));

		// ((x * i0) << i1) == x * (i0 << i1)
		if (getOpcode(*Op1) == Mul && isImm(*getOp2(Op1)))
		{
			return FoldMul(getOp1(Op1), EmitIntConst(GetImmValue(getOp2(Op1)) << GetImmValue(Op2)));
		}
	}

	// 0 << x => 0
	if (isImm(*Op1) && GetImmValue(Op1) == 0)
	{
		return EmitIntConst(0);
	}

	return EmitBiOp(Shl, Op1, Op2);
}

InstLoc IRBuilder::FoldShrl(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op1) && isImm(*Op2))
	{
		return EmitIntConst(GetImmValue(Op1) >> (GetImmValue(Op2) & 31));
	}

	return EmitBiOp(Shrl, Op1, Op2);
}

InstLoc IRBuilder::FoldRol(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op2))
	{
		if (isImm(*Op1))
			return EmitIntConst(_rotl(GetImmValue(Op1), GetImmValue(Op2)));

		if (!(GetImmValue(Op2) & 31))
			return Op1;
	}
	return EmitBiOp(Rol, Op1, Op2);
}

InstLoc IRBuilder::FoldBranchCond(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op1))
	{
		if (GetImmValue(Op1))
			return EmitBranchUncond(Op2);

		return nullptr;
	}

	return EmitBiOp(BranchCond, Op1, Op2);
}

InstLoc IRBuilder::FoldICmp(unsigned Opcode, InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op1))
	{
		if (isImm(*Op2))
		{
			unsigned result = 0;
			switch (Opcode)
			{
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
				result = (signed)GetImmValue(Op1) >  (signed)GetImmValue(Op2);
				break;
			case ICmpSlt:
				result = (signed)GetImmValue(Op1) <  (signed)GetImmValue(Op2);
				break;
			case ICmpSge:
				result = (signed)GetImmValue(Op1) >= (signed)GetImmValue(Op2);
				break;
			case ICmpSle:
				result = (signed)GetImmValue(Op1) <= (signed)GetImmValue(Op2);
				break;
			}
			return EmitIntConst(result);
		}
		switch (Opcode)
		{
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

InstLoc IRBuilder::FoldICmpCRSigned(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op1) && isImm(*Op2))
	{
		s64 diff = (s64)(s32)GetImmValue(Op1) - (s64)(s32)GetImmValue(Op2);
		return EmitIntConst64((u64)diff);
	}

	return EmitBiOp(ICmpCRSigned, Op1, Op2);
}

InstLoc IRBuilder::FoldICmpCRUnsigned(InstLoc Op1, InstLoc Op2)
{
	if (isImm(*Op1) && isImm(*Op2))
	{
		u64 diff = (u64)GetImmValue(Op1) - (u64)GetImmValue(Op2);
		return EmitIntConst64(diff);
	}

	return EmitBiOp(ICmpCRUnsigned, Op1, Op2);
}

InstLoc IRBuilder::FoldFallBackToInterpreter(InstLoc Op1, InstLoc Op2)
{
	for (unsigned i = 0; i < 32; i++)
	{
		GRegCache[i] = nullptr;
		GRegCacheStore[i] = nullptr;
		FRegCache[i] = nullptr;
		FRegCacheStore[i] = nullptr;
	}

	CarryCache = nullptr;
	CarryCacheStore = nullptr;

	for (unsigned i = 0; i < 8; i++)
	{
		CRCache[i] = nullptr;
		CRCacheStore[i] = nullptr;
	}

	CTRCache = nullptr;
	CTRCacheStore = nullptr;
	return EmitBiOp(FallBackToInterpreter, Op1, Op2);
}

InstLoc IRBuilder::FoldDoubleBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2)
{
	if (getOpcode(*Op1) == InsertDoubleInMReg)
	{
		return FoldDoubleBiOp(Opcode, getOp1(Op1), Op2);
	}

	if (getOpcode(*Op2) == InsertDoubleInMReg)
	{
		return FoldDoubleBiOp(Opcode, Op1, getOp1(Op2));
	}

	return EmitBiOp(Opcode, Op1, Op2);
}

InstLoc IRBuilder::FoldBiOp(unsigned Opcode, InstLoc Op1, InstLoc Op2, unsigned extra)
{
	switch (Opcode)
	{
		case Add:
			return FoldAdd(Op1, Op2);
		case Sub:
			return FoldSub(Op1, Op2);
		case Mul:
			return FoldMul(Op1, Op2);
		case MulHighUnsigned:
			return FoldMulHighUnsigned(Op1, Op2);
		case And:
			return FoldAnd(Op1, Op2);
		case Or:
			return FoldOr(Op1, Op2);
		case Xor:
			return FoldXor(Op1, Op2);
		case Shl:
			return FoldShl(Op1, Op2);
		case Shrl:
			return FoldShrl(Op1, Op2);
		case Rol:
			return FoldRol(Op1, Op2);
		case BranchCond:
			return FoldBranchCond(Op1, Op2);
		case ICmpEq: case ICmpNe:
		case ICmpUgt: case ICmpUlt: case ICmpUge: case ICmpUle:
		case ICmpSgt: case ICmpSlt: case ICmpSge: case ICmpSle:
			return FoldICmp(Opcode, Op1, Op2);
		case ICmpCRSigned:
			return FoldICmpCRSigned(Op1, Op2);
		case ICmpCRUnsigned:
			return FoldICmpCRUnsigned(Op1, Op2);
		case FallBackToInterpreter:
			return FoldFallBackToInterpreter(Op1, Op2);
		case FDMul:
		case FDAdd:
		case FDSub:
			return FoldDoubleBiOp(Opcode, Op1, Op2);
		default:
			return EmitBiOp(Opcode, Op1, Op2, extra);
	}
}

InstLoc IRBuilder::EmitIntConst64(u64 value)
{
	InstLoc curIndex = InstList.data() + InstList.size();
	InstList.push_back(CInt32 | ((unsigned int)ConstList.size() << 8));
	MarkUsed.push_back(false);
	ConstList.push_back(value);
	return curIndex;
}

u64 IRBuilder::GetImmValue64(InstLoc I) const
{
	return ConstList[*I >> 8];
}

void IRBuilder::SetMarkUsed(InstLoc I)
{
	const unsigned i = (unsigned)(I - InstList.data());
	MarkUsed[i] = true;
}

bool IRBuilder::IsMarkUsed(InstLoc I) const
{
	const unsigned i = (unsigned)(I - InstList.data());
	return MarkUsed[i];
}

bool IRBuilder::isSameValue(InstLoc Op1, InstLoc Op2) const
{
	if (Op1 == Op2)
	{
		return true;
	}

	if (isImm(*Op1) && isImm(*Op2) && GetImmValue(Op1) == GetImmValue(Op2))
	{
		return true;
	}

	if (getNumberOfOperands(Op1) == 2 && getOpcode(*Op1) != StorePaired && getOpcode(*Op1) == getOpcode(*Op2) &&
	    isSameValue(getOp1(Op1), getOp1(Op2)) && isSameValue(getOp2(Op1), getOp2(Op2)))
	{
		return true;
	}

	return false;
}

// Assign a complexity or rank value to Inst
// Ported from InstructionCombining.cpp in LLVM
// 0 -> undef
// 1 -> Nop, Const
// 2 -> ZeroOp
// 3 -> UOp
// 4 -> BiOp
unsigned IRBuilder::getComplexity(InstLoc I) const
{
	const unsigned Opcode = getOpcode(*I);
	if (Opcode == Nop || Opcode == CInt16 || Opcode == CInt32)
	{
		return 1;
	}

	const unsigned numberOfOperands = getNumberOfOperands(I);
	if (numberOfOperands == -1U)
	{
		return 0;
	}

	return numberOfOperands + 2;
}


unsigned IRBuilder::getNumberOfOperands(InstLoc I) const
{
	static unsigned numberOfOperands[256];
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;
		std::fill_n(numberOfOperands, sizeof(numberOfOperands) / sizeof(numberOfOperands[0]), -1U);

		numberOfOperands[Nop] = 0;
		numberOfOperands[CInt16] = 0;
		numberOfOperands[CInt32] = 0;

		static unsigned ZeroOp[] = { LoadCR, LoadLink, LoadMSR, LoadGReg, LoadCTR, InterpreterBranch, LoadCarry, RFIExit, LoadFReg, LoadFRegDENToZero, LoadGQR, Int3, };
		static unsigned UOp[] = { StoreLink, BranchUncond, StoreCR, StoreMSR, StoreFPRF, StoreGReg, StoreCTR, Load8, Load16, Load32, SExt16, SExt8, Cntlzw, Not, StoreCarry, SystemCall, ShortIdleLoop, LoadSingle, LoadDouble, LoadPaired, StoreFReg, DupSingleToMReg, DupSingleToPacked, ExpandPackedToMReg, CompactMRegToPacked, FSNeg, FDNeg, FPDup0, FPDup1, FPNeg, DoubleToSingle, StoreGQR, StoreSRR, ConvertFromFastCR, ConvertToFastCR, FastCRSOSet, FastCREQSet, FastCRGTSet, FastCRLTSet, };
		static unsigned BiOp[] = { BranchCond, IdleBranch, And, Xor, Sub, Or, Add, Mul, Rol, Shl, Shrl, Sarl, ICmpEq, ICmpNe, ICmpUgt, ICmpUlt, ICmpSgt, ICmpSlt, ICmpSge, ICmpSle, Store8, Store16, Store32, ICmpCRSigned, ICmpCRUnsigned, FallBackToInterpreter, StoreSingle, StoreDouble, StorePaired, InsertDoubleInMReg, FSMul, FSAdd, FSSub, FDMul, FDAdd, FDSub, FPAdd, FPMul, FPSub, FPMerge00, FPMerge01, FPMerge10, FPMerge11, FDCmpCR, };
		for (auto& op : ZeroOp)
			numberOfOperands[op] = 0;

		for (auto& op : UOp)
			numberOfOperands[op] = 1;

		for (auto& op : BiOp)
			numberOfOperands[op] = 2;
	}

	return numberOfOperands[getOpcode(*I)];
}

// Performs a few simplifications for commutative operators
// Ported from InstructionCombining.cpp in LLVM
void IRBuilder::simplifyCommutative(unsigned Opcode, InstLoc& Op1, InstLoc& Op2)
{
	// Order operands such that they are listed from right (least complex) to
	// left (most complex).  This puts constants before unary operators before
	// binary operators.
	if (getComplexity(Op1) < getComplexity(Op2))
	{
		std::swap(Op1, Op2);
	}

	// Is this associative?
	switch (Opcode)
	{
		case Add:
		case Mul:
		case And:
		case Or:
		case Xor:
			break;
		default:
			return;
	}

	// (V op C1) op C2 => V + (C1 + C2)
	if (getOpcode(*Op1) == Opcode && isImm(*getOp2(Op1)) && isImm(*Op2))
	{
		const InstLoc Op1Old = Op1;
		const InstLoc Op2Old = Op2;
		Op1 = getOp1(Op1Old);
		Op2 = FoldBiOp(Opcode, getOp2(Op1Old), Op2Old);
	}

	// ((V1 op C1) op (V2 op C2)) => ((V1 op V2) op (C1 op C2))
	// Transform: (op (op V1, C1), (op V2, C2)) ==> (op (op V1, V2), (op C1,C2))
	if (getOpcode(*Op1) == Opcode && isImm(*getOp2(Op1)) && getOpcode(*Op2) == Opcode && isImm(*getOp2(Op2)))
	{
		const InstLoc Op1Old = Op1;
		const InstLoc Op2Old = Op2;
		Op1 = FoldBiOp(Opcode, getOp1(Op1Old), getOp1(Op2Old));
		Op2 = FoldBiOp(Opcode, getOp2(Op1Old), getOp2(Op2Old));
	}

	// FIXME: Following code has a bug.
	// ((w op x) op (y op z)) => (((w op x) op y) op z)
	/*
	if (getOpcode(*Op1) == Opcode && getOpcode(*Op2) == Opcode)
	{
		// Sort the operands where the complexities will be descending order.
		std::pair<unsigned, InstLoc> ops[4];
		ops[0] = std::make_pair(getComplexity(getOp1(Op1)), getOp1(Op1));
		ops[1] = std::make_pair(getComplexity(getOp2(Op1)), getOp2(Op1));
		ops[2] = std::make_pair(getComplexity(getOp1(Op2)), getOp1(Op2));
		ops[3] = std::make_pair(getComplexity(getOp2(Op2)), getOp2(Op2));
		std::sort(ops, ops + 4, std::greater<std::pair<unsigned, InstLoc> >());

		Op1 = FoldBiOp(Opcode, FoldBiOp(Opcode, ops[0].second, ops[1].second), ops[2].second);
		Op2 = ops[3].second;
	}
	*/
}

bool IRBuilder::maskedValueIsZero(InstLoc Op1, InstLoc Op2) const
{
	return (~ComputeKnownZeroBits(Op1) & ~ComputeKnownZeroBits(Op2)) == 0;
}

// Returns I' if I == (0 - I')
InstLoc IRBuilder::isNeg(InstLoc I) const
{
	if (getOpcode(*I) == Sub && isImm(*getOp1(I)) && GetImmValue(getOp1(I)) == 0)
	{
		return getOp2(I);
	}

	return nullptr;
}

// TODO: Move the following code to a separated file.
struct Writer
{
	File::IOFile file;
	Writer() : file(nullptr)
	{
		std::string filename = StringFromFormat("JitIL_IR_%d.txt", (int)time(nullptr));
		file.Open(filename, "w");
		setvbuf(file.GetHandle(), nullptr, _IOFBF, 1024 * 1024);
	}

	virtual ~Writer() {}
};

static std::unique_ptr<Writer> writer;

static const std::string opcodeNames[] = {
	"Nop", "LoadGReg", "LoadLink", "LoadCR", "LoadCarry", "LoadCTR",
	"LoadMSR", "LoadGQR", "SExt8", "SExt16", "BSwap32", "BSwap16", "Cntlzw",
	"Not", "Load8", "Load16", "Load32", "BranchUncond", "ConvertFromFastCR",
	"ConvertToFastCR", "StoreGReg",	"StoreCR", "StoreLink", "StoreCarry",
	"StoreCTR", "StoreMSR", "StoreFPRF", "StoreGQR", "StoreSRR",
	"FastCRSOSet", "FastCREQSet", "FastCRGTSet", "FastCRLTSet",
	"FallBackToInterpreter", "Add", "Mul", "And", "Or",	"Xor",
	"MulHighUnsigned", "Sub", "Shl", "Shrl", "Sarl", "Rol",
	"ICmpCRSigned", "ICmpCRUnsigned", "ICmpEq", "ICmpNe", "ICmpUgt",
	"ICmpUlt", "ICmpUge", "ICmpUle", "ICmpSgt", "ICmpSlt", "ICmpSge",
	"ICmpSle", "Store8", "Store16", "Store32", "BranchCond", "FResult_Start",
	"LoadSingle", "LoadDouble", "LoadPaired", "DoubleToSingle",
	"DupSingleToMReg", "DupSingleToPacked", "InsertDoubleInMReg",
	"ExpandPackedToMReg", "CompactMRegToPacked", "LoadFReg",
	"LoadFRegDENToZero", "FSMul", "FSAdd", "FSSub", "FSNeg", "FSRSqrt",
	"FPAdd", "FPMul", "FPSub", "FPNeg", "FDMul", "FDAdd", "FDSub", "FDNeg",
	"FPMerge00", "FPMerge01", "FPMerge10", "FPMerge11", "FPDup0", "FPDup1",
	"FResult_End", "StorePaired", "StoreSingle", "StoreDouble", "StoreFReg",
	"FDCmpCR", "CInt16", "CInt32", "SystemCall", "RFIExit",
	"InterpreterBranch", "IdleBranch", "ShortIdleLoop",
	"FPExceptionCheckStart", "FPExceptionCheckEnd", "ExtExceptionCheck",
	"Tramp", "BlockStart", "BlockEnd", "Int3",
};
static const unsigned alwaysUsedList[] = {
	FallBackToInterpreter, StoreGReg, StoreCR, StoreLink, StoreCTR, StoreMSR,
	StoreGQR, StoreSRR, StoreCarry, StoreFPRF, Load8, Load16, Load32, Store8,
	Store16, Store32, StoreSingle, StoreDouble, StorePaired, StoreFReg, FDCmpCR,
	BlockStart, BlockEnd, IdleBranch, BranchCond, BranchUncond, ShortIdleLoop,
	SystemCall, InterpreterBranch, RFIExit, FPExceptionCheck,
	DSIExceptionCheck, ExtExceptionCheck, BreakPointCheck,
	Int3, Tramp, Nop
};
static const unsigned extra8RegList[] = {
	LoadGReg, LoadCR, LoadGQR, LoadFReg, LoadFRegDENToZero,
};
static const unsigned extra16RegList[] = {
	StoreGReg, StoreCR, StoreGQR, StoreSRR, LoadPaired, StoreFReg,
};
static const unsigned extra24RegList[] = {
	StorePaired,
};

static const std::set<unsigned> alwaysUseds(alwaysUsedList, alwaysUsedList + sizeof(alwaysUsedList) / sizeof(alwaysUsedList[0]));
static const std::set<unsigned> extra8Regs(extra8RegList, extra8RegList + sizeof(extra8RegList) / sizeof(extra8RegList[0]));
static const std::set<unsigned> extra16Regs(extra16RegList, extra16RegList + sizeof(extra16RegList) / sizeof(extra16RegList[0]));
static const std::set<unsigned> extra24Regs(extra24RegList, extra24RegList + sizeof(extra24RegList) / sizeof(extra24RegList[0]));

void IRBuilder::WriteToFile(u64 codeHash)
{
	_assert_(sizeof(opcodeNames) / sizeof(opcodeNames[0]) == Int3 + 1);

	if (!writer.get())
	{
		writer = std::make_unique<Writer>();
	}

	FILE* const file = writer->file.GetHandle();
	fprintf(file, "\ncode hash:%016" PRIx64 "\n", codeHash);

	const InstLoc lastCurReadPtr = curReadPtr;
	StartForwardPass();
	const unsigned numInsts = getNumInsts();
	for (unsigned int i = 0; i < numInsts; ++i)
	{
		const InstLoc I = ReadForward();
		const unsigned opcode = getOpcode(*I);
		const bool thisUsed = IsMarkUsed(I) ||
			alwaysUseds.find(opcode) != alwaysUseds.end();

		// Line number
		fprintf(file, "%4u", i);

		if (!thisUsed)
			fprintf(file, "%*c", 32, ' ');

		// Opcode
		const std::string& opcodeName = opcodeNames[opcode];
		fprintf(file, " %-20s", opcodeName.c_str());
		const unsigned numberOfOperands = getNumberOfOperands(I);

		// Op1
		if (numberOfOperands >= 1)
		{
			const IREmitter::InstLoc inst = getOp1(I);

			if (isImm(*inst))
				fprintf(file, " 0x%08x", GetImmValue(inst));
			else
				fprintf(file, " %10u", i - (unsigned int)(I - inst));
		}

		// Op2
		if (numberOfOperands >= 2)
		{
			const IREmitter::InstLoc inst = getOp2(I);

			if (isImm(*inst))
				fprintf(file, " 0x%08x", GetImmValue(inst));
			else
				fprintf(file, " %10u", i - (unsigned int)(I - inst));
		}

		if (extra8Regs.count(opcode))
			fprintf(file, " R%d", *I >> 8);

		if (extra16Regs.count(opcode))
			fprintf(file, " R%d", *I >> 16);

		if (extra24Regs.count(opcode))
			fprintf(file, " R%d", *I >> 24);

		if (opcode == CInt32 || opcode == CInt16)
			fprintf(file, " 0x%08x", GetImmValue(I));

		fprintf(file, "\n");
	}

	curReadPtr = lastCurReadPtr;
}

}
