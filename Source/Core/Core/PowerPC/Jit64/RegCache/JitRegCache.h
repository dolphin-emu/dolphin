// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <variant>

#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/RegCache/RCMode.h"

class Jit64;
enum class RCMode;

class RCOpArg;
class RCX64Reg;
class RegCache;

using preg_t = u8;
static constexpr size_t NUM_HOST_REGS = 16;
using BitSetHost = BitSet16;
static constexpr size_t NUM_GUEST_REGS = 32;
using BitSetGuest = BitSet32;

class RCOpArg
{
public:
  static RCOpArg Imm32(u32 imm);
  static RCOpArg R(Gen::X64Reg xr);
  RCOpArg();
  ~RCOpArg();
  RCOpArg(RCOpArg&&) noexcept;
  RCOpArg& operator=(RCOpArg&&) noexcept;

  RCOpArg(RCX64Reg&&) noexcept;
  RCOpArg& operator=(RCX64Reg&&) noexcept;

  RCOpArg(const RCOpArg&) = delete;
  RCOpArg& operator=(const RCOpArg&) = delete;

  void Realize();
  Gen::OpArg Location() const;
  operator Gen::OpArg() const& { return Location(); }
  operator Gen::OpArg() const&& = delete;
  bool IsSimpleReg() const { return Location().IsSimpleReg(); }
  bool IsSimpleReg(Gen::X64Reg reg) const { return Location().IsSimpleReg(reg); }
  Gen::X64Reg GetSimpleReg() const { return Location().GetSimpleReg(); }

  void Unlock();

  bool IsImm() const { return Location().IsImm(); }
  s32 SImm32() const { return Location().SImm32(); }
  u32 Imm32() const { return Location().Imm32(); }
  bool IsZero() const { return IsImm() && Imm32() == 0; }

private:
  friend class RegCache;

  explicit RCOpArg(u32 imm);
  explicit RCOpArg(Gen::X64Reg xr);
  RCOpArg(RegCache* rc_, preg_t preg);

  RegCache* rc = nullptr;
  std::variant<std::monostate, Gen::X64Reg, u32, preg_t> contents;
};

class RCX64Reg
{
public:
  RCX64Reg();
  ~RCX64Reg();
  RCX64Reg(RCX64Reg&&) noexcept;
  RCX64Reg& operator=(RCX64Reg&&) noexcept;

  RCX64Reg(const RCX64Reg&) = delete;
  RCX64Reg& operator=(const RCX64Reg&) = delete;

  void Realize();
  operator Gen::OpArg() const&;
  operator Gen::X64Reg() const&;
  operator Gen::OpArg() const&& = delete;
  operator Gen::X64Reg() const&& = delete;

  void Unlock();

private:
  friend class RegCache;
  friend class RCOpArg;

  RCX64Reg(RegCache* rc_, preg_t preg);
  RCX64Reg(RegCache* rc_, Gen::X64Reg xr);

  RegCache* rc = nullptr;
  std::variant<std::monostate, Gen::X64Reg, preg_t> contents;
};

struct RegsState
{
  // If m_hosts_in_guest_register[xr], then xr is bound to m_hosts_guest_register[xr]
  std::array<preg_t, NUM_HOST_REGS> m_hosts_guest_register = {};
  BitSetHost m_hosts_in_guest_register;
  // While it is possible for a register to be locked multiple times, all the unlocks happen at the
  // same time.
  BitSetHost m_hosts_is_locked;

  // If m_guests_in_host_register[preg], then preg is bound to m_guests_host_register[preg]
  std::array<Gen::X64Reg, NUM_GUEST_REGS> m_guests_host_register{};
  BitSetGuest m_guests_in_host_register;
  // Is the value in PPCState correct right now?
  BitSetGuest m_guests_in_ppc_state = BitSetGuest::AllTrue();
  // If certain memory load operations fail, the destination register must be reverted.
  // This is achieved by not flushing it, and using the old value from PPCState.
  BitSetGuest m_guests_revertable;
  // While it is possible for a register to be locked multiple times, all the unlocks happen at the
  // same time.
  BitSetGuest m_guests_is_locked;
};

class RCForkGuard
{
public:
  ~RCForkGuard() { EndFork(); }
  RCForkGuard(RCForkGuard&&) noexcept;

  RCForkGuard(const RCForkGuard&) = delete;
  RCForkGuard& operator=(const RCForkGuard&) = delete;
  RCForkGuard& operator=(RCForkGuard&&) = delete;

  void EndFork();

private:
  friend class RegCache;

  explicit RCForkGuard(RegCache& rc_);

  RegCache* rc;
  RegsState m_state;
};

class RCConstraints
{
public:
  bool IsRealized(preg_t preg) const { return m_realized[preg] != RealizedLoc::Invalid; }
  bool IsAnyConstraintActive() const
  {
    constexpr std::array<RealizedLoc, NUM_GUEST_REGS> all_invalid{};
    // memcmp is faster, since compilers aren't able to vectorize multiple enum comparisons.
    return std::memcmp(m_realized.data(), all_invalid.data(), NUM_GUEST_REGS) != 0 ||
           (m_write | m_read | m_kill_imm | m_kill_mem | m_revertable);
  }

  bool ShouldLoad(preg_t preg) const { return m_read[preg]; }
  bool ShouldDirty(preg_t preg) const { return m_write[preg]; }
  bool ShouldBeRevertable(preg_t preg) const { return m_revertable[preg]; }
  bool ShouldKillImmediate(preg_t preg) const { return m_kill_imm[preg]; }
  bool ShouldKillMemory(preg_t preg) const { return m_kill_mem[preg]; }

  enum class RealizedLoc : u8
  {
    Invalid,
    Bound,
    Imm,
    Mem,
  };

  void Realized(preg_t preg, RealizedLoc loc)
  {
    m_realized[preg] = loc;
    ASSERT(IsRealized(preg));
  }

  enum class ConstraintLoc : u8
  {
    Bound,
    BoundOrImm,
    BoundOrMem,
    Any,
  };

  void AddUse(preg_t preg, RCMode mode) { AddConstraint(preg, mode, ConstraintLoc::Any, false); }
  void AddUseNoImm(preg_t preg, RCMode mode)
  {
    AddConstraint(preg, mode, ConstraintLoc::BoundOrMem, false);
  }
  void AddBindOrImm(preg_t preg, RCMode mode)
  {
    AddConstraint(preg, mode, ConstraintLoc::BoundOrImm, false);
  }
  void AddBind(preg_t preg, RCMode mode) { AddConstraint(preg, mode, ConstraintLoc::Bound, false); }
  void AddRevertableBind(preg_t preg, RCMode mode)
  {
    AddConstraint(preg, mode, ConstraintLoc::Bound, true);
  }

  void Reset(preg_t preg)
  {
    m_realized[preg] = {};
    m_write[preg] = false;
    m_read[preg] = false;
    m_kill_imm[preg] = false;
    m_kill_mem[preg] = false;
    m_revertable[preg] = false;
  }

private:
  void AddConstraint(preg_t preg, RCMode mode, ConstraintLoc loc, bool should_revertable)
  {
    if (IsRealized(preg))
    {
      ASSERT(IsCompatible(preg, mode, loc, should_revertable));
      return;
    }

    if (should_revertable)
      m_revertable[preg] = true;

    switch (loc)
    {
    case ConstraintLoc::Bound:
      m_kill_imm[preg] = true;
      m_kill_mem[preg] = true;
      break;
    case ConstraintLoc::BoundOrImm:
      m_kill_mem[preg] = true;
      break;
    case ConstraintLoc::BoundOrMem:
      m_kill_imm[preg] = true;
      break;
    case ConstraintLoc::Any:
      break;
    }

    switch (mode)
    {
    case RCMode::Read:
      m_read[preg] = true;
      break;
    case RCMode::Write:
      m_write[preg] = true;
      break;
    case RCMode::ReadWrite:
      m_read[preg] = true;
      m_write[preg] = true;
      break;
    }
  }

  bool IsCompatible(preg_t preg, RCMode mode, ConstraintLoc loc, bool should_revertable) const
  {
    if (should_revertable && !m_revertable[preg])
    {
      return false;
    }

    const bool is_loc_compatible = [&] {
      switch (loc)
      {
      case ConstraintLoc::Bound:
        return m_realized[preg] == RealizedLoc::Bound;
      case ConstraintLoc::BoundOrImm:
        return m_realized[preg] == RealizedLoc::Bound || m_realized[preg] == RealizedLoc::Imm;
      case ConstraintLoc::BoundOrMem:
        return m_realized[preg] == RealizedLoc::Bound || m_realized[preg] == RealizedLoc::Mem;
      case ConstraintLoc::Any:
        return true;
      }
      ASSERT(false);
      return false;
    }();

    const bool is_mode_compatible = [&] -> bool {
      switch (mode)
      {
      case RCMode::Read:
        return m_read[preg];
      case RCMode::Write:
        return m_write[preg];
      case RCMode::ReadWrite:
        return m_read[preg] && m_write[preg];
      }
      ASSERT(false);
      return false;
    }();

    return is_loc_compatible && is_mode_compatible;
  }

  std::array<RealizedLoc, NUM_GUEST_REGS> m_realized{};
  BitSetGuest m_write;
  BitSetGuest m_read;
  BitSetGuest m_kill_imm;
  BitSetGuest m_kill_mem;
  BitSetGuest m_revertable;
};

class RegCache
{
public:
  enum class FlushMode
  {
    // All dirty registers get written back, and all registers get removed from the cache.
    Full,
    // All dirty registers get written back and get set as no longer dirty.
    // No registers are removed from the cache.
    Undirty,
  };

  enum class IgnoreDiscardedRegisters
  {
    No,
    Yes,
  };

  explicit RegCache(Jit64& jit, Gen::XEmitter& emitter);
  virtual ~RegCache() = default;

  void Start();
  bool SanityCheck() const;

  template <typename... Ts>
  static void Realize(Ts&... rc)
  {
    static_assert(((std::is_same<Ts, RCOpArg>() || std::is_same<Ts, RCX64Reg>()) && ...));
    (rc.Realize(), ...);
  }

  template <typename... Ts>
  static void Unlock(Ts&... rc)
  {
    static_assert(((std::is_same<Ts, RCOpArg>() || std::is_same<Ts, RCX64Reg>()) && ...));
    (rc.Unlock(), ...);
  }

  template <typename... Args>
  bool IsImm(Args... pregs) const
  {
    static_assert(sizeof...(pregs) > 0);
    return (IsImm(preg_t(pregs)) && ...);
  }

  virtual bool IsImm(preg_t preg) const = 0;
  virtual u32 Imm32(preg_t preg) const = 0;
  virtual s32 SImm32(preg_t preg) const = 0;

  bool IsBound(preg_t preg) const { return m_state.m_guests_in_host_register[preg]; }

  RCOpArg Use(preg_t preg, RCMode mode)
  {
    m_guests_constraints.AddUse(preg, mode);
    return RCOpArg{this, preg};
  }

  RCOpArg UseNoImm(preg_t preg, RCMode mode)
  {
    m_guests_constraints.AddUseNoImm(preg, mode);
    return RCOpArg{this, preg};
  }

  RCOpArg BindOrImm(preg_t preg, RCMode mode)
  {
    m_guests_constraints.AddBindOrImm(preg, mode);
    return RCOpArg{this, preg};
  }

  RCX64Reg Bind(preg_t preg, RCMode mode)
  {
    m_guests_constraints.AddBind(preg, mode);
    return RCX64Reg{this, preg};
  }

  RCX64Reg RevertableBind(preg_t preg, RCMode mode)
  {
    m_guests_constraints.AddRevertableBind(preg, mode);
    return RCX64Reg{this, preg};
  }
  RCX64Reg Scratch();
  RCX64Reg Scratch(Gen::X64Reg xr);

  RCForkGuard Fork();
  void Discard(BitSetGuest pregs);
  void Flush(BitSetGuest pregs = BitSetGuest::AllTrue(), FlushMode mode = FlushMode::Full,
             IgnoreDiscardedRegisters ignore_discarded_registers = IgnoreDiscardedRegisters::No);
  void Reset(BitSetGuest pregs);
  BitSetGuest RegistersRevertable() const;
  void Commit();

  bool IsAllUnlocked() const;

  void PreloadRegisters(BitSetGuest to_preload);
  BitSetHost HostRegistersInUse() const;

protected:
  friend class RCOpArg;
  friend class RCX64Reg;
  friend class RCForkGuard;

  virtual Gen::OpArg GetPPCStateLocation(preg_t preg) const = 0;
  virtual void StoreRegister(preg_t preg, const Gen::OpArg& new_loc,
                             IgnoreDiscardedRegisters ignore_discarded_registers) = 0;
  virtual void LoadRegister(preg_t preg, Gen::X64Reg new_loc) = 0;
  virtual void DiscardImm(preg_t preg) = 0;

  virtual std::span<const Gen::X64Reg> GetAllocationOrder() const = 0;
  virtual BitSetHost GetAllocatableRegisters() const = 0;
  virtual BitSetGuest GetRegUtilization() const = 0;
  virtual BitSetGuest CountRegsIn(preg_t preg, u32 lookahead) const = 0;

  void FlushX(Gen::X64Reg reg);
  void DiscardRegister(preg_t preg);
  void BindToRegister(preg_t preg, bool doLoad = true, bool makeDirty = true);
  void StoreFromRegister(
      preg_t preg, FlushMode mode = FlushMode::Full,
      IgnoreDiscardedRegisters ignore_discarded_registers = IgnoreDiscardedRegisters::No);

  Gen::X64Reg GetFreeXReg();

  BitSetHost GetFreeRegisters() const;
  float ScoreRegister(Gen::X64Reg xreg) const;

  virtual Gen::OpArg R(preg_t preg) const = 0;
  Gen::X64Reg RX(preg_t preg) const;

  void Lock(preg_t preg);
  void Unlock(preg_t preg);
  void LockX(Gen::X64Reg xr);
  void UnlockX(Gen::X64Reg xr);
  bool IsRealized(preg_t preg) const { return m_guests_constraints.IsRealized(preg); }
  bool IsAnyConstraintActive() const { return m_guests_constraints.IsAnyConstraintActive(); }
  void Realize(preg_t preg);

  Jit64& m_jit;
  Gen::XEmitter& m_emitter;

  RegsState m_state{};
  RCConstraints m_guests_constraints{};
};
