// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>

#include "Common/x64Emitter.h"
#include "Core/PowerPC/PPCAnalyst.h"

enum FlushMode
{
	FLUSH_ALL,
	FLUSH_MAINTAIN_STATE,
};

struct PPCCachedReg
{
	Gen::OpArg location;
	bool away;  // value not in source register
	bool locked;
};

struct X64CachedReg
{
	size_t ppcReg;
	bool dirty;
	bool free;
	bool locked;
};

typedef int XReg;
typedef int PReg;

#define NUMXREGS 16

class RegCache
{
protected:
	std::array<PPCCachedReg, 32> regs;
	std::array<X64CachedReg, NUMXREGS> xregs;

	virtual const int *GetAllocationOrder(size_t& count) = 0;

	virtual u32 GetRegUtilization() = 0;
	virtual u32 CountRegsIn(size_t preg, u32 lookahead) = 0;

	Gen::XEmitter *emit;

	float ScoreRegister(Gen::X64Reg xreg);

public:
	RegCache();
	virtual ~RegCache() {}

	void Start();

	void DiscardRegContentsIfCached(size_t preg);
	void SetEmitter(Gen::XEmitter *emitter)
	{
		emit = emitter;
	}

	void FlushR(Gen::X64Reg reg);
	void FlushR(Gen::X64Reg reg, Gen::X64Reg reg2)
	{
		FlushR(reg);
		FlushR(reg2);
	}

	void FlushLockX(Gen::X64Reg reg)
	{
		FlushR(reg);
		LockX(reg);
	}
	void FlushLockX(Gen::X64Reg reg1, Gen::X64Reg reg2)
	{
		FlushR(reg1); FlushR(reg2);
		LockX(reg1); LockX(reg2);
	}

	void Flush(FlushMode mode = FLUSH_ALL);
	void Flush(PPCAnalyst::CodeOp *op) {Flush();}
	int SanityCheck() const;
	void KillImmediate(size_t preg, bool doLoad, bool makeDirty);

	//TODO - instead of doload, use "read", "write"
	//read only will not set dirty flag
	void BindToRegister(size_t preg, bool doLoad = true, bool makeDirty = true);
	void StoreFromRegister(size_t preg, FlushMode mode = FLUSH_ALL);
	virtual void StoreRegister(size_t preg, Gen::OpArg newLoc) = 0;
	virtual void LoadRegister(size_t preg, Gen::X64Reg newLoc) = 0;

	const Gen::OpArg &R(size_t preg) const
	{
		return regs[preg].location;
	}

	Gen::X64Reg RX(size_t preg) const
	{
		if (IsBound(preg))
			return regs[preg].location.GetSimpleReg();

		PanicAlert("Not so simple - %u", (unsigned int) preg);
		return Gen::INVALID_REG;
	}
	virtual Gen::OpArg GetDefaultLocation(size_t reg) const = 0;

	// Register locking.
	void Lock(int p1, int p2=0xff, int p3=0xff, int p4=0xff);
	void LockX(int x1, int x2=0xff, int x3=0xff, int x4=0xff);
	void UnlockAll();
	void UnlockAllX();

	bool IsFreeX(size_t xreg) const
	{
		return xregs[xreg].free && !xregs[xreg].locked;
	}

	bool IsBound(size_t preg) const
	{
		return regs[preg].away && regs[preg].location.IsSimpleReg();
	}


	Gen::X64Reg GetFreeXReg();
	int NumFreeRegisters();
};

class GPRRegCache : public RegCache
{
public:
	void StoreRegister(size_t preg, Gen::OpArg newLoc) override;
	void LoadRegister(size_t preg, Gen::X64Reg newLoc) override;
	Gen::OpArg GetDefaultLocation(size_t reg) const override;
	const int* GetAllocationOrder(size_t& count) override;
	void SetImmediate32(size_t preg, u32 immValue);
	u32 GetRegUtilization();
	u32 CountRegsIn(size_t preg, u32 lookahead);
};


class FPURegCache : public RegCache
{
public:
	void StoreRegister(size_t preg, Gen::OpArg newLoc) override;
	void LoadRegister(size_t preg, Gen::X64Reg newLoc) override;
	const int* GetAllocationOrder(size_t& count) override;
	Gen::OpArg GetDefaultLocation(size_t reg) const override;
	u32 GetRegUtilization();
	u32 CountRegsIn(size_t preg, u32 lookahead);
};
