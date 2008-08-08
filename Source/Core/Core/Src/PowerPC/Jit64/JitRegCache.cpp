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
#include "../PowerPC.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"
#include "Jit.h"
#include "JitCache.h"
#include "JitAsm.h"
#include "JitRegCache.h"


using namespace Gen;
using namespace PowerPC;

namespace Jit64
{
	GPRRegCache gpr;
	FPURegCache fpr;
	
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
		int maxPreload = 3;
		for (int i = 0; i < 32; i++)
		{
			if (stats.numReads[i] > 2 || stats.numWrites[i] >= 2)
			{
				LoadToX64(i, true, false); //, stats.firstRead[i] <= stats.firstWrite[i], false);
				maxPreload--;
				if (!maxPreload)
					break;
			}
		}
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
			PanicAlert("RegCache: x %i already locked!");
		}
		xlocks[x1] = true;
		if (x2 != 0xFF) xlocks[x2] = true;
		if (x3 != 0xFF) xlocks[x3] = true;
		if (x4 != 0xFF) xlocks[x4] = true;
	}

	bool RegCache::IsFreeX(int xreg) const
	{
		return xregs[xreg].free && !xlocks[xreg];
	}

	void RegCache::UnlockAll()
	{
		for (int i = 0; i < 32; i++)
			locks[i] = false;
	}

	void RegCache::UnlockAllX()
	{
		for (int i = 0; i < 16; i++)
			xlocks[i] = false;
	}
	
	X64Reg RegCache::GetFreeXReg()
	{
		int aCount;
		const int *aOrder = GetAllocationOrder(aCount);
		int i;
		for (i = 0; i < aCount; i++)
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
				StoreFromX64(preg);
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
		if (!xregs[reg].free)
		{
			StoreFromX64(xregs[reg].ppcReg);
		}
	}

	void RegCache::SanityCheck() const
	{
		for (int i = 0; i < 32; i++) {
			if (regs[i].away) {
				if (regs[i].location.IsSimpleReg()) {
					Gen::X64Reg simple = regs[i].location.GetSimpleReg();
					if (xlocks[simple]) {
						PanicAlert("%08x : PPC Reg %i is in locked x64 register %i", js.compilerPC, i, regs[i].location.GetSimpleReg());
					}
					if (xregs[simple].ppcReg != i) {
						PanicAlert("%08x : Xreg/ppcreg mismatch");
					}
				}
			}
		}
	}

	void GPRRegCache::GetReadyForOp(int dest, int source)
	{
		if (regs[dest].location.CanDoOpWith(regs[source].location))
			return;
		LoadToX64(dest);
		if (!regs[dest].location.CanDoOpWith(regs[source].location))
		{
			_assert_msg_(DYNA_REC, 0, "GetReadyForOp failed");
		}
	}

	void FPURegCache::GetReadyForOp(int dest, int source)
	{
		if (regs[dest].location.IsSimpleReg())
			return;
		LoadToX64(dest); //all fp ops have reg as destination
		if (!regs[dest].location.CanDoOpWith(regs[source].location))
		{
			_assert_msg_(DYNA_REC, 0, "GetReadyForOp failed");
		}
	}


	bool GPRRegCache::IsXRegVolatile(X64Reg reg) const
	{
#ifdef _WIN32
		switch (reg)
		{
		case RAX: case RCX: case RDX: case R8: case R9: case R10: case R11:
#ifdef _M_IX86
		case RBX:
#endif
			return true;
		default:
			return false;
		}
#else
		return true;
#endif
	}

	void RegCache::DiscardRegContentsIfCached(int preg)
	{
		if (regs[preg].away && regs[preg].location.IsSimpleReg())
		{
			xregs[regs[preg].location.GetSimpleReg()].free = true;
			regs[preg].away = false;
		}
	}


	void GPRRegCache::SetImmediate32(int preg, u32 immValue)
	{
		DiscardRegContentsIfCached(preg);
		regs[preg].away = true;
		regs[preg].location = Imm32(immValue);
	}

	bool FPURegCache::IsXRegVolatile(X64Reg reg) const
	{
#ifdef _WIN32
		if (reg < 6) 
			return true;
		else
			return false;
#else
		return true;
#endif
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
#ifdef _M_X64
#ifdef _WIN32
			RSI, RDI, R12, R13, R14, R8, R9, RDX, R10, R11 //, RCX
#else
			R12, R13, R14, R8, R9, R10, R11, RSI, RDI //, RCX
#endif
#elif _M_IX86
			ESI, EDI, EBX, EBP, EDX //, RCX
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

	// eheh, this was a dupe.
	void RegCache::KillImmediate(int preg)
	{
		if (regs[preg].away && regs[preg].location.IsImm())
		{
			StoreFromX64(preg);
		}
	}

	void GPRRegCache::LoadToX64(int i, bool doLoad, bool makeDirty)
	{
		if (!regs[i].away || (regs[i].away && regs[i].location.IsImm()))
		{
			X64Reg xr = GetFreeXReg();
			if (xlocks[xr]) PanicAlert("GetFreeXReg returned locked register");
			xregs[xr].free = false;
			xregs[xr].ppcReg = i;
			xregs[xr].dirty = makeDirty;
			OpArg newloc = ::Gen::R(xr);
			if (doLoad || regs[i].location.IsImm())
				MOV(32, newloc, regs[i].location);
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
			// HERE HOLDS: regs[i].away == true
			//             
			//reg location must be simplereg
			xregs[RX(i)].dirty |= makeDirty;
		}
		if (xlocks[RX(i)]) {
			PanicAlert("Seriously WTF, this reg should have been flushed");
		}
	}

	void GPRRegCache::StoreFromX64(int i)
	{
		if (regs[i].away)
		{
			if (regs[i].location.IsSimpleReg())
			{
				X64Reg xr = RX(i);
				xregs[xr].free = true;
				xregs[xr].ppcReg = -1;
				xregs[xr].dirty = false;
			}
			else
			{
				//must be immediate - do nothing
			}
			OpArg newLoc = GetDefaultLocation(i);
			MOV(32, newLoc, regs[i].location);
			regs[i].location = newLoc;
			regs[i].away = false;
		}
	}

	void FPURegCache::LoadToX64(int i, bool doLoad, bool makeDirty)
	{
		_assert_msg_(DYNA_REC, !regs[i].location.IsImm(), "WTF - load - imm");
		if (!regs[i].away)
		{
			X64Reg xr = GetFreeXReg();
			_assert_msg_(DYNA_REC, xr >= 0 && xr < NUMXREGS, "WTF - load - invalid reg");
			xregs[xr].ppcReg = i;
			xregs[xr].free = false;
			xregs[xr].dirty = makeDirty;
			OpArg newloc = ::Gen::R(xr);
			if (doLoad)
				MOVAPD(xr, regs[i].location);
			regs[i].location = newloc;
			regs[i].away = true;
		}
	}

	void FPURegCache::StoreFromX64(int i)
	{
		_assert_msg_(DYNA_REC, !regs[i].location.IsImm(), "WTF - store - imm");
		if (regs[i].away)
		{
			X64Reg xr = regs[i].location.GetSimpleReg();
			_assert_msg_(DYNA_REC, xr >= 0 && xr < NUMXREGS, "WTF - store - invalid reg");
			xregs[xr].free = true;
			xregs[xr].dirty = false;
			xregs[xr].ppcReg = -1;
			OpArg newLoc = GetDefaultLocation(i);
			MOVAPD(newLoc, xr);
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
		for (int i = 0; i < 32; i++)
		{
			if (locks[i])
			{
				_assert_msg_(DYNA_REC,0,"Somebody forgot some register locks.");
			}
			if (regs[i].away)
			{
				if (regs[i].location.IsSimpleReg())
				{
					X64Reg xr = RX(i);
					if (mode != FLUSH_VOLATILE || IsXRegVolatile(xr))
					{
						StoreFromX64(i);
					}
					xregs[xr].dirty = false;
				}
				else if (regs[i].location.IsImm())
				{
					StoreFromX64(i);
				}
				else
				{
					_assert_msg_(DYNA_REC,0,"Jit64 - Flush unhandled case, reg %i", i);
				}
			}
		}
	}
}
