// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/RegCache/JitRegCache.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <variant>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/MsgHandler.h"
#include "Common/VariantUtil.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/RegCache/CachedReg.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"
#include "Core/PowerPC/PowerPC.h"

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

bool RCOpArg::IsImm() const
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    return rc->R(*preg).IsImm();
  }
  else if (std::holds_alternative<u32>(contents))
  {
    return true;
  }
  return false;
}

s32 RCOpArg::SImm32() const
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    return rc->R(*preg).SImm32();
  }
  else if (const u32* imm = std::get_if<u32>(&contents))
  {
    return static_cast<s32>(*imm);
  }
  ASSERT(false);
  return 0;
}

u32 RCOpArg::Imm32() const
{
  if (const preg_t* preg = std::get_if<preg_t>(&contents))
  {
    return rc->R(*preg).Imm32();
  }
  else if (const u32* imm = std::get_if<u32>(&contents))
  {
    return *imm;
  }
  ASSERT(false);
  return 0;
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

RCForkGuard::RCForkGuard(RegCache& rc_) : rc(&rc_), m_regs(rc_.m_regs), m_xregs(rc_.m_xregs)
{
  ASSERT(!rc->IsAnyConstraintActive());
}

RCForkGuard::RCForkGuard(RCForkGuard&& other) noexcept
    : rc(other.rc), m_regs(std::move(other.m_regs)), m_xregs(std::move(other.m_xregs))
{
  other.rc = nullptr;
}

void RCForkGuard::EndFork()
{
  if (!rc)
    return;

  ASSERT(!rc->IsAnyConstraintActive());
  rc->m_regs = m_regs;
  rc->m_xregs = m_xregs;
  rc = nullptr;
}

RegCache::RegCache(Jit64& jit) : m_jit{jit}
{
}

void RegCache::Start()
{
  m_xregs.fill({});
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    m_regs[i] = PPCCachedReg{GetDefaultLocation(i)};
  }
}

void RegCache::SetEmitter(XEmitter* emitter)
{
  m_emitter = emitter;
}

bool RegCache::SanityCheck() const
{
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    switch (m_regs[i].GetLocationType())
    {
    case PPCCachedReg::LocationType::Default:
    case PPCCachedReg::LocationType::Discarded:
    case PPCCachedReg::LocationType::SpeculativeImmediate:
    case PPCCachedReg::LocationType::Immediate:
      break;
    case PPCCachedReg::LocationType::Bound:
    {
      if (m_regs[i].IsLocked() || m_regs[i].IsRevertable())
        return false;

      Gen::X64Reg xr = m_regs[i].Location()->GetSimpleReg();
      if (m_xregs[xr].IsLocked())
        return false;
      if (m_xregs[xr].Contents() != i)
        return false;
      break;
    }
    }
  }
  return true;
}

RCOpArg RegCache::Use(preg_t preg, RCMode mode)
{
  m_constraints[preg].AddUse(mode);
  return RCOpArg{this, preg};
}

RCOpArg RegCache::UseNoImm(preg_t preg, RCMode mode)
{
  m_constraints[preg].AddUseNoImm(mode);
  return RCOpArg{this, preg};
}

RCOpArg RegCache::BindOrImm(preg_t preg, RCMode mode)
{
  m_constraints[preg].AddBindOrImm(mode);
  return RCOpArg{this, preg};
}

RCX64Reg RegCache::Bind(preg_t preg, RCMode mode)
{
  m_constraints[preg].AddBind(mode);
  return RCX64Reg{this, preg};
}

RCX64Reg RegCache::RevertableBind(preg_t preg, RCMode mode)
{
  m_constraints[preg].AddRevertableBind(mode);
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

void RegCache::Discard(BitSet32 pregs)
{
  ASSERT_MSG(DYNA_REC, std::ranges::none_of(m_xregs, [](const auto& x) { return x.IsLocked(); }),
             "Someone forgot to unlock a X64 reg");

  for (preg_t i : pregs)
  {
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsLocked(), "Someone forgot to unlock PPC reg {} (X64 reg {}).",
               i, Common::ToUnderlying(RX(i)));
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsRevertable(), "Register transaction is in progress for {}!",
               i);

    if (m_regs[i].IsBound())
    {
      X64Reg xr = RX(i);
      m_xregs[xr].Unbind();
    }

    m_regs[i].SetDiscarded();
  }
}

void RegCache::Flush(BitSet32 pregs)
{
  ASSERT_MSG(DYNA_REC, std::ranges::none_of(m_xregs, [](const auto& x) { return x.IsLocked(); }),
             "Someone forgot to unlock a X64 reg");

  for (preg_t i : pregs)
  {
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsLocked(), "Someone forgot to unlock PPC reg {} (X64 reg {}).",
               i, Common::ToUnderlying(RX(i)));
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsRevertable(), "Register transaction is in progress for {}!",
               i);

    switch (m_regs[i].GetLocationType())
    {
    case PPCCachedReg::LocationType::Default:
      break;
    case PPCCachedReg::LocationType::Discarded:
      ASSERT_MSG(DYNA_REC, false, "Attempted to flush discarded PPC reg {}", i);
      break;
    case PPCCachedReg::LocationType::SpeculativeImmediate:
      // We can have a cached value without a host register through speculative constants.
      // It must be cleared when flushing, otherwise it may be out of sync with PPCSTATE,
      // if PPCSTATE is modified externally (e.g. fallback to interpreter).
      m_regs[i].SetFlushed();
      break;
    case PPCCachedReg::LocationType::Bound:
    case PPCCachedReg::LocationType::Immediate:
      StoreFromRegister(i);
      break;
    }
  }
}

void RegCache::Reset(BitSet32 pregs)
{
  for (preg_t i : pregs)
  {
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsAway(),
               "Attempted to reset a loaded register (did you mean to flush it?)");
    m_regs[i].SetFlushed();
  }
}

void RegCache::Revert()
{
  ASSERT(IsAllUnlocked());
  for (auto& reg : m_regs)
  {
    if (reg.IsRevertable())
      reg.SetRevert();
  }
}

void RegCache::Commit()
{
  ASSERT(IsAllUnlocked());
  for (auto& reg : m_regs)
  {
    if (reg.IsRevertable())
      reg.SetCommit();
  }
}

bool RegCache::IsAllUnlocked() const
{
  return std::ranges::none_of(m_regs, [](const auto& r) { return r.IsLocked(); }) &&
         std::ranges::none_of(m_xregs, [](const auto& x) { return x.IsLocked(); }) &&
         !IsAnyConstraintActive();
}

void RegCache::PreloadRegisters(BitSet32 to_preload)
{
  for (preg_t preg : to_preload)
  {
    if (NumFreeRegisters() < 2)
      return;
    if (!R(preg).IsImm())
      BindToRegister(preg, true, false);
  }
}

BitSet32 RegCache::RegistersInUse() const
{
  BitSet32 result;
  for (size_t i = 0; i < m_xregs.size(); i++)
  {
    if (!m_xregs[i].IsFree())
      result[i] = true;
  }
  return result;
}

void RegCache::FlushX(X64Reg reg)
{
  ASSERT_MSG(DYNA_REC, reg < m_xregs.size(), "Flushing non-existent reg {}",
             Common::ToUnderlying(reg));
  ASSERT(!m_xregs[reg].IsLocked());
  if (!m_xregs[reg].IsFree())
  {
    StoreFromRegister(m_xregs[reg].Contents());
  }
}

void RegCache::DiscardRegContentsIfCached(preg_t preg)
{
  if (m_regs[preg].IsBound())
  {
    X64Reg xr = m_regs[preg].Location()->GetSimpleReg();
    m_xregs[xr].Unbind();
    m_regs[preg].SetFlushed();
  }
}

void RegCache::BindToRegister(preg_t i, bool doLoad, bool makeDirty)
{
  if (!m_regs[i].IsBound())
  {
    X64Reg xr = GetFreeXReg();

    ASSERT_MSG(DYNA_REC, !m_xregs[xr].IsDirty(), "Xreg {} already dirty", Common::ToUnderlying(xr));
    ASSERT_MSG(DYNA_REC, !m_xregs[xr].IsLocked(), "GetFreeXReg returned locked register");
    ASSERT_MSG(DYNA_REC, !m_regs[i].IsRevertable(), "Invalid transaction state");

    m_xregs[xr].SetBoundTo(i, makeDirty || m_regs[i].IsAway());

    if (doLoad)
    {
      ASSERT_MSG(DYNA_REC, !m_regs[i].IsDiscarded(), "Attempted to load a discarded value");
      LoadRegister(i, xr);
    }

    ASSERT_MSG(DYNA_REC,
               std::ranges::none_of(m_regs,
                                    [xr](const auto& r) {
                                      return r.Location().has_value() &&
                                             r.Location()->IsSimpleReg(xr);
                                    }),
               "Xreg {} already bound", Common::ToUnderlying(xr));

    m_regs[i].SetBoundTo(xr);
  }
  else
  {
    // reg location must be simplereg; memory locations
    // and immediates are taken care of above.
    if (makeDirty)
      m_xregs[RX(i)].MakeDirty();
  }

  ASSERT_MSG(DYNA_REC, !m_xregs[RX(i)].IsLocked(),
             "WTF, this reg ({} -> {}) should have been flushed", i, Common::ToUnderlying(RX(i)));
}

void RegCache::StoreFromRegister(preg_t i, FlushMode mode)
{
  // When a transaction is in progress, allowing the store would overwrite the old value.
  ASSERT_MSG(DYNA_REC, !m_regs[i].IsRevertable(), "Register transaction on {} is in progress!", i);

  bool doStore = false;

  switch (m_regs[i].GetLocationType())
  {
  case PPCCachedReg::LocationType::Default:
  case PPCCachedReg::LocationType::Discarded:
  case PPCCachedReg::LocationType::SpeculativeImmediate:
    return;
  case PPCCachedReg::LocationType::Bound:
  {
    X64Reg xr = RX(i);
    doStore = m_xregs[xr].IsDirty();
    if (mode == FlushMode::Full)
      m_xregs[xr].Unbind();
    break;
  }
  case PPCCachedReg::LocationType::Immediate:
    doStore = true;
    break;
  }

  if (doStore)
    StoreRegister(i, GetDefaultLocation(i));
  if (mode == FlushMode::Full)
    m_regs[i].SetFlushed();
}

X64Reg RegCache::GetFreeXReg()
{
  const auto order = GetAllocationOrder();
  for (const X64Reg xr : order)
  {
    if (m_xregs[xr].IsFree())
      return xr;
  }

  // Okay, not found; run the register allocator heuristic and
  // figure out which register we should clobber.
  float min_score = std::numeric_limits<float>::max();
  X64Reg best_xreg = INVALID_REG;
  size_t best_preg = 0;
  for (const X64Reg xreg : order)
  {
    const preg_t preg = m_xregs[xreg].Contents();
    if (m_xregs[xreg].IsLocked() || m_regs[preg].IsLocked())
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

int RegCache::NumFreeRegisters() const
{
  int count = 0;
  for (const X64Reg reg : GetAllocationOrder())
  {
    if (m_xregs[reg].IsFree())
      count++;
  }
  return count;
}

// Estimate roughly how bad it would be to de-allocate this register. Higher score
// means more bad.
float RegCache::ScoreRegister(X64Reg xreg) const
{
  preg_t preg = m_xregs[xreg].Contents();
  float score = 0;

  // If it's not dirty, we don't need a store to write it back to the register file, so
  // bias a bit against dirty registers. Testing shows that a bias of 2 seems roughly
  // right: 3 causes too many extra clobbers, while 1 saves very few clobbers relative
  // to the number of extra stores it causes.
  if (m_xregs[xreg].IsDirty())
    score += 2;

  // If the register isn't actually needed in a physical register for a later instruction,
  // writing it back to the register file isn't quite as bad.
  if (GetRegUtilization()[preg])
  {
    // Don't look too far ahead; we don't want to have quadratic compilation times for
    // enormous block sizes!
    // This actually improves register allocation a tiny bit; I'm not sure why.
    u32 lookahead = std::min(m_jit.js.instructionsLeft, 64);
    // Count how many other registers are going to be used before we need this one again.
    u32 regs_in_count = CountRegsIn(preg, lookahead).Count();
    // Totally ad-hoc heuristic to bias based on how many other registers we'll need
    // before this one gets used again.
    score += 1 + 2 * (5 - log2f(1 + (float)regs_in_count));
  }

  return score;
}

const OpArg& RegCache::R(preg_t preg) const
{
  ASSERT_MSG(DYNA_REC, !m_regs[preg].IsDiscarded(), "Discarded register - {}", preg);
  return m_regs[preg].Location().value();
}

X64Reg RegCache::RX(preg_t preg) const
{
  ASSERT_MSG(DYNA_REC, m_regs[preg].IsBound(), "Unbound register - {}", preg);
  return m_regs[preg].Location()->GetSimpleReg();
}

void RegCache::Lock(preg_t preg)
{
  m_regs[preg].Lock();
}

void RegCache::Unlock(preg_t preg)
{
  m_regs[preg].Unlock();
  if (!m_regs[preg].IsLocked())
  {
    // Fully unlocked, reset realization state.
    m_constraints[preg] = {};
  }
}

void RegCache::LockX(X64Reg xr)
{
  m_xregs[xr].Lock();
}

void RegCache::UnlockX(X64Reg xr)
{
  m_xregs[xr].Unlock();
}

bool RegCache::IsRealized(preg_t preg) const
{
  return m_constraints[preg].IsRealized();
}

void RegCache::Realize(preg_t preg)
{
  if (m_constraints[preg].IsRealized())
    return;

  const bool load = m_constraints[preg].ShouldLoad();
  const bool dirty = m_constraints[preg].ShouldDirty();
  const bool kill_imm = m_constraints[preg].ShouldKillImmediate();
  const bool kill_mem = m_constraints[preg].ShouldKillMemory();

  const auto do_bind = [&] {
    BindToRegister(preg, load, dirty);
    m_constraints[preg].Realized(RCConstraint::RealizedLoc::Bound);
  };

  if (m_constraints[preg].ShouldBeRevertable())
  {
    StoreFromRegister(preg, FlushMode::MaintainState);
    do_bind();
    m_regs[preg].SetRevertable();
    return;
  }

  switch (m_regs[preg].GetLocationType())
  {
  case PPCCachedReg::LocationType::Default:
    if (kill_mem)
    {
      do_bind();
      return;
    }
    m_constraints[preg].Realized(RCConstraint::RealizedLoc::Mem);
    return;
  case PPCCachedReg::LocationType::Discarded:
  case PPCCachedReg::LocationType::Bound:
    do_bind();
    return;
  case PPCCachedReg::LocationType::Immediate:
  case PPCCachedReg::LocationType::SpeculativeImmediate:
    if (dirty || kill_imm)
    {
      do_bind();
      return;
    }
    m_constraints[preg].Realized(RCConstraint::RealizedLoc::Imm);
    break;
  }
}

bool RegCache::IsAnyConstraintActive() const
{
  return std::ranges::any_of(m_constraints, [](const auto& c) { return c.IsActive(); });
}
