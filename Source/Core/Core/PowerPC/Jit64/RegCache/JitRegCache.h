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

class RCConstraint
{
public:
  bool IsRealized() const { return realized != RealizedLoc::Invalid; }
  bool IsActive() const
  {
    return IsRealized() || write || read || kill_imm || kill_mem || revertable;
  }

  bool ShouldLoad() const { return read; }
  bool ShouldDirty() const { return write; }
  bool ShouldBeRevertable() const { return revertable; }
  bool ShouldKillImmediate() const { return kill_imm; }
  bool ShouldKillMemory() const { return kill_mem; }

  enum class RealizedLoc
  {
    Invalid,
    Bound,
    Imm,
    Mem,
  };

  void Realized(RealizedLoc loc)
  {
    realized = loc;
    ASSERT(IsRealized());
  }

  enum class ConstraintLoc
  {
    Bound,
    BoundOrImm,
    BoundOrMem,
    Any,
  };

  void AddUse(RCMode mode) { AddConstraint(mode, ConstraintLoc::Any, false); }
  void AddUseNoImm(RCMode mode) { AddConstraint(mode, ConstraintLoc::BoundOrMem, false); }
  void AddBindOrImm(RCMode mode) { AddConstraint(mode, ConstraintLoc::BoundOrImm, false); }
  void AddBind(RCMode mode) { AddConstraint(mode, ConstraintLoc::Bound, false); }
  void AddRevertableBind(RCMode mode) { AddConstraint(mode, ConstraintLoc::Bound, true); }

private:
  void AddConstraint(RCMode mode, ConstraintLoc loc, bool should_revertable)
  {
    if (IsRealized())
    {
      ASSERT(IsCompatible(mode, loc, should_revertable));
      return;
    }

    if (should_revertable)
      revertable = true;

    switch (loc)
    {
    case ConstraintLoc::Bound:
      kill_imm = true;
      kill_mem = true;
      break;
    case ConstraintLoc::BoundOrImm:
      kill_mem = true;
      break;
    case ConstraintLoc::BoundOrMem:
      kill_imm = true;
      break;
    case ConstraintLoc::Any:
      break;
    }

    switch (mode)
    {
    case RCMode::Read:
      read = true;
      break;
    case RCMode::Write:
      write = true;
      break;
    case RCMode::ReadWrite:
      read = true;
      write = true;
      break;
    }
  }

  bool IsCompatible(RCMode mode, ConstraintLoc loc, bool should_revertable) const
  {
    if (should_revertable && !revertable)
    {
      return false;
    }

    const bool is_loc_compatible = [&] {
      switch (loc)
      {
      case ConstraintLoc::Bound:
        return realized == RealizedLoc::Bound;
      case ConstraintLoc::BoundOrImm:
        return realized == RealizedLoc::Bound || realized == RealizedLoc::Imm;
      case ConstraintLoc::BoundOrMem:
        return realized == RealizedLoc::Bound || realized == RealizedLoc::Mem;
      case ConstraintLoc::Any:
        return true;
      }
      ASSERT(false);
      return false;
    }();

    const bool is_mode_compatible = [&] {
      switch (mode)
      {
      case RCMode::Read:
        return read;
      case RCMode::Write:
        return write;
      case RCMode::ReadWrite:
        return read && write;
      }
      ASSERT(false);
      return false;
    }();

    return is_loc_compatible && is_mode_compatible;
  }

  RealizedLoc realized = RealizedLoc::Invalid;
  bool write = false;
  bool read = false;
  bool kill_imm = false;
  bool kill_mem = false;
  bool revertable = false;
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

  RCOpArg Use(preg_t preg, RCMode mode);
  RCOpArg UseNoImm(preg_t preg, RCMode mode);
  RCOpArg BindOrImm(preg_t preg, RCMode mode);
  RCX64Reg Bind(preg_t preg, RCMode mode);
  RCX64Reg RevertableBind(preg_t preg, RCMode mode);
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
  bool IsRealized(preg_t preg) const;
  void Realize(preg_t preg);

  bool IsAnyConstraintActive() const;

  Jit64& m_jit;
  Gen::XEmitter& m_emitter;

  RegsState m_state{};
  std::array<RCConstraint, NUM_GUEST_REGS> m_guests_constraints;
};
