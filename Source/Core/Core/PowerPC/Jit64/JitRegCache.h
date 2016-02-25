// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>
#include <map>

#include "Common/x64Emitter.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/Jit64/Jit64_RegCache.h"

enum FlushMode
{
	FLUSH_ALL,
	FLUSH_MAINTAIN_STATE,
};

#define NUMXREGS 16

template <Jit64Reg::Type type>
class RegCache
{
public:
	RegCache(Jit64Reg::RegisterClass<type>& r): regs(r) {}
	virtual ~RegCache() {}

	void FlushLockX(Gen::X64Reg reg)
	{
		lockedX.emplace(reg, regs.Borrow(reg));
		lockedX.find(reg)->second.ForceRealize();
	}
	void FlushLockX(Gen::X64Reg reg1, Gen::X64Reg reg2)
	{
		FlushLockX(reg1);
		FlushLockX(reg2);
	}

	void Flush(FlushMode mode = FLUSH_ALL, BitSet32 regsToFlush = BitSet32::AllTrue(32))
	{
		if (regsToFlush == BitSet32::AllTrue(32))
		{
			if (mode == FLUSH_ALL)
			{
				regs.Flush();
			}
			else
			{
				regs.Sync();
			}
		}
		else
		{
			if (mode == FLUSH_MAINTAIN_STATE)
			{
				for (int reg : regsToFlush)
				{
					regs.Lock(reg).Sync();
				}
			}
			else
			{
				_assert_msg_(REGCACHE, 0, "uhoh");
			}
		}
	}

	void KillImmediate(size_t preg, bool doLoad, bool makeDirty)
	{
		if (!doLoad)
		{
			_assert_msg_(REGCACHE, 0, "KillImmediate() doLoad must be true");
			return;
		}

		Gen::OpArg loc = R(preg);
		if (loc.IsImm())
		{
			BindToRegister(preg, doLoad, makeDirty);
		}
		else if (loc.IsSimpleReg())
		{
			if (makeDirty)
				BindToRegister(preg, false, true);
		}
	}

	//TODO - instead of doload, use "read", "write"
	//read only will not set dirty flag
	void BindToRegister(size_t preg, bool doLoad = true, bool makeDirty = true)
	{
		Jit64Reg::BindMode mode;

		if (doLoad)
		{
			if (makeDirty)
				mode = Jit64Reg::BindMode::ReadWrite;
			else
				mode = Jit64Reg::BindMode::Read;
		}
		else
		{
			if (makeDirty)
				mode = Jit64Reg::BindMode::Write;
			else
				mode = Jit64Reg::BindMode::Reuse;
		}

		auto it = locked.find(preg);
		if (it == locked.end())
		{
			// Registers can be bound without being locked, but they could get bumped
			auto reg = regs.Lock(preg);
			reg.Bind(mode).ForceRealize();
			return;
		}

		auto bind = it->second.Bind(mode);
		locked.erase(preg);
		locked.emplace(preg, bind);
		bind.ForceRealize();
	}

	void StoreFromRegister(size_t preg, FlushMode mode = FLUSH_ALL)
	{
		auto reg = regs.Lock(preg);

		if (mode == FLUSH_ALL)
			reg.Flush();
		else
			reg.Sync();
	}

	const Gen::OpArg R(size_t preg)
	{
		auto it = locked.find(preg);
		if (it == locked.end())
		{
			// temp lock
			return regs.Lock(preg);
		}

		return (Gen::OpArg)it->second;
	}

	Gen::X64Reg RX(size_t preg)
	{
		auto it = locked.find(preg);
		if (it == locked.end())
		{
			// This is valid, but kind of dangerous
			auto reg = regs.Lock(preg);
			return ((Gen::OpArg)reg).GetSimpleReg();
		}

		return ((Gen::OpArg)it->second).GetSimpleReg();
	}

	virtual Gen::OpArg GetDefaultLocation(size_t reg) const = 0;

	// Register locking.

	// these are powerpc reg indices
	template<typename T>
	void Lock(T p)
	{
		locked.emplace(p, regs.Lock(p));
		locked.find(p)->second.ForceRealize();
	}
	template<typename T, typename... Args>
	void Lock(T first, Args... args)
	{
		Lock(first);
		Lock(args...);
	}

	template<typename T>
	void UnlockX(T x)
	{
		if (lockedX.erase(x) == 0)
			_assert_msg_(REGCACHE, 0, "UnlockX() Register was not locked");
	}
	template<typename T, typename... Args>
	void UnlockX(T first, Args... args)
	{
		UnlockX(first);
		UnlockX(args...);
	}

	void UnlockAll()
	{
		locked.clear();
	}

	void UnlockAllX()
	{
		lockedX.clear();
	}

	bool IsBound(size_t preg)
	{
		auto it = locked.find(preg);
		if (it == locked.end())
		{
			_assert_msg_(REGCACHE, 0, "IsBound() Register was not locked");
			return false;
		}

		return it->second.IsRegBound();
	}

	Gen::X64Reg GetFreeXReg()
	{
		// Assumes that this will be properly locked immediately after
		// Note that we don't allow scratch regs here as old code won't expect it
		auto xr = regs.Borrow(Gen::INVALID_REG, false);
		return xr;
	}

protected:
	Jit64Reg::RegisterClass<type>& regs;
	std::map<int, Jit64Reg::Any<type>> locked;
	std::map<int, Jit64Reg::Native<type>> lockedX;
};

class GPRRegCache final : public RegCache<Jit64Reg::Type::GPR>
{
public:
	GPRRegCache(Jit64Reg::RegisterClass<Jit64Reg::Type::GPR>& r): RegCache(r) {}
	Gen::OpArg GetDefaultLocation(size_t reg) const override;
	void SetImmediate32(size_t preg, u32 immValue)
	{
		auto it = locked.find(preg);
		if (it == locked.end())
		{
			// Not an error to set without locking
			this->regs.Lock(preg).SetImm32(immValue);
			return;
		}

		it->second.SetImm32(immValue);
	}
};


class FPURegCache final : public RegCache<Jit64Reg::Type::FPU>
{
public:
	FPURegCache(Jit64Reg::RegisterClass<Jit64Reg::Type::FPU>& r): RegCache(r) {}
	Gen::OpArg GetDefaultLocation(size_t reg) const override;
};
