// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Jit.h"
#include "JitAsm.h"
#include "JitRegCache.h"

using namespace Gen;
using namespace PowerPC;

RegCache::RegCache() : emit(0)
{
	memset(locks, 0, sizeof(locks));
	memset(xlocks, 0, sizeof(xlocks));
	memset(saved_locks, 0, sizeof(saved_locks));
	memset(saved_xlocks, 0, sizeof(saved_xlocks));
	memset(regs, 0, sizeof(regs));
	memset(xregs, 0, sizeof(xregs));
	memset(saved_regs, 0, sizeof(saved_regs));
	memset(saved_xregs, 0, sizeof(saved_xregs));
}

void RegCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	for (int i = 0; i < NUMXREGS; i++)
	{
		xregs[i].free = true;
		xregs[i].dirty = false;
		xlocks[i] = false;
	}
	for (int i = 0; i < 32; i++)
	{
		regs[i].location = GetDefaultLocation(i);
		regs[i].away = false;
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
	locks[p1] = true;
	if (p2 != 0xFF) locks[p2] = true;
	if (p3 != 0xFF) locks[p3] = true;
	if (p4 != 0xFF) locks[p4] = true;
}

// these are x64 reg indices
void RegCache::LockX(int x1, int x2, int x3, int x4)
{
	if (xlocks[x1]) {
		PanicAlert("RegCache: x %i already locked!", x1);
	}
	xlocks[x1] = true;
	if (x2 != 0xFF) xlocks[x2] = true;
	if (x3 != 0xFF) xlocks[x3] = true;
	if (x4 != 0xFF) xlocks[x4] = true;
}

void RegCache::UnlockAll()
{
	for (auto& lock : locks)
		lock = false;
}

void RegCache::UnlockAllX()
{
	for (auto& xlock : xlocks)
		xlock = false;
}

X64Reg RegCache::GetFreeXReg()
{
	int aCount;
	const int *aOrder = GetAllocationOrder(aCount);
	for (int i = 0; i < aCount; i++)
	{
		X64Reg xr = (X64Reg)aOrder[i];
		if (!xlocks[xr] && xregs[xr].free)
		{
			return (X64Reg)xr;
		}
	}
	//Okay, not found :( Force grab one

	//TODO - add a pass to grab xregs whose ppcreg is not used in the next 3 instructions
	for (int i = 0; i < aCount; i++)
	{
		X64Reg xr = (X64Reg)aOrder[i];
		if (xlocks[xr])
			continue;
		int preg = xregs[xr].ppcReg;
		if (!locks[preg])
		{
			StoreFromRegister(preg);
			return xr;
		}
	}
	//Still no dice? Die!
	_assert_msg_(DYNA_REC, 0, "Regcache ran out of regs");
	return (X64Reg) -1;
}

void RegCache::SaveState()
{
	memcpy(saved_locks, locks, sizeof(locks));
	memcpy(saved_xlocks, xlocks, sizeof(xlocks));
	memcpy(saved_regs, regs, sizeof(regs));
	memcpy(saved_xregs, xregs, sizeof(xregs));
}

void RegCache::LoadState()
{
	memcpy(xlocks, saved_xlocks, sizeof(xlocks));
	memcpy(locks, saved_locks, sizeof(locks));
	memcpy(regs, saved_regs, sizeof(regs));
	memcpy(xregs, saved_xregs, sizeof(xregs));
}

void RegCache::FlushR(X64Reg reg)
{
	if (reg >= NUMXREGS)
		PanicAlert("Flushing non existent reg");
	if (!xregs[reg].free)
	{
		StoreFromRegister(xregs[reg].ppcReg);
	}
}

int RegCache::SanityCheck() const
{
	for (int i = 0; i < 32; i++)
	{
		if (regs[i].away)
		{
			if (regs[i].location.IsSimpleReg())
			{
				Gen::X64Reg simple = regs[i].location.GetSimpleReg();
				if (xlocks[simple])
					return 1;
				if (xregs[simple].ppcReg != i)
					return 2;
			}
			else if (regs[i].location.IsImm())
				return 3;
		}
	}
	return 0;
}

void RegCache::DiscardRegContentsIfCached(int preg)
{
	if (IsBound(preg))
	{
		X64Reg xr = regs[preg].location.GetSimpleReg();
		xregs[xr].free = true;
		xregs[xr].dirty = false;
		xregs[xr].ppcReg = -1;
		regs[preg].away = false;
		regs[preg].location = GetDefaultLocation(preg);
	}
}


void GPRRegCache::SetImmediate32(int preg, u32 immValue)
{
	DiscardRegContentsIfCached(preg);
	regs[preg].away = true;
	regs[preg].location = Imm32(immValue);
}

void GPRRegCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	RegCache::Start(stats);
}

void FPURegCache::Start(PPCAnalyst::BlockRegStats &stats)
{
	RegCache::Start(stats);
}

const int *GPRRegCache::GetAllocationOrder(int &count)
{
	static const int allocationOrder[] =
	{
		// R12, when used as base register, for example in a LEA, can generate bad code! Need to look into this.
#ifdef _M_X64
#ifdef _WIN32
		RSI, RDI, R13, R14, R8, R9, R10, R11, R12, //, RCX
#else
		RBP, R13, R14, R8, R9, R10, R11, R12, //, RCX
#endif
#elif _M_IX86
		ESI, EDI, EBX, EBP, EDX, ECX,
#endif
	};
	count = sizeof(allocationOrder) / sizeof(const int);
	return allocationOrder;
}

const int *FPURegCache::GetAllocationOrder(int &count)
{
	static const int allocationOrder[] =
	{
#ifdef _M_X64
		XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5
#elif _M_IX86
		XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
#endif
	};
	count = sizeof(allocationOrder) / sizeof(int);
	return allocationOrder;
}

OpArg GPRRegCache::GetDefaultLocation(int reg) const
{
	return M(&ppcState.gpr[reg]);
}

OpArg FPURegCache::GetDefaultLocation(int reg) const
{
	return M(&ppcState.ps[reg][0]);
}

void RegCache::KillImmediate(int preg, bool doLoad, bool makeDirty)
{
	if (regs[preg].away)
	{
		if (regs[preg].location.IsImm())
			BindToRegister(preg, doLoad, makeDirty);
		else if (regs[preg].location.IsSimpleReg())
			xregs[RX(preg)].dirty |= makeDirty;
	}
}

void GPRRegCache::BindToRegister(int i, bool doLoad, bool makeDirty)
{
	if (!regs[i].away && regs[i].location.IsImm())
		PanicAlert("Bad immediate");

	if (!regs[i].away || (regs[i].away && regs[i].location.IsImm()))
	{
		X64Reg xr = GetFreeXReg();
		if (xregs[xr].dirty) PanicAlert("Xreg already dirty");
		if (xlocks[xr]) PanicAlert("GetFreeXReg returned locked register");
		xregs[xr].free = false;
		xregs[xr].ppcReg = i;
		xregs[xr].dirty = makeDirty || regs[i].location.IsImm();
		OpArg newloc = ::Gen::R(xr);
		if (doLoad)
			emit->MOV(32, newloc, regs[i].location);
		for (int j = 0; j < 32; j++)
		{
			if (i != j && regs[j].location.IsSimpleReg() && regs[j].location.GetSimpleReg() == xr)
			{
				Crash();
			}
		}
		regs[i].away = true;
		regs[i].location = newloc;
	}
	else
	{
		// reg location must be simplereg; memory locations
		// and immediates are taken care of above.
		xregs[RX(i)].dirty |= makeDirty;
	}

	if (xlocks[RX(i)])
	{
		PanicAlert("Seriously WTF, this reg should have been flushed");
	}
}

void GPRRegCache::StoreFromRegister(int i)
{
	if (regs[i].away)
	{
		bool doStore;
		if (regs[i].location.IsSimpleReg())
		{
			X64Reg xr = RX(i);
			xregs[xr].free = true;
			xregs[xr].ppcReg = -1;
			doStore = xregs[xr].dirty;
			xregs[xr].dirty = false;
		}
		else
		{
			//must be immediate - do nothing
			doStore = true;
		}
		OpArg newLoc = GetDefaultLocation(i);
		if (doStore)
			emit->MOV(32, newLoc, regs[i].location);
		regs[i].location = newLoc;
		regs[i].away = false;
	}
}

void FPURegCache::BindToRegister(int i, bool doLoad, bool makeDirty)
{
	_assert_msg_(DYNA_REC, !regs[i].location.IsImm(), "WTF - load - imm");
	if (!regs[i].away)
	{
		// Reg is at home in the memory register file. Let's pull it out.
		X64Reg xr = GetFreeXReg();
		_assert_msg_(DYNA_REC, xr < NUMXREGS, "WTF - load - invalid reg");
		xregs[xr].ppcReg = i;
		xregs[xr].free = false;
		xregs[xr].dirty = makeDirty;
		OpArg newloc = ::Gen::R(xr);
		if (doLoad)
		{
			if (!regs[i].location.IsImm() && (regs[i].location.offset & 0xF))
			{
				PanicAlert("WARNING - misaligned fp register location %i", i);
			}
			emit->MOVAPD(xr, regs[i].location);
		}
		regs[i].location = newloc;
		regs[i].away = true;
	}
	else
	{
		// There are no immediates in the FPR reg file, so we already had this in a register. Make dirty as necessary.
		xregs[RX(i)].dirty |= makeDirty;
	}
}

void FPURegCache::StoreFromRegister(int i)
{
	_assert_msg_(DYNA_REC, !regs[i].location.IsImm(), "WTF - store - imm");
	if (regs[i].away)
	{
		X64Reg xr = regs[i].location.GetSimpleReg();
		_assert_msg_(DYNA_REC, xr < NUMXREGS, "WTF - store - invalid reg");
		OpArg newLoc = GetDefaultLocation(i);
		if (xregs[xr].dirty)
			emit->MOVAPD(newLoc, xr);
		xregs[xr].free = true;
		xregs[xr].dirty = false;
		xregs[xr].ppcReg = -1;
		regs[i].location = newLoc;
		regs[i].away = false;
	}
	else
	{
	//	_assert_msg_(DYNA_REC,0,"already stored");
	}
}

void RegCache::Flush(FlushMode mode)
{
	for (int i = 0; i < NUMXREGS; i++)
	{
		if (xlocks[i])
			PanicAlert("Someone forgot to unlock X64 reg %i.", i);
	}

	for (int i = 0; i < 32; i++)
	{
		if (locks[i])
		{
			PanicAlert("Someone forgot to unlock PPC reg %i.", i);
		}
		if (regs[i].away)
		{
			if (regs[i].location.IsSimpleReg())
			{
				X64Reg xr = RX(i);
				StoreFromRegister(i);
				xregs[xr].dirty = false;
			}
			else if (regs[i].location.IsImm())
			{
				StoreFromRegister(i);
			}
			else
			{
				_assert_msg_(DYNA_REC,0,"Jit64 - Flush unhandled case, reg %i PC: %08x", i, PC);
			}
		}
	}
}
