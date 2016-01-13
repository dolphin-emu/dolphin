// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/JitRegCache.h"

using namespace Gen;

namespace Jit64Reg 
{

template <Type T>
Native<T> Any<T>::Bind(Jit64Reg::BindMode mode)
{
	RealizeLock();
	_assert_msg_(REGCACHE, m_type == PPC, "Cannot bind a native or immediate register");

	switch (mode)
	{
	case Preload:
	case Read:
		m_reg->BindToRegister(m_val, true, false);
		break;
	case ReadWrite:
		m_reg->BindToRegister(m_val, true, true);
		break;
	case Write:
		m_reg->BindToRegister(m_val, false, true);
		break;
	default:
		_assert_msg_(REGCACHE, 0, "Unhandled bind %d", mode);
		break;
	}

	DEBUG_LOG(REGCACHE, "Bind %d to %d", m_val, m_reg->m_regs[m_val].location.GetSimpleReg());
	return Native<T>(m_reg, mode == Preload, PPC, m_reg->m_regs[m_val].location.GetSimpleReg());
}

template <Type T>
void Any<T>::LoadIfNotImmediate()
{
	RealizeLock();
	if (!IsRegBound())
	{
		m_reg->BindToRegister(m_val, true, false);
		DEBUG_LOG(REGCACHE, "LoadIfNotImmediate bind %d to %d", m_val, m_reg->m_regs[m_val].location.GetSimpleReg());
	}
}

template <Type T>
void Any<T>::RealizeImmediate()
{
	RealizeLock();
	_assert_msg_(REGCACHE, 0, "TODO RealizeImmediate");	
}

template <Type T>
bool Any<T>::IsRegBound()
{
	auto& ppc = m_reg->m_regs[m_val];
	return ppc.location.IsSimpleReg();
}

template <Type T>
void Any<T>::Unlock()
{
	_assert_msg_(REGCACHE, m_realized, "Locked register was never realized");
	if (m_type == PPC)
	{
		auto& ppc = m_reg->m_regs[m_val];
		ppc.locked--;
		DEBUG_LOG(REGCACHE, "-- Unlock %d (count = %d)", m_val, ppc.locked);
		_assert_msg_(REGCACHE, ppc.locked >= 0, "Register was excessively unlocked");
	}
}

template <Type T>
void Any<T>::SetImm32(u32 imm)
{
	RealizeLock();

	DEBUG_LOG(REGCACHE, "SetImm32 %d = %08x", m_val, imm);
	_assert_msg_(REGCACHE, m_type == PPC, "SetImm32 is only valid on guest registers");

	OpArg loc = *this;
	if (loc.IsSimpleReg())
	{
		X64Reg reg = loc.GetSimpleReg();
		DEBUG_LOG(REGCACHE, "Abandoning %d native reg %d", m_val, reg);
		// Ditch the register, freeing it up
		auto& xreg = m_reg->m_xregs[reg];
		xreg.lock = Free;
		xreg.dirty = false;
		xreg.ppcReg = INVALID_REG;
	}

	auto& ppc = m_reg->m_regs[m_val];
	ppc.away = true;
	ppc.location = Gen::Imm32(imm);
}

template <Type T>
Any<T>::operator Gen::OpArg()
{
	RealizeLock();

	if (m_type == PPC)
	{
		auto& ppc = m_reg->m_regs[m_val];
		return ppc.location;
	}

	if (m_type == Imm)
	{
		return Gen::Imm32(m_val);
	}

	_assert_msg_(REGCACHE, 0, "Unhandled type %d", m_type);
	return OpArg();
}

template <Type T>
void Any<T>::RealizeLock()
{
	if (!m_realized)
	{
		if (m_type == PPC)
		{
			auto& ppc = m_reg->m_regs[m_val];
			ppc.locked++;
			DEBUG_LOG(REGCACHE, "++ Lock %d (count = %d)", m_val, ppc.locked);
		}

		m_realized = true;
	}
}

template <Type T>
OpArg Any<T>::Sync()
{
	RealizeLock();
	_assert_msg_(REGCACHE, 0, "TODO Sync");	
	return OpArg();
}

template <Type T>
void Native<T>::Unlock()
{
	Any<T>::Unlock();
}

template <Type T>
Native<T>::operator Gen::OpArg()
{
	this->RealizeLock();
	return R(m_xreg);
}

template <Type T>
Native<T>::operator Gen::X64Reg()
{
	this->RealizeLock();
	return m_xreg;
}


Any<GPR> RegisterClass<GPR>::Imm32(u32 value)
{
	return Any<GPR>(this, Imm, value);
}


template<Type T>
Native<T> RegisterClassBase<T>::Borrow(Gen::X64Reg which)
{
	if (which == INVALID_REG)
	{
		// This should happen when the lock is realized to give us the best chance at choosing unused regs
		which = GetFreeXReg(true);
		DEBUG_LOG(REGCACHE, "Borrowing register %d (auto)", which);
		return Native<T>(this, false, X64, which);
	}
	DEBUG_LOG(REGCACHE, "Borrowing register %d", which);
	return Native<T>(this, false, X64, which);
}

template<Type T>
void RegisterClassBase<T>::Init()
{
	for (auto& xreg : m_xregs)
	{
		xreg.lock = Free;
		xreg.dirty = false;
		xreg.ppcReg = INVALID_REG;
	}
	for (auto& reg : m_regs)
	{
		reg.location = GetDefaultLocation(&reg - &m_regs[0]);
		reg.away = false;
		reg.locked = 0;
	}
}

template<Type T>
Any<T> RegisterClassBase<T>::Lock(reg_t preg)
{
	return Any<T>(this, PPC, preg);
}



template<Type T>
int RegisterClassBase<T>::NumFreeRegisters()
{
	int count = 0;
	const std::vector<X64Reg>& order = GetAllocationOrder();
	for (auto reg : order)
		if (m_xregs[reg].lock == Free)
			count++;
	return count;
}

template<Type T>
void RegisterClassBase<T>::BindBatch(BitSet32 regs)
{
	DEBUG_LOG(REGCACHE, "Bind batch %08x", regs.m_val);
	for (int reg : regs)
	{
		if (NumFreeRegisters() < 2)
			break;

		auto r = Lock(reg);
		if (!r.IsImm())
			r.Bind(Jit64Reg::Preload);
	}
}

template<Type T>
void RegisterClassBase<T>::FlushBatch(BitSet32 regs)
{
	DEBUG_LOG(REGCACHE, "Flush batch %08x", regs.m_val);
	for (int i : regs)
	{
		auto& reg = m_regs[i];
		_assert_msg_(REGCACHE, !reg.locked, "Someone forgot to unlock PPC reg %u", i);
		if (reg.away)
		{
			_assert_msg_(REGCACHE, reg.location.IsSimpleReg() || reg.location.IsImm(), "Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC); 
			StoreFromRegister(i, FlushAll);
		}
	}
}

template<Type T>
BitSet32 RegisterClassBase<T>::InUse() const
{
	BitSet32 inuse{0};

	for (auto& xreg : m_xregs)
	{
		if (xreg.lock != Free)
		{
			inuse[&xreg - &m_xregs[0]] = true;
		}
	}

	return inuse;
}

template<Type T>
BitSet32 RegisterClassBase<T>::CountRegsIn(reg_t preg, u32 lookahead)
{
	BitSet32 regsUsed;
	for (u32 i = 1; i < lookahead; i++)
	{
		BitSet32 regsIn = GetRegsIn(i);
		regsUsed |= regsIn;
		if (regsIn[preg])
			return regsUsed;
	}
	return regsUsed;
}

// Estimate roughly how bad it would be to de-allocate this register. Higher score
// means more bad.
template<Type T>
float RegisterClassBase<T>::ScoreRegister(X64Reg xr)
{
	// TODO: we should note that a register has an unrealized lock and score it higher to
	// avoid flushing and reloading it
	size_t preg = m_xregs[xr].ppcReg;
	float score = 0;

	// If it's not dirty, we don't need a store to write it back to the register file, so
	// bias a bit against dirty registers. Testing shows that a bias of 2 seems roughly
	// right: 3 causes too many extra clobbers, while 1 saves very few clobbers relative
	// to the number of extra stores it causes.
	if (m_xregs[xr].dirty)
		score += 2;

	// If the register isn't actually needed in a physical register for a later instruction,
	// writing it back to the register file isn't quite as bad.
	if (GetRegUtilization()[preg])
	{
		// Don't look too far ahead; we don't want to have quadratic compilation times for
		// enormous block sizes!
		// This actually improves register allocation a tiny bit; I'm not sure why.
		u32 lookahead = std::min(m_jit->js.instructionsLeft, 64);
		// Count how many other registers are going to be used before we need this one again.
		u32 regs_in_count = CountRegsIn(preg, lookahead).Count();
		// Totally ad-hoc heuristic to bias based on how many other registers we'll need
		// before this one gets used again.
		score += 1 + 2 * (5 - log2f(1 + (float)regs_in_count));
	}

	return score;
}

template<Type T>
void RegisterClassBase<T>::Flush()
{
	// Flushing will automatically release bound registers, but not borrowed ones
	for (auto& xreg : m_xregs)
	{
		int i = &xreg - &m_xregs[0];
		_assert_msg_(REGCACHE, xreg.lock != Borrowed, "Someone forgot to unlock borrowed X64 reg %u", i);
	}

	for (auto& reg : m_regs)
	{
		int i = &reg - &m_regs[0];
		_assert_msg_(REGCACHE, !reg.locked, "Someone forgot to unlock PPC reg %u", i);
		if (reg.away)
		{
			_assert_msg_(REGCACHE, reg.location.IsSimpleReg() || reg.location.IsImm(), "Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC); 
			StoreFromRegister(i, FlushAll);
		}
	}
}

template<Type T>
X64Reg RegisterClassBase<T>::GetFreeXReg(bool scratch)
{
	if (scratch)
	{
		// Try scratch registers first
		for (X64Reg xr : GetScratchAllocationOrder())
			if (m_xregs[xr].lock == Free)
				return xr;
	}

	// Are there any X64 registers of this type that are currently unlocked 
	// (ie: not borrowed) and free (ie: not bound)?
	for (X64Reg xr : GetAllocationOrder())
		if (m_xregs[xr].lock == Free)
			return xr;

	// Okay, not found; run the register allocator heuristic and figure out which register we should
	// clobber.
	float min_score = std::numeric_limits<float>::max();
	X64Reg best_xreg = INVALID_REG;
	size_t best_preg = 0;
	for (X64Reg xreg : GetAllocationOrder())
	{
		// Borrowed X64 regs aren't available
		if (m_xregs[xreg].lock == Borrowed)
			continue;

		size_t preg = m_xregs[xreg].ppcReg;
		if (m_regs[preg].locked > 0)
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
		StoreFromRegister(best_preg, FlushAll);
		return best_xreg;
	}

	//Still no dice? Die!
	_assert_msg_(REGCACHE, 0, "Regcache ran out of regs");
	return INVALID_REG;
}

template<Type T>
void RegisterClassBase<T>::BindToRegister(reg_t preg, bool doLoad, bool makeDirty)
{
	auto& ppc = m_regs[preg];

	_assert_msg_(REGCACHE, ppc.away || !ppc.location.IsImm(), "Bad immediate");

	if (!ppc.away || (ppc.away && ppc.location.IsImm()))
	{
		X64Reg xr = GetFreeXReg(false);
		auto& xreg = m_xregs[xr];

		// Sanity check
		_assert_msg_(REGCACHE, !xreg.dirty, "Xreg already dirty");
		_assert_msg_(REGCACHE, xreg.lock != Borrowed, "GetFreeXReg returned borrowed register");
		for (PPCCachedReg& reg : m_regs)
			if (reg.location.IsSimpleReg(xr))
				Crash();

		xreg.lock = Bound;
		xreg.ppcReg = preg;
		xreg.dirty = makeDirty || ppc.location.IsImm();
		if (doLoad)
			LoadRegister(xr, ppc.location);

		ppc.away = true;
		ppc.location = R(xr);
	}
	else
	{
		// reg location must be simplereg; memory locations
		// and immediates are taken care of above.
		m_xregs[ppc.location.GetSimpleReg()].dirty |= makeDirty;
	}

	_assert_msg_(REGCACHE, m_xregs[ppc.location.GetSimpleReg()].lock == Bound, "This reg should be bound");
}

template<Type T>
void RegisterClassBase<T>::StoreFromRegister(size_t preg, FlushMode mode)
{
	DEBUG_LOG(REGCACHE, "Flushing %zu (mode = %d)", preg, mode);
	auto& ppc = m_regs[preg];

	if (ppc.away)
	{
		bool doStore;
		if (ppc.location.IsSimpleReg())
		{
			X64Reg xr = ppc.location.GetSimpleReg();
			auto& xreg = m_xregs[xr];
			doStore = xreg.dirty;
			if (mode == FlushAll)
			{
				xreg.lock = Free;
				xreg.ppcReg = INVALID_REG;
				xreg.dirty = false;
			}
		}
		else
		{
			//must be immediate - do nothing
			doStore = true;
		}
		OpArg newLoc = GetDefaultLocation(preg);
		if (doStore)
			StoreRegister(newLoc, ppc.location);
		if (mode == FlushAll)
		{
			ppc.location = newLoc;
			ppc.away = false;
		}
	}
}

template<Type T>
void RegisterClassBase<T>::CheckUnlocked()
{
	for (auto& reg : m_regs)
	{
		int i = &reg - &m_regs[0];
		_assert_msg_(REGCACHE, !reg.locked, "Someone forgot to unlock PPC reg %u", i);
	}
}


Registers Registers::Branch() 
{
	Registers branch{m_emit, m_jit};
	branch.fpu.CopyFrom(fpu);
	branch.gpr.CopyFrom(gpr);
	return branch;
}

void Registers::Init() 
{
	gpr.Init();
	fpu.Init();
}

int Registers::SanityCheck() const
{
	// _assert_msg_(REGCACHE, 0, "TODO SanityCheck");	
	return 0;
}

void Registers::Commit()
{
	// _assert_msg_(REGCACHE, 0, "TODO Commit");	
}

void Registers::Rollback()
{
	// _assert_msg_(REGCACHE, 0, "TODO Rollback");	
}


template class Any<GPR>;
template class Any<FPU>;

template class Native<GPR>;
template class Native<FPU>;

template class RegisterClassBase<GPR>;
template class RegisterClassBase<FPU>;
template class RegisterClass<GPR>;
template class RegisterClass<FPU>;
};
