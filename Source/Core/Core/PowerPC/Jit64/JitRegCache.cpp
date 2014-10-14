// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitAsm.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;
using namespace PowerPC;

RegCache::RegCache() : emit(nullptr)
{
}

void RegCache::Start()
{
	for (auto& xreg : xregs)
	{
		xreg.free = true;
		xreg.dirty = false;
		xreg.locked = false;
		xreg.ppcReg = INVALID_REG;
	}
	for (size_t i = 0; i < regs.size(); i++)
	{
		regs[i].location = GetDefaultLocation(i);
		regs[i].away = false;
		regs[i].locked = false;
	}

	// todo: sort to find the most popular regs
	/*
	int maxPreload = 2;
	for (int i = 0; i < 32; i++)
	{
		if (stats.numReads[i] > 2 || stats.numWrites[i] >= 2)
		{
			LoadToX64(i, true, false); //stats.firstRead[i] <= stats.firstWrite[i], false);
			maxPreload--;
			if (!maxPreload)
				break;
		}
	}*/
	//Find top regs - preload them (load bursts ain't bad)
	//But only preload IF written OR reads >= 3
}

// these are powerpc reg indices
void RegCache::Lock(int p1, int p2, int p3, int p4)
{
	regs[p1].locked = true;

	if (p2 != 0xFF)
		regs[p2].locked = true;

	if (p3 != 0xFF)
		regs[p3].locked = true;

	if (p4 != 0xFF)
		regs[p4].locked = true;
}

// these are x64 reg indices
void RegCache::LockX(int x1, int x2, int x3, int x4)
{
	if (xregs[x1].locked)
	{
		PanicAlert("RegCache: x %i already locked!", x1);
	}

	xregs[x1].locked = true;

	if (x2 != 0xFF)
		xregs[x2].locked = true;

	if (x3 != 0xFF)
		xregs[x3].locked = true;

	if (x4 != 0xFF)
		xregs[x4].locked = true;
}

void RegCache::UnlockAll()
{
	for (auto& reg : regs)
		reg.locked = false;
}

void RegCache::UnlockAllX()
{
	for (auto& xreg : xregs)
		xreg.locked = false;
}

u32 GPRRegCache::GetRegUtilization()
{
	return jit->js.op->gprInReg;
}

u32 FPURegCache::GetRegUtilization()
{
	return jit->js.op->gprInReg;
}

u32 GPRRegCache::CountRegsIn(size_t preg, u32 lookahead)
{
	u32 regsUsed = 0;
	for (u32 i = 1; i < lookahead; i++)
	{
		for (int j = 0; j < 3; j++)
			if (jit->js.op[i].regsIn[j] >= 0)
				regsUsed |= 1 << jit->js.op[i].regsIn[j];
		for (int j = 0; j < 3; j++)
			if ((size_t)jit->js.op[i].regsIn[j] == preg)
				return regsUsed;
	}
	return regsUsed;
}

u32 FPURegCache::CountRegsIn(size_t preg, u32 lookahead)
{
	u32 regsUsed = 0;
	for (u32 i = 1; i < lookahead; i++)
	{
		for (int j = 0; j < 4; j++)
			if (jit->js.op[i].fregsIn[j] >= 0)
				regsUsed |= 1 << jit->js.op[i].fregsIn[j];
		for (int j = 0; j < 4; j++)
			if ((size_t)jit->js.op[i].fregsIn[j] == preg)
				return regsUsed;
	}
	return regsUsed;
}

// Estimate roughly how bad it would be to de-allocate this register. Higher score
// means more bad.
float RegCache::ScoreRegister(X64Reg xr)
{
	size_t preg = xregs[xr].ppcReg;
	float score = 0;

	// If it's not dirty, we don't need a store to write it back to the register file, so
	// bias a bit against dirty registers. Testing shows that a bias of 2 seems roughly
	// right: 3 causes too many extra clobbers, while 1 saves very few clobbers relative
	// to the number of extra stores it causes.
	if (xregs[xr].dirty)
		score += 2;

	// If the register isn't actually needed in a physical register for a later instruction,
	// writing it back to the register file isn't quite as bad.
	if (GetRegUtilization() & (1 << preg))
	{
		// Don't look too far ahead; we don't want to have quadratic compilation times for
		// enormous block sizes!
		// This actually improves register allocation a tiny bit; I'm not sure why.
		u32 lookahead = std::min(jit->js.instructionsLeft, 64);
		// Count how many other registers are going to be used before we need this one again.
		u32 regs_in = CountRegsIn(preg, lookahead);
		u32 regs_in_count = 0;
		for (int i = 0; i < 32; i++)
			regs_in_count += !!(regs_in & (1 << i));
		// Totally ad-hoc heuristic to bias based on how many other registers we'll need
		// before this one gets used again.
		score += 1 + 2 * (5 - log2f(1 + (float)regs_in_count));
	}

	return score;
}

X64Reg RegCache::GetFreeXReg()
{
	size_t aCount;
	const int* aOrder = GetAllocationOrder(aCount);
	for (size_t i = 0; i < aCount; i++)
	{
		X64Reg xr = (X64Reg)aOrder[i];
		if (!xregs[xr].locked && xregs[xr].free)
		{
			return (X64Reg)xr;
		}
	}

	// Okay, not found; run the register allocator heuristic and figure out which register we should
	// clobber.
	float min_score = std::numeric_limits<float>::max();
	X64Reg best_xreg = INVALID_REG;
	size_t best_preg = 0;
	for (size_t i = 0; i < aCount; i++)
	{
		X64Reg xreg = (X64Reg)aOrder[i];
		size_t preg = xregs[xreg].ppcReg;
		if (xregs[xreg].locked || regs[preg].locked)
			continue;
		float score = ScoreRegister(xreg);
		if (score < min_score)
		{
			min_score = score;
			best_xreg = xreg;
			best_preg = preg;
		}
	}

	if (best_xreg != INVALID_REG)
	{
		StoreFromRegister(best_preg);
		return best_xreg;
	}

	//Still no dice? Die!
	_assert_msg_(DYNA_REC, 0, "Regcache ran out of regs");
	return INVALID_REG;
}

void RegCache::FlushR(X64Reg reg)
{
	if (reg >= xregs.size())
		PanicAlert("Flushing non existent reg");
	if (!xregs[reg].free)
	{
		StoreFromRegister(xregs[reg].ppcReg);
	}
}

int RegCache::SanityCheck() const
{
	for (size_t i = 0; i < regs.size(); i++)
	{
		if (regs[i].away)
		{
			if (regs[i].location.IsSimpleReg())
			{
				Gen::X64Reg simple = regs[i].location.GetSimpleReg();
				if (xregs[simple].locked)
					return 1;
				if (xregs[simple].ppcReg != i)
					return 2;
			}
			else if (regs[i].location.IsImm())
			{
				return 3;
			}
		}
	}
	return 0;
}

void RegCache::DiscardRegContentsIfCached(size_t preg)
{
	if (IsBound(preg))
	{
		X64Reg xr = regs[preg].location.GetSimpleReg();
		xregs[xr].free = true;
		xregs[xr].dirty = false;
		xregs[xr].ppcReg = INVALID_REG;
		regs[preg].away = false;
		regs[preg].location = GetDefaultLocation(preg);
	}
}


void GPRRegCache::SetImmediate32(size_t preg, u32 immValue)
{
	DiscardRegContentsIfCached(preg);
	regs[preg].away = true;
	regs[preg].location = Imm32(immValue);
}

const int* GPRRegCache::GetAllocationOrder(size_t& count)
{
	static const int allocationOrder[] =
	{
		// R12, when used as base register, for example in a LEA, can generate bad code! Need to look into this.
#ifdef _WIN32
		RSI, RDI, R13, R14, R8, R9, R10, R11, R12, RCX
#else
		R12, R13, R14, RSI, RDI, R8, R9, R10, R11, RCX
#endif
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}

const int* FPURegCache::GetAllocationOrder(size_t& count)
{
	static const int allocationOrder[] =
	{
		XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5
	};
	count = sizeof(allocationOrder) / sizeof(int);
	return allocationOrder;
}

OpArg GPRRegCache::GetDefaultLocation(size_t reg) const
{
	return PPCSTATE(gpr[reg]);
}

OpArg FPURegCache::GetDefaultLocation(size_t reg) const
{
	return PPCSTATE(ps[reg][0]);
}

void RegCache::KillImmediate(size_t preg, bool doLoad, bool makeDirty)
{
	if (regs[preg].away)
	{
		if (regs[preg].location.IsImm())
			BindToRegister(preg, doLoad, makeDirty);
		else if (regs[preg].location.IsSimpleReg())
			xregs[RX(preg)].dirty |= makeDirty;
	}
}

void RegCache::BindToRegister(size_t i, bool doLoad, bool makeDirty)
{
	if (!regs[i].away && regs[i].location.IsImm())
		PanicAlert("Bad immediate");

	if (!regs[i].away || (regs[i].away && regs[i].location.IsImm()))
	{
		X64Reg xr = GetFreeXReg();
		if (xregs[xr].dirty) PanicAlert("Xreg already dirty");
		if (xregs[xr].locked) PanicAlert("GetFreeXReg returned locked register");
		xregs[xr].free = false;
		xregs[xr].ppcReg = i;
		xregs[xr].dirty = makeDirty || regs[i].location.IsImm();
		if (doLoad)
			LoadRegister(i, xr);
		for (size_t j = 0; j < regs.size(); j++)
		{
			if (i != j && regs[j].location.IsSimpleReg() && regs[j].location.GetSimpleReg() == xr)
			{
				Crash();
			}
		}
		regs[i].away = true;
		regs[i].location = ::Gen::R(xr);
	}
	else
	{
		// reg location must be simplereg; memory locations
		// and immediates are taken care of above.
		xregs[RX(i)].dirty |= makeDirty;
	}

	if (xregs[RX(i)].locked)
	{
		PanicAlert("Seriously WTF, this reg should have been flushed");
	}
}

void RegCache::StoreFromRegister(size_t i, FlushMode mode)
{
	if (regs[i].away)
	{
		bool doStore;
		if (regs[i].location.IsSimpleReg())
		{
			X64Reg xr = RX(i);
			doStore = xregs[xr].dirty;
			if (mode == FLUSH_ALL)
			{
				xregs[xr].free = true;
				xregs[xr].ppcReg = INVALID_REG;
				xregs[xr].dirty = false;
			}
		}
		else
		{
			//must be immediate - do nothing
			doStore = true;
		}
		OpArg newLoc = GetDefaultLocation(i);
		if (doStore)
			StoreRegister(i, newLoc);
		if (mode == FLUSH_ALL)
		{
			regs[i].location = newLoc;
			regs[i].away = false;
		}
	}
}

void GPRRegCache::LoadRegister(size_t preg, X64Reg newLoc)
{
	emit->MOV(32, ::Gen::R(newLoc), regs[preg].location);
}

void GPRRegCache::StoreRegister(size_t preg, OpArg newLoc)
{
	emit->MOV(32, newLoc, regs[preg].location);
}

void FPURegCache::LoadRegister(size_t preg, X64Reg newLoc)
{
	if (!regs[preg].location.IsImm() && (regs[preg].location.offset & 0xF))
	{
		PanicAlert("WARNING - misaligned fp register location %u", (unsigned int) preg);
	}
	emit->MOVAPD(newLoc, regs[preg].location);
}

void FPURegCache::StoreRegister(size_t preg, OpArg newLoc)
{
	emit->MOVAPD(newLoc, regs[preg].location.GetSimpleReg());
}

void RegCache::Flush(FlushMode mode)
{
	for (unsigned int i = 0; i < xregs.size(); i++)
	{
		if (xregs[i].locked)
			PanicAlert("Someone forgot to unlock X64 reg %u", i);
	}

	for (unsigned int i = 0; i < regs.size(); i++)
	{
		if (regs[i].locked)
		{
			PanicAlert("Someone forgot to unlock PPC reg %u (X64 reg %i).", i, RX(i));
		}

		if (regs[i].away)
		{
			if (regs[i].location.IsSimpleReg() || regs[i].location.IsImm())
			{
				StoreFromRegister(i, mode);
			}
			else
			{
				_assert_msg_(DYNA_REC,0,"Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC);
			}
		}
	}
}

int RegCache::NumFreeRegisters()
{
	int count = 0;
	size_t aCount;
	const int* aOrder = GetAllocationOrder(aCount);
	for (size_t i = 0; i < aCount; i++)
		if (!xregs[aOrder[i]].locked && xregs[aOrder[i]].free)
			count++;
	return count;
}
