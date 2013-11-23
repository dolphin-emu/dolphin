// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _JIT64REGCACHE_H
#define _JIT64REGCACHE_H

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

	virtual const int *GetAllocationOrder(int &count) = 0;

	XEmitter *emit;

public:
	RegCache();

	virtual ~RegCache() {}
	virtual void Start(PPCAnalyst::BlockRegStats &stats) = 0;

	void DiscardRegContentsIfCached(int preg);
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
	int SanityCheck() const;
	void KillImmediate(int preg, bool doLoad, bool makeDirty);

	//TODO - instead of doload, use "read", "write"
	//read only will not set dirty flag
	virtual void BindToRegister(int preg, bool doLoad = true, bool makeDirty = true) = 0;
	virtual void StoreFromRegister(int preg) = 0;

	const OpArg &R(int preg) const {return regs[preg].location;}
	X64Reg RX(int preg) const
	{
		if (IsBound(preg))
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

	bool IsFreeX(int xreg) const
	{
		return xregs[xreg].free && !xlocks[xreg];
	}

	bool IsBound(int preg) const
	{
		return regs[preg].away && regs[preg].location.IsSimpleReg();
	}


	X64Reg GetFreeXReg();

	void SaveState();
	void LoadState();
};

class GPRRegCache : public RegCache
{
public:
	void Start(PPCAnalyst::BlockRegStats &stats) override;
	void BindToRegister(int preg, bool doLoad = true, bool makeDirty = true) override;
	void StoreFromRegister(int preg) override;
	OpArg GetDefaultLocation(int reg) const override;
	const int *GetAllocationOrder(int &count) override;
	void SetImmediate32(int preg, u32 immValue);
};


class FPURegCache : public RegCache
{
public:
	void Start(PPCAnalyst::BlockRegStats &stats) override;
	void BindToRegister(int preg, bool doLoad = true, bool makeDirty = true) override;
	void StoreFromRegister(int preg) override;
	const int *GetAllocationOrder(int &count) override;
	OpArg GetDefaultLocation(int reg) const override;
};

#endif  // _JIT64REGCACHE_H
