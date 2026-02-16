// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <variant>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/VariantUtil.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"

using namespace Gen;
using namespace PowerPC;

RCOpArg RCOpArg::Imm32(u32 imm)
{
  return RCOpArg{imm};
}

RCOpArg RCOpArg::R(X64Reg xr)
{
  return RCOpArg{xr};
}

RCOpArg::RCOpArg() = default;

RCOpArg::RCOpArg(u32 imm) : rc(nullptr), contents(imm)
{
}

RCOpArg::RCOpArg(X64Reg xr) : rc(nullptr), contents(xr)
{
}

RCOpArg::RCOpArg(RegCache* rc_, preg_t preg) : rc(rc_), contents(preg)
{
  rc->Lock(preg);
}

RCOpArg::~RCOpArg()
{
  Unlock();
}

RCOpArg::RCOpArg(RCOpArg&& other) noexcept
    : rc(std::exchange(other.rc, nullptr)),
      contents(std::exchange(other.contents, std::monostate{}))
{
}

RCOpArg& RCOpArg::operator=(RCOpArg&& other) noexcept
{
  Unlock();
  rc = std::exchange(other.rc, nullptr);
  contents = std::exchange(other.contents, std::monostate{});
  return *this;
}

RCOpArg::RCOpArg(RCX64Reg&& other) noexcept
    : rc(std::exchange(other.rc, nullptr)),
      contents(VariantCast(std::exchange(other.contents, std::monostate{})))
{
}

RCOpArg& RCOpArg::operator=(RCX64Reg&& other) noexcept
{
  Unlock();
  rc = std::exchange(other.rc, nullptr);
  contents = VariantCast(std::exchange(other.contents, std::monostate{}));
  return *this;
}

void RCOpArg::Realize()
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    rc->Realize(*preg);
  }
}

OpArg RCOpArg::Location() const
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    ASSERT(rc->IsRealized(*preg));
    return rc->R(*preg);
  }
  else if (const X64Reg* xr = std::get_if<X64Reg>(&contents))
  {
    return Gen::R(*xr);
  }
  else if (const u32* imm = std::get_if<u32>(&contents))
  {
    return Gen::Imm32(*imm);
  }
  ASSERT(false);
  return {};
}

void RCOpArg::Unlock()
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    ASSERT(rc);
    rc->Unlock(*preg);
  }
  else if (const X64Reg* xr = std::get_if<X64Reg>(&contents))
  {
    // If rc, we got this from an RCX64Reg.
    // If !rc, we got this from RCOpArg::R.
    if (rc)
      rc->UnlockX(*xr);
  }
  else
  {
    ASSERT(!rc);
  }

  rc = nullptr;
  contents = std::monostate{};
}

RCX64Reg::RCX64Reg() = default;

RCX64Reg::RCX64Reg(RegCache* rc_, preg_t preg) : rc(rc_), contents(preg)
{
  rc->Lock(preg);
}

RCX64Reg::RCX64Reg(RegCache* rc_, X64Reg xr) : rc(rc_), contents(xr)
{
  rc->LockX(xr);
}

RCX64Reg::~RCX64Reg()
{
  Unlock();
}

RCX64Reg::RCX64Reg(RCX64Reg&& other) noexcept
    : rc(std::exchange(other.rc, nullptr)),
      contents(std::exchange(other.contents, std::monostate{}))
{
}

RCX64Reg& RCX64Reg::operator=(RCX64Reg&& other) noexcept
{
  Unlock();
  rc = std::exchange(other.rc, nullptr);
  contents = std::exchange(other.contents, std::monostate{});
  return *this;
}

void RCX64Reg::Realize()
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    rc->Realize(*preg);
  }
}

RCX64Reg::operator X64Reg() const&
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    ASSERT(rc->IsRealized(*preg));
    return rc->RX(*preg);
  }
  else if (const X64Reg* xr = std::get_if<X64Reg>(&contents))
  {
    return *xr;
  }
  ASSERT(false);
  return {};
}

RCX64Reg::operator OpArg() const&
{
  return Gen::R(RCX64Reg::operator X64Reg());
}

void RCX64Reg::Unlock()
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    ASSERT(rc);
    rc->Unlock(*preg);
  }
  else if (const X64Reg* xr = std::get_if<X64Reg>(&contents))
  {
    ASSERT(rc);
    rc->UnlockX(*xr);
  }
  else
  {
    ASSERT(!rc);
  }

  rc = nullptr;
  contents = std::monostate{};
}

RCForkGuard::RCForkGuard(RegCache& rc_) : rc(&rc_), m_state(rc_.m_state)
{
  ASSERT(!rc->IsAnyConstraintActive());
}

RCForkGuard::RCForkGuard(RCForkGuard&& other) noexcept : rc(other.rc), m_state(other.m_state)
{
  other.rc = nullptr;
}

void RCForkGuard::EndFork()
{
  if (!rc)
    return;

  ASSERT(!rc->IsAnyConstraintActive());

  rc->m_state = m_state;
  rc = nullptr;
}

RegCache::RegCache(Jit64& jit, XEmitter& emitter) : m_jit{jit}, m_emitter(emitter)
{
}

void RegCache::Start()
{
  m_state = {};
}

bool RegCache::SanityCheck() const
{
  if (m_state.m_guests_in_host_register &
      (m_state.m_guests_is_locked | m_state.m_guests_revertable))
    return false;

  for (const preg_t i : m_state.m_guests_in_host_register)
  {
    Gen::X64Reg xr = m_state.m_guests_host_register[i];
    if (m_state.m_hosts_is_locked[xr])
      return false;
    if (m_state.m_hosts_guest_register[xr] != i)
      return false;
  }
  return true;
}

RCOpArg RegCache::Use(preg_t preg, RCMode mode)
{
  m_guests_constraints[preg].AddUse(mode);
  return RCOpArg{this, preg};
}

RCOpArg RegCache::UseNoImm(preg_t preg, RCMode mode)
{
  m_guests_constraints[preg].AddUseNoImm(mode);
  return RCOpArg{this, preg};
}

RCOpArg RegCache::BindOrImm(preg_t preg, RCMode mode)
{
  m_guests_constraints[preg].AddBindOrImm(mode);
  return RCOpArg{this, preg};
}

RCX64Reg RegCache::Bind(preg_t preg, RCMode mode)
{
  m_guests_constraints[preg].AddBind(mode);
  return RCX64Reg{this, preg};
}

RCX64Reg RegCache::RevertableBind(preg_t preg, RCMode mode)
{
  m_guests_constraints[preg].AddRevertableBind(mode);
  return RCX64Reg{this, preg};
}

RCX64Reg RegCache::Scratch()
{
  return Scratch(GetFreeXReg());
}

RCX64Reg RegCache::Scratch(X64Reg xr)
{
  FlushX(xr);
  return RCX64Reg{this, xr};
}

RCForkGuard RegCache::Fork()
{
  return RCForkGuard{*this};
}

void RegCache::Discard(BitSetGuest pregs)
{
  ASSERT_MSG(DYNA_REC, !m_state.m_hosts_is_locked, "Someone forgot to unlock a X64 reg");
  const BitSetGuest locked_pregs = pregs & m_state.m_guests_is_locked;
  ASSERT_MSG(DYNA_REC, !locked_pregs, "Someone forgot to unlock the following PPC regs {:b}.",
             locked_pregs.m_val);
  const BitSetGuest revertable_pregs = pregs & m_state.m_guests_revertable;
  ASSERT_MSG(DYNA_REC, !revertable_pregs,
             "Register transaction is in progress for the following PPC regs {:b}.",
             revertable_pregs.m_val);

  for (const preg_t i : (pregs & m_state.m_guests_in_host_register))
  {
    const X64Reg xr = m_state.m_guests_host_register[i];
    m_state.m_hosts_in_guest_register[xr] = false;
  }

  m_state.m_guests_in_ppc_state &= ~pregs;
  m_state.m_guests_in_host_register &= ~pregs;
}

void RegCache::Flush(BitSetGuest pregs, FlushMode mode,
                     IgnoreDiscardedRegisters ignore_discarded_registers)
{
  const BitSetGuest revertable_pregs = pregs & m_state.m_guests_revertable;
  ASSERT_MSG(DYNA_REC, !revertable_pregs,
             "Register transaction is in progress for the following PPC regs {:b}.",
             revertable_pregs.m_val);

  for (const preg_t i : (pregs & ~m_state.m_guests_in_ppc_state))
  {
    StoreRegister(i, GetPPCStateLocation(i), ignore_discarded_registers);
  }

  if (mode == FlushMode::Full)
  {
    const BitSetGuest locked_pregs = pregs & m_state.m_guests_is_locked;
    ASSERT_MSG(DYNA_REC, !locked_pregs, "Someone forgot to unlock the following PPC regs {:b}.",
               locked_pregs.m_val);

    for (const preg_t i : (pregs & m_state.m_guests_in_host_register))
    {
      const X64Reg xr = m_state.m_guests_host_register[i];
      ASSERT_MSG(DYNA_REC, !m_state.m_hosts_is_locked[xr],
                 "Someone forgot to unlock X64 reg {} (PPC reg {}).", std::to_underlying(xr), i);
      m_state.m_hosts_in_guest_register[xr] = false;
    }

    m_state.m_guests_in_host_register &= ~pregs;
  }

  m_state.m_guests_in_ppc_state |= pregs;
}

void RegCache::Reset(BitSetGuest pregs)
{
  const BitSetGuest in_host_register_pregs = pregs & m_state.m_guests_in_host_register;
  ASSERT_MSG(DYNA_REC, !in_host_register_pregs,
             "Attempted to reset the loaded registers {:b} (did you mean to flush them?)",
             in_host_register_pregs.m_val);

  m_state.m_guests_in_ppc_state |= pregs;
}

BitSetGuest RegCache::RegistersRevertable() const
{
  ASSERT(IsAllUnlocked());
  return m_state.m_guests_revertable;
}

void RegCache::Commit()
{
  ASSERT(IsAllUnlocked());
  m_state.m_guests_revertable = {};
}

bool RegCache::IsAllUnlocked() const
{
  return !m_state.m_hosts_is_locked && !m_state.m_guests_is_locked && !IsAnyConstraintActive();
}

void RegCache::PreloadRegisters(BitSetGuest to_preload)
{
  for (const preg_t preg : to_preload & ~m_state.m_guests_in_host_register)
  {
    if (GetFreeRegisters().Count() < 2)
      return;
    if (!IsImm(preg))
      BindToRegister(preg, true, false);
  }
}

BitSetHost RegCache::HostRegistersInUse() const
{
  return m_state.m_hosts_in_guest_register | m_state.m_hosts_is_locked;
}

void RegCache::FlushX(X64Reg reg)
{
  ASSERT(!m_state.m_hosts_is_locked[reg]);
  if (m_state.m_hosts_in_guest_register[reg])
  {
    StoreFromRegister(m_state.m_hosts_guest_register[reg]);
  }
}

void RegCache::DiscardRegister(preg_t preg)
{
  if (m_state.m_guests_in_host_register[preg])
  {
    const X64Reg xr = m_state.m_guests_host_register[preg];
    m_state.m_hosts_in_guest_register[xr] = false;
  }

  m_state.m_guests_in_ppc_state[preg] = false;
  m_state.m_guests_in_host_register[preg] = false;
}

void RegCache::BindToRegister(preg_t i, bool doLoad, bool makeDirty)
{
  if (!m_state.m_guests_in_host_register[i])
  {
    const X64Reg xr = GetFreeXReg();

    ASSERT_MSG(DYNA_REC, !m_state.m_hosts_is_locked[xr], "GetFreeXReg returned locked register");
    ASSERT_MSG(DYNA_REC, !m_state.m_guests_revertable[i], "Invalid transaction state");

    m_state.m_hosts_in_guest_register[xr] = true;
    m_state.m_hosts_guest_register[xr] = i;

    if (doLoad)
      LoadRegister(i, xr);

    ASSERT_MSG(DYNA_REC,
               std::ranges::none_of(
                   m_state.m_guests_in_host_register,
                   [&](const auto& r) { return m_state.m_guests_host_register[r] == xr; }),
               "Xreg {} already bound", std::to_underlying(xr));

    m_state.m_guests_in_host_register[i] = true;
    m_state.m_guests_host_register[i] = xr;
  }
  if (makeDirty)
  {
    m_state.m_guests_in_ppc_state[i] = false;
    DiscardImm(i);
  }

  ASSERT_MSG(DYNA_REC, !m_state.m_hosts_is_locked[RX(i)],
             "WTF, this reg ({} -> {}) should have been flushed", i, std::to_underlying(RX(i)));
}

void RegCache::StoreFromRegister(preg_t i, FlushMode mode,
                                 IgnoreDiscardedRegisters ignore_discarded_registers)
{
  // When a transaction is in progress, allowing the store would overwrite the old value.
  ASSERT_MSG(DYNA_REC, !m_state.m_guests_revertable[i],
             "Register transaction on {} is in progress!", i);

  if (!m_state.m_guests_in_ppc_state[i])
    StoreRegister(i, GetPPCStateLocation(i), ignore_discarded_registers);

  if (mode == FlushMode::Full && m_state.m_guests_in_host_register[i])
  {
    m_state.m_guests_in_host_register[i] = false;
    m_state.m_hosts_in_guest_register[m_state.m_guests_host_register[i]] = false;
  }

  m_state.m_guests_in_ppc_state[i] = true;
}

X64Reg RegCache::GetFreeXReg()
{
  const BitSetHost free_registers = GetFreeRegisters();
  if (free_registers)
  {
    for (const X64Reg xr : GetAllocationOrder())
    {
      if (free_registers[xr])
        return xr;
    }
  }

  // Okay, not found; run the register allocator heuristic and
  // figure out which register we should clobber.
  float min_score = std::numeric_limits<float>::max();
  X64Reg best_xreg = INVALID_REG;
  preg_t best_preg = 0;
  for (const preg_t i : GetAllocatableRegisters() & ~m_state.m_hosts_is_locked)
  {
    X64Reg xreg = (X64Reg)i;
    const preg_t preg = m_state.m_hosts_guest_register[xreg];
    if (m_state.m_guests_is_locked[preg])
      continue;

    const float score = ScoreRegister(xreg);
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

  // Still no dice? Die!
  ASSERT_MSG(DYNA_REC, false, "Regcache ran out of regs");
  return INVALID_REG;
}

BitSetHost RegCache::GetFreeRegisters() const
{
  return (~m_state.m_hosts_in_guest_register & ~m_state.m_hosts_is_locked &
          GetAllocatableRegisters());
}

// Estimate roughly how bad it would be to de-allocate this register. Higher score
// means more bad.
float RegCache::ScoreRegister(X64Reg xreg) const
{
  const preg_t preg = m_state.m_hosts_guest_register[xreg];
  float score = 0;

  // If it's not dirty, we don't need a store to write it back to the register file, so
  // bias a bit against dirty registers. Testing shows that a bias of 2 seems roughly
  // right: 3 causes too many extra clobbers, while 1 saves very few clobbers relative
  // to the number of extra stores it causes.
  if (!m_state.m_guests_in_ppc_state[preg])
    score += 2;

  // If the register isn't actually needed in a physical register for a later instruction,
  // writing it back to the register file isn't quite as bad.
  if (GetRegUtilization()[preg])
  {
    // Don't look too far ahead; we don't want to have quadratic compilation times for
    // enormous block sizes!
    // This actually improves register allocation a tiny bit; I'm not sure why.
    const u32 lookahead = std::min(m_jit.js.instructionsLeft, 64);
    // Count how many other registers are going to be used before we need this one again.
    const u32 regs_in_count = CountRegsIn(preg, lookahead).Count();
    // Totally ad-hoc heuristic to bias based on how many other registers we'll need
    // before this one gets used again.
    score += 1 + 2 * (5 - log2f(1 + (float)regs_in_count));
  }

  return score;
}

X64Reg RegCache::RX(preg_t preg) const
{
  ASSERT_MSG(DYNA_REC, m_state.m_guests_in_host_register[preg], "Not in host register - {}", preg);
  return m_state.m_guests_host_register[preg];
}

void RegCache::Lock(preg_t preg)
{
  m_state.m_guests_is_locked[preg] = true;
}

void RegCache::Unlock(preg_t preg)
{
  m_state.m_guests_is_locked[preg] = false;
  // Reset realization state.
  m_guests_constraints[preg] = {};
}

void RegCache::LockX(X64Reg xr)
{
  m_state.m_hosts_is_locked[xr] = true;
}

void RegCache::UnlockX(X64Reg xr)
{
  m_state.m_hosts_is_locked[xr] = false;
}

bool RegCache::IsRealized(preg_t preg) const
{
  return m_guests_constraints[preg].IsRealized();
}

void RegCache::Realize(preg_t preg)
{
  if (m_guests_constraints[preg].IsRealized())
    return;

  const bool load = m_guests_constraints[preg].ShouldLoad();
  const bool dirty = m_guests_constraints[preg].ShouldDirty();
  const bool kill_imm = m_guests_constraints[preg].ShouldKillImmediate();
  const bool kill_mem = m_guests_constraints[preg].ShouldKillMemory();

  const auto do_bind = [&] {
    BindToRegister(preg, load, dirty);
    m_guests_constraints[preg].Realized(RCConstraint::RealizedLoc::Bound);
  };

  if (m_guests_constraints[preg].ShouldBeRevertable())
  {
    StoreFromRegister(preg, FlushMode::Undirty);
    do_bind();
    m_state.m_guests_revertable[preg] = true;
    return;
  }

  if (IsImm(preg))
  {
    if (dirty || kill_imm)
      do_bind();
    else
      m_guests_constraints[preg].Realized(RCConstraint::RealizedLoc::Imm);
  }
  else if (!m_state.m_guests_in_host_register[preg])
  {
    if (kill_mem)
      do_bind();
    else
      m_guests_constraints[preg].Realized(RCConstraint::RealizedLoc::Mem);
  }
  else
  {
    do_bind();
  }
}

bool RegCache::IsAnyConstraintActive() const
{
  return std::ranges::any_of(m_guests_constraints, &RCConstraint::IsActive);
}
