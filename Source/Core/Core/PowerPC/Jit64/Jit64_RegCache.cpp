// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_RegCache.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"

using namespace Gen;

namespace Jit64Reg 
{

template <Type T>
Native<T> Any<T>::Bind(Jit64Reg::BindMode mode)
{
	// Transactions are a no-op when memcheck is off
	if (mode == BindMode::ReadWriteTransactional && !m_reg->m_jit->jo.memcheck)
		mode = BindMode::ReadWrite;
	if (mode == BindMode::WriteTransactional && !m_reg->m_jit->jo.memcheck)
		mode = BindMode::Write;

	// If this register (or an alias) was bound write-only and we are now
	// attempting to read it, barf
	if (mode == BindMode::Read || mode == BindMode::ReadWrite || mode == BindMode::ReadWriteTransactional)
	{
		auto& ppc = m_reg->m_regs[m_data.reg];
		_assert_msg_(REGCACHE, ppc.bindWriteWithoutReadAdvise == 0, "Register was bound as write-only before being bound for read");
		// we can continue here, but it may not be pretty
	}

	_assert_msg_(REGCACHE, m_data.type == RegisterType::PPC || m_data.type == RegisterType::Bind, "Only PPC registers can be bound");
	RealizeLock();
	RegisterData data = { .type = RegisterType::Bind, .mode = mode, .reg = m_data.reg };
	return Native<T>(m_reg, data);
}

template <Type T>
Native<T> Any<T>::BorrowCopy()
{
	// When we implement single tracking for FPU register, this will be valid
	_assert_msg_(REGCACHE, T == Type::GPR, "BorrowCopy only implemented for GPRs");

	// TODO: If we know that the bound register we are borrowing a copy of is
	// no longer used, we could rename instead.
	RealizeLock();
	X64Reg other = m_reg->GetFreeXReg(BitSet32{0});

	if (IsRegBound())
	{
		m_reg->CopyRegister(other, Location().GetSimpleReg());
	}
	else
	{
		m_reg->LoadRegister(other, Location());
	}

	RegisterData data = { .type = RegisterType::Borrow, .xreg = m_data.xreg, .disallowed=BitSet32{0} };
	return Native<T>(m_reg, data);
}

template <Type T>
void Any<T>::LoadIfNotImmediate()
{
	RealizeLock();
	if (!IsRegBound() && !IsImm())
	{
		m_reg->BindToRegister(m_data.reg, true, false, false);
		DEBUG_LOG(REGCACHE, "LoadIfNotImmediate bind %zu to %d", m_data.reg, m_reg->m_regs[m_data.reg].location.GetSimpleReg());
	}
}

template <Type T>
void Any<T>::RealizeImmediate()
{
	RealizeLock();
	// TODO: check register pressure
	if (IsImm())
		m_reg->BindToRegister(m_data.reg, true, false, false);
}

template <Type T>
bool Any<T>::IsRegBound()
{
	RealizeLock();
	if (m_data.type == RegisterType::Borrow || m_data.type == RegisterType::Bind)
		return true;
	if (m_data.type == RegisterType::PPC)
		return Location().IsSimpleReg();
	_assert_msg_(REGCACHE, 0, "Invalid type for IsRegBound");
	return false;
}

template <Type T>
void Any<T>::Lock(RegisterData& data)
{
	switch (data.type)
	{
	case RegisterType::Bind:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.locked++;
		switch (data.mode)
		{
		case BindMode::Reuse:
			_assert_msg_(REGCACHE, IsRegBound(), "Must already be register-bound to reuse a bind");
			data.xreg = Location().GetSimpleReg();
			break;
		case BindMode::Read:
			data.xreg = m_reg->BindToRegister(data.reg, true, false, false);
			break;
		case BindMode::ReadWrite:
			data.xreg = m_reg->BindToRegister(data.reg, true, true, false);
			break;
		case BindMode::Write:
			data.xreg = m_reg->BindToRegister(data.reg, false, true, false);
			break;
		case BindMode::ReadWriteTransactional:
			data.xreg = m_reg->BindToRegister(data.reg, true, true, true);
			break;
		case BindMode::WriteTransactional:
			data.xreg = m_reg->BindToRegister(data.reg, false, true, true);
			break;
		default:
			_assert_msg_(REGCACHE, 0, "Unhandled bind %d", data.mode);
			break;
		}
		auto& xreg = m_reg->m_xregs[data.xreg];
		_assert_msg_(DYNAREC, xreg.ppcReg == data.reg, "Attempted to bind register bound elsewhere");
		xreg.locked++;
		break;
	}
	case RegisterType::Borrow:
	{
		if (data.xreg == INVALID_REG)
		{
			// This should happen when the lock is realized to give us the best chance at choosing unused regs
			data.xreg = m_reg->GetFreeXReg(data.disallowed);
			DEBUG_LOG(REGCACHE, "Borrowing register %d (auto)", data.xreg);
		}
		else
		{
			auto& xreg = m_reg->m_xregs[data.xreg];
			_assert_msg_(DYNAREC, xreg.locked == 0 || xreg.IsBorrowed(), "Attempting to steal a register that is bound");

			// If this x64 register is bound to PPC one, we need to store back
			if (xreg.IsBound())
			{
				m_reg->StoreFromRegister(xreg.ppcReg, FlushMode::FlushAll);
				_assert_msg_(DYNAREC, xreg.ppcReg == INVALID_REG, "Didn't properly flush the register");
			}

			DEBUG_LOG(REGCACHE, "Borrowing register %d", data.xreg);
		}
		auto& xreg = m_reg->m_xregs[data.xreg];
		_assert_msg_(DYNAREC, !xreg.IsBound(), "Attempted to borrow already-bound register");
		_assert_msg_(DYNAREC, !xreg.dirty, "Register was left dirty");
		xreg.locked++;
		break;
	}
	case RegisterType::Immediate:
	{
		// nothing
		break;
	}
	case RegisterType::PPC:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.locked++;
		DEBUG_LOG(REGCACHE, "++ Lock %zu (count = %d)", data.reg, ppc.locked);
		break;
	}
	}
}

template <Type T>
void Any<T>::LockAdvisory(RegisterData& data)
{
	switch (data.type)
	{
	case RegisterType::Bind:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.bindAdvise++;

		if (data.mode == BindMode::Write || data.mode == BindMode::WriteTransactional)
		{
			ppc.bindWriteWithoutReadAdvise++;
		}
	}
	case RegisterType::PPC:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.lockAdvise++;
	}
	default:
		break;
	}
}

template <Type T>
void Any<T>::SetFrom(Any<T> other)
{
	// When we implement single tracking for FPU register, this will be valid
	_assert_msg_(REGCACHE, T == Type::GPR, "SetFrom only implemented for GPRs");

	// TODO: If we know that the bound register we are copying is no longer
	// used, we could rename instead.

	auto bind = Bind(BindMode::Write);
	if (other.IsRegBound())
	{
		auto reg = other.Bind(BindMode::Reuse);
		m_reg->CopyRegister(bind, reg);
	}
	else
	{
		m_reg->LoadRegister(bind, other);
	}
}

template <Type T>
void Any<T>::Unlock()
{
	// If you trip this, you locked or bound a register that was never
	// actually used
	//_assert_msg_(REGCACHE, m_realized, "Locked register was never realized");
	if (m_realized)
		Unlock(m_data);

	UnlockAdvisory(m_data);
}

template <Type T>
void Any<T>::Unlock(RegisterData& data)
{
	switch (data.type)
	{
	case RegisterType::Bind:
	{
		// We leave the X64 register as-is until we need the room, but unlock the
		// PPC register
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.locked--;
		auto& xreg = this->m_reg->m_xregs[data.xreg];
		xreg.locked--;
		_assert_msg_(REGCACHE, xreg.ppcReg == data.reg, "Register is no longer associated with PPC register");
		_assert_msg_(REGCACHE, xreg.locked >= 0, "Register was excessively unlocked");
		break;
	}
	case RegisterType::Borrow:
	{
		auto& xreg = this->m_reg->m_xregs[data.xreg];
		xreg.locked--;

		_assert_msg_(REGCACHE, xreg.locked >= 0, "Register was excessively unlocked");
		_assert_msg_(REGCACHE, xreg.ppcReg == INVALID_REG, "Register wasn't borrowed");

		if (xreg.locked == 0)
			m_reg->ReleaseXReg(data.xreg);
		break;
	}
	case RegisterType::Immediate:
	{
		// nothing
		break;
	}
	case RegisterType::PPC: 
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.locked--;
		DEBUG_LOG(REGCACHE, "-- Unlock %zu (count = %d)", data.reg, ppc.locked);
		_assert_msg_(REGCACHE, ppc.locked >= 0, "Register was excessively unlocked");
		break;
	}
	}
}

template <Type T>
void Any<T>::UnlockAdvisory(RegisterData& data)
{
	switch (data.type)
	{
	case RegisterType::Bind:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.bindAdvise--;

		if (data.mode == BindMode::Write || data.mode == BindMode::WriteTransactional)
		{
			ppc.bindWriteWithoutReadAdvise--;
		}
	}
	case RegisterType::PPC:
	{
		auto& ppc = m_reg->m_regs[data.reg];
		ppc.lockAdvise--;
	}
	default:
		break;
	}
}

template <Type T>
void Any<T>::SetImm32(u32 imm)
{
	RealizeLock();

	DEBUG_LOG(REGCACHE, "SetImm32 %zu = %08x", m_data.reg, imm);
	_assert_msg_(REGCACHE, m_data.type == RegisterType::PPC, "SetImm32 is only valid on guest registers");

	OpArg loc = *this;
	if (loc.IsSimpleReg())
	{
		// Ditch the register, freeing it up
		X64Reg reg = loc.GetSimpleReg();
		DEBUG_LOG(REGCACHE, "Abandoning %zu native reg %d", m_data.reg, reg);
		m_reg->ReleaseXReg(reg);
	}

	auto& ppc = m_reg->m_regs[m_data.reg];
	ppc.away = true;
	ppc.location = Gen::Imm32(imm);
}

template <Type T>
Any<T>::operator OpArg()
{
	RealizeLock();

	if (m_data.type == RegisterType::PPC)
	{
		auto& ppc = m_reg->m_regs[m_data.reg];
		// If you trip this, it means you did something like:
		//   auto xd = rd.Bind(...);
		//   MOV(32, rd, ...);
		// or you've got two registers that alias each other and only one is bound
		_assert_msg_(REGCACHE, ppc.bindAdvise == 0, "Register is bound but caller attempted to use unbound register");
		return ppc.location;
	}

	if (m_data.type == RegisterType::Immediate)
	{
		return Gen::Imm32(m_data.val);
	}

	if (m_data.type == RegisterType::Bind || m_data.type == RegisterType::Borrow)
	{
		return R(m_data.xreg);
	}

	_assert_msg_(REGCACHE, 0, "Unhandled type %d", m_data.type);
	return OpArg();
}

template <Type T>
void Any<T>::RealizeLock()
{
	if (!m_realized)
	{
		m_realized = true;
		Lock(m_data);
	}
}

template <Type T>
OpArg Any<T>::Sync()
{
	RealizeLock();
	m_reg->StoreFromRegister(this->m_data.reg, FlushMode::FlushMaintainState);
	// Required by ExtractToReg in Jit_Integer (need to get rid of that)
	return m_reg->GetDefaultLocation(this->m_data.reg);
}

template <Type T>
void Any<T>::Flush()
{
	RealizeLock();
	m_reg->StoreFromRegister(this->m_data.reg, FlushMode::FlushAll);
}

template <Type T>
void Any<T>::Unbind()
{
	RealizeLock();
	m_reg->StoreFromRegister(this->m_data.reg, FlushMode::FlushDiscard);
}

template <Type T>
Native<T>::operator X64Reg()
{
	this->RealizeLock();
	return this->m_data.xreg;
}


Any<Type::GPR> RegisterClass<Type::GPR>::Imm32(u32 value)
{
	RegisterData data = { .type = RegisterType::Immediate, .val = value };
	return Any<Type::GPR>(this, data);
}


template<Type T>
Native<T> RegisterClassBase<T>::Borrow(X64Reg which, bool allowScratch)
{
	BitSet32 disallowed(0);
	if (!allowScratch)
		for (X64Reg xr : GetScratchAllocationOrder())
			disallowed[xr] = true;

	RegisterData data = { .type = RegisterType::Borrow, .xreg = which, .disallowed = disallowed };
	return Native<T>(this, data);
}

template<Type T>
Native<T> RegisterClassBase<T>::BorrowAnyBut(BitSet32 which)
{
	RegisterData data = { .type = RegisterType::Borrow, .xreg = INVALID_REG, .disallowed = which };
	return Native<T>(this, data);
}

template<Type T>
void RegisterClassBase<T>::Init()
{
	for (auto& xreg : m_xregs)
	{
		ReleaseXReg((X64Reg)(&xreg - &m_xregs[0]));
	}
	for (auto& reg : m_regs)
	{
		reg.location = GetDefaultLocation(&reg - &m_regs[0]);
		reg.away = false;
		reg.locked = 0;
		reg.transaction = false;
	}
}

template<Type T>
Any<T> RegisterClassBase<T>::Lock(reg_t preg)
{
	RegisterData data = { .type = RegisterType::PPC, .reg = preg };
	return Any<T>(this, data);
}


template<Type T>
int RegisterClassBase<T>::NumFreeRegisters()
{
	const std::vector<X64Reg>& order = GetAllocationOrder();
	return std::count_if(order.begin(), order.end(), [&](auto reg) { return m_xregs[reg].locked == 0; });
}

template<Type T>
void RegisterClassBase<T>::BindBatch(BitSet32 regs)
{
	DEBUG_LOG(REGCACHE, "Bind batch %08x", regs.m_val);
	for (int reg : regs)
	{
		if (NumFreeRegisters() < 2)
			break;

		// Don't bind immediates
		auto& ppc = m_regs[reg];
		if (ppc.away && ppc.location.IsImm())
			continue;

		BindToRegister(reg, true, false, false);
	}
}

template<Type T>
void RegisterClassBase<T>::FlushBatch(BitSet32 regs)
{
	DEBUG_LOG(REGCACHE, "Flush batch %08x", regs.m_val);
	for (int i : regs)
	{
		auto& reg = m_regs[i];
		if (reg.away)
		{
			_assert_msg_(REGCACHE, reg.location.IsSimpleReg() || reg.location.IsImm(), "Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC); 
			StoreFromRegister(i, FlushMode::FlushAll);
		}
	}
}

template<Type T>
BitSet32 RegisterClassBase<T>::InUse() const
{
	BitSet32 inuse(0);

	for (auto& xreg : m_xregs)
	{
		if (!xreg.IsFree())
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

template<Type T>
void RegisterClassBase<T>::ReleaseXReg(X64Reg xr)
{
	auto& xreg = m_xregs[xr];
	xreg.locked = 0;
	xreg.dirty = false;
	xreg.ppcReg = INVALID_REG;
}

template<Type T>
int RegisterClassBase<T>::SanityCheck() const
{
	for (auto& xreg : m_xregs)
	{
		if (xreg.locked > 0)
		{
			return 1;
		}
	}

	for (auto& reg : m_regs)
	{
		if (reg.locked > 0)
		{
			return 2;
		}
	}

	return 0;
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
		_assert_msg_(REGCACHE, !xreg.IsBorrowed(), "Someone forgot to unlock borrowed X64 reg %u", i);
	}

	for (auto& reg : m_regs)
	{
		int i = &reg - &m_regs[0];
		// We flush locked registers here because this is considered explicit
		if (reg.away && !reg.transaction)
		{
			_assert_msg_(REGCACHE, reg.location.IsSimpleReg() || reg.location.IsImm(), "Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC);
			StoreFromRegister(i, FlushMode::FlushAll);
		}
	}
}

template<Type T>
void RegisterClassBase<T>::Sync()
{
	// Flushing will automatically release bound registers, but not borrowed ones
	for (auto& xreg : m_xregs)
	{
		int i = &xreg - &m_xregs[0];
		_assert_msg_(REGCACHE, !xreg.IsBorrowed(), "Someone forgot to unlock borrowed X64 reg %u", i);
	}

	for (auto& reg : m_regs)
	{
		int i = &reg - &m_regs[0];
		// We flush locked registers here because this is considered explicit
		if (reg.away && !reg.transaction)
		{
			_assert_msg_(REGCACHE, reg.location.IsSimpleReg() || reg.location.IsImm(), "Jit64 - Flush unhandled case, reg %u PC: %08x", i, PC);
			StoreFromRegister(i, FlushMode::FlushMaintainState);
		}
	}
}

template<Type T>
X64Reg RegisterClassBase<T>::GetFreeXReg(BitSet32 disallowed)
{
	// Try scratch registers first
	for (X64Reg xr : GetScratchAllocationOrder())
		if (m_xregs[xr].IsFree() && !disallowed[xr])
			return xr;

	// Are there any X64 registers of this type that are currently unlocked 
	// (ie: not borrowed) and free (ie: not bound)?
	for (X64Reg xr : GetAllocationOrder())
		if (m_xregs[xr].IsFree() && !disallowed[xr])
			return xr;

	// Okay, not found; run the register allocator heuristic and figure out which register we should
	// clobber.
	float min_score = std::numeric_limits<float>::max();
	X64Reg best_xreg = INVALID_REG;
	size_t best_preg = 0;
	for (X64Reg xr : GetAllocationOrder())
	{
		// Explicitly disallowed
		if (disallowed[xr])
			continue;

		// Borrowed X64 regs aren't available
		if (m_xregs[xr].IsBorrowed())
			continue;

		// If this is attached to a bound PPC register, it isn't available
		size_t preg = m_xregs[xr].ppcReg;
		if (m_regs[preg].locked > 0)
			continue;

		float score = ScoreRegister(xr);
		if (score < min_score)
		{
			min_score = score;
			best_xreg = xr;
			best_preg = preg;
		}
	}

	if (best_xreg != INVALID_REG)
	{
		StoreFromRegister(best_preg, FlushMode::FlushAll);
		return best_xreg;
	}

	//Still no dice? Die!
	_assert_msg_(REGCACHE, 0, "Regcache ran out of regs");
	return INVALID_REG;
}

template<Type T>
X64Reg RegisterClassBase<T>::BindToRegister(reg_t preg, bool doLoad, bool makeDirty, bool tx)
{
	auto& ppc = m_regs[preg];

	_assert_msg_(REGCACHE, ppc.away || !ppc.location.IsImm(), "Bad immediate");

	if (tx)
	{
		ppc.rollbackLocation = ppc.location;
		ppc.transaction = true;

		// Get our new register
		BitSet32 disallowed(0);
		for (X64Reg xr : GetScratchAllocationOrder())
			disallowed[xr] = true;
		X64Reg xr = GetFreeXReg(disallowed);
		auto& xreg = m_xregs[xr];
		xreg.ppcReg = preg;
		xreg.dirty = true;
		xreg.locked = 0;

		if (doLoad)
			LoadRegister(xr, ppc.rollbackLocation);

		// If this register was previously in a simple register, let's keep it
		// locked until the transaction is committed or rolled back
		if (ppc.rollbackLocation.IsSimpleReg())
		{
			X64Reg xrOld = ppc.rollbackLocation.GetSimpleReg();
			auto& xregOld = m_xregs[xrOld];

			// Transfer the lock count, but keep the old register locked
			xreg.locked = xregOld.locked;
			xregOld.locked = 1;
		}

		ppc.away = true;
		ppc.location = R(xr);

		return xr;
	}

	// If the register location is the default, or the register is an
	// immediate, let's grab a new register.
	if (!ppc.away || (ppc.away && ppc.location.IsImm()))
	{
		BitSet32 disallowed(0);
		for (X64Reg xr : GetScratchAllocationOrder())
			disallowed[xr] = true;
		X64Reg xr = GetFreeXReg(disallowed);
		auto& xreg = m_xregs[xr];

		// Sanity check
		_assert_msg_(REGCACHE, !xreg.dirty, "Xreg already dirty");
		_assert_msg_(REGCACHE, xreg.locked == 0, "GetFreeXReg returned borrowed register");
		for (PPCCachedReg& reg : m_regs)
			if (reg.location.IsSimpleReg(xr))
				Crash();

		xreg.ppcReg = preg;
		xreg.dirty = makeDirty || ppc.location.IsImm();
		// Caller must lock this register
		xreg.locked = 0;
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

	X64Reg xr = ppc.location.GetSimpleReg();
	auto& xreg = m_xregs[xr];
	_assert_msg_(REGCACHE, xreg.ppcReg == preg, "This reg should have been bound");
	_assert_msg_(REGCACHE, ppc.location.IsImm() || ppc.location.IsSimpleReg(), "We didn't end up with a simple register or immediate");
	return xr;
}

template<Type T>
void RegisterClassBase<T>::StoreFromRegister(size_t preg, FlushMode mode)
{
	DEBUG_LOG(REGCACHE, "Flushing %zu (mode = %d)", preg, mode);
	auto& ppc = m_regs[preg];

	if (mode == FlushMode::FlushDiscard)
	{
		_assert_msg_(REGCACHE, ppc.away, "Discarding a register that wasn't actually away");
		if (ppc.location.IsSimpleReg())
		{
			X64Reg xr = ppc.location.GetSimpleReg();
			ReleaseXReg(xr);
		}

		ppc.location = GetDefaultLocation(preg);
		ppc.away = false;

		return;
	}

	_assert_msg_(REGCACHE, !ppc.transaction, "Register %zu has an open transaction during store", preg);

	if (ppc.away)
	{
		bool doStore;
		if (ppc.location.IsSimpleReg())
		{
			X64Reg xr = ppc.location.GetSimpleReg();
			auto& xreg = m_xregs[xr];
			doStore = xreg.dirty;
			if (mode == FlushMode::FlushAll)
				ReleaseXReg(xr);
		}
		else
		{
			//must be immediate - do nothing
			doStore = true;
		}
		OpArg newLoc = GetDefaultLocation(preg);
		if (doStore)
			StoreRegister(newLoc, ppc.location);
		if (mode == FlushMode::FlushAll)
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

template<Type T>
void RegisterClassBase<T>::Commit()
{
	for (auto& reg : m_regs)
	{
		if (reg.transaction)
		{
			// We commit by throwing away the rollback location
			reg.transaction = false;
			if (reg.rollbackLocation.IsSimpleReg())
			{
				X64Reg xr = reg.rollbackLocation.GetSimpleReg();
				ReleaseXReg(xr);
			}
		}
	}
}

template<Type T>
void RegisterClassBase<T>::Rollback()
{
	for (auto& reg : m_regs)
	{
		if (reg.transaction)
		{
			// Rolling back requires us to restore the old location, lock count and away flag
			reg.transaction = false;
			X64Reg xr = reg.location.GetSimpleReg();
			auto& xreg = m_xregs[xr];
			if (reg.rollbackLocation.IsSimpleReg())
			{
				X64Reg xrOld = reg.rollbackLocation.GetSimpleReg();
				auto& xregOld = m_xregs[xrOld];

				// Transfer the lock count
				xregOld.locked = xreg.locked;
			}

			// We don't care about this register any more
			ReleaseXReg(xr);

			reg.location = reg.rollbackLocation;
			reg.away = reg.rollbackLocation.IsSimpleReg() || reg.rollbackLocation.IsImm();
		}
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
	return 0;//return gpr.SanityCheck() | (fpu.SanityCheck() << 16);
}

template class Any<Type::GPR>;
template class Any<Type::FPU>;

template class Native<Type::GPR>;
template class Native<Type::FPU>;

template class RegisterClassBase<Type::GPR>;
template class RegisterClassBase<Type::FPU>;
template class RegisterClass<Type::GPR>;
template class RegisterClass<Type::FPU>;
};
