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

#ifndef _JITREGCACHE_H
#define _JITREGCACHE_H

#include "x64Emitter.h"

	using namespace Gen;
	enum FlushMode
	{
		FLUSH_ALL
	};

	enum GrabMode
	{
		M_READ = 1,
		M_WRITE = 2, 
		M_READWRITE = 3,
	};

	struct PPCCachedReg
	{
		OpArg location;
		bool away;  // value not in source register
	};

	struct X64CachedReg
	{
		int ppcReg;
		bool dirty;
		bool free;
	};

	typedef int XReg;
	typedef int PReg;

#ifdef _M_X64
#define NUMXREGS 16
#elif _M_IX86
#define NUMXREGS 8
#endif

	class RegCache
	{
	private:
		bool locks[32];
		bool saved_locks[32];
		bool saved_xlocks[NUMXREGS];

	protected:
		bool xlocks[NUMXREGS];
		PPCCachedReg regs[32];
		X64CachedReg xregs[NUMXREGS];

		PPCCachedReg saved_regs[32];
		X64CachedReg saved_xregs[NUMXREGS];

		void DiscardRegContentsIfCached(int preg);
		virtual const int *GetAllocationOrder(int &count) = 0;
		
		XEmitter *emit;

	public:
		virtual ~RegCache() {}
		virtual void Start(PPCAnalyst::BlockRegStats &stats) = 0;

		void SetEmitter(XEmitter *emitter) {emit = emitter;}

		void FlushR(X64Reg reg); 
		void FlushR(X64Reg reg, X64Reg reg2) {FlushR(reg); FlushR(reg2);}
		void FlushLockX(X64Reg reg) {
			FlushR(reg);
			LockX(reg);
		}
		void FlushLockX(X64Reg reg1, X64Reg reg2) {
			FlushR(reg1); FlushR(reg2);
			LockX(reg1); LockX(reg2);
		}
		virtual void Flush(FlushMode mode);
		virtual void Flush(PPCAnalyst::CodeOp *op) {Flush(FLUSH_ALL);}
		void SanityCheck() const;
		void KillImmediate(int preg);

		//TODO - instead of doload, use "read", "write"
		//read only will not set dirty flag
		virtual void LoadToX64(int preg, bool doLoad = true, bool makeDirty = true) = 0;
		virtual void StoreFromX64(int preg) = 0;

		const OpArg &R(int preg) const {return regs[preg].location;}
		X64Reg RX(int preg) const
		{
			if (regs[preg].away && regs[preg].location.IsSimpleReg()) 
				return regs[preg].location.GetSimpleReg(); 
			PanicAlert("Not so simple - %i", preg); 
			return (X64Reg)-1;
		}
		virtual OpArg GetDefaultLocation(int reg) const = 0;

		// Register locking.
		void Lock(int p1, int p2=0xff, int p3=0xff, int p4=0xff);
		void LockX(int x1, int x2=0xff, int x3=0xff, int x4=0xff);
		void UnlockAll();
		void UnlockAllX();

		bool IsFreeX(int xreg) const;

		X64Reg GetFreeXReg();

		void SaveState();
		void LoadState();
	};

	class GPRRegCache : public RegCache
	{
	public:
		void Start(PPCAnalyst::BlockRegStats &stats);
		void LoadToX64(int preg, bool doLoad = true, bool makeDirty = true);
		void StoreFromX64(int preg);
		OpArg GetDefaultLocation(int reg) const;
		const int *GetAllocationOrder(int &count);
		void SetImmediate32(int preg, u32 immValue);
	};


	class FPURegCache : public RegCache
	{
	public:
		void Start(PPCAnalyst::BlockRegStats &stats);
		void LoadToX64(int preg, bool doLoad = true, bool makeDirty = true);
		void StoreFromX64(int preg);
		const int *GetAllocationOrder(int &count);
		OpArg GetDefaultLocation(int reg) const;
	};

#endif

