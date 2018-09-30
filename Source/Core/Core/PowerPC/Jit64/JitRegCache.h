// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>

#include "Common/Assert.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/PPCAnalyst.h"

class Jit64;

class PPCCachedReg
{
public:
  enum class LocationType
  {
    /// Value is currently at its default location
    Default,
    /// Value is currently bound to a x64 register
    Bound,
    /// Value is known as an immediate and has not been written back to its default location
    Immediate,
    /// Value is known as an immediate and is already present at its default location
    SpeculativeImmediate,
  };

  PPCCachedReg() = default;

  explicit PPCCachedReg(Gen::OpArg default_location_)
      : default_location(default_location_), location(default_location_)
  {
  }

  const Gen::OpArg& Location() const { return location; }

  LocationType GetLocationType() const
  {
    if (!away)
    {
      if (location.IsImm())
        return LocationType::SpeculativeImmediate;

      ASSERT(location == default_location);
      return LocationType::Default;
    }

    ASSERT(location.IsImm() || location.IsSimpleReg());
    return location.IsImm() ? LocationType::Immediate : LocationType::Bound;
  }

  bool IsAway() const { return away; }
  bool IsBound() const { return GetLocationType() == LocationType::Bound; }

  void BoundTo(Gen::X64Reg xreg)
  {
    away = true;
    location = Gen::R(xreg);
  }

  void Flushed()
  {
    away = false;
    location = default_location;
  }

  void SetToImm32(u32 imm32, bool dirty = true)
  {
    away |= dirty;
    location = Gen::Imm32(imm32);
  }

  bool IsLocked() const { return locked; }
  void Lock() { locked = true; }
  void Unlock() { locked = false; }

private:
  Gen::OpArg default_location{};
  Gen::OpArg location{};
  bool away = false;  // value not in source register
  bool locked = false;
};

struct X64CachedReg
{
  size_t ppcReg;
  bool dirty;
  bool free;
  bool locked;
};

class RegCache
{
public:
  enum class FlushMode
  {
    All,
    MaintainState,
  };

  static constexpr size_t NUM_XREGS = 16;

  explicit RegCache(Jit64& jit);
  virtual ~RegCache() = default;

  virtual void StoreRegister(size_t preg, const Gen::OpArg& new_loc) = 0;
  virtual void LoadRegister(size_t preg, Gen::X64Reg new_loc) = 0;
  virtual Gen::OpArg GetDefaultLocation(size_t reg) const = 0;

  void Start();

  void DiscardRegContentsIfCached(size_t preg);
  void SetEmitter(Gen::XEmitter* emitter);

  void Flush(FlushMode mode = FlushMode::All, BitSet32 regsToFlush = BitSet32::AllTrue(32));

  void FlushR(Gen::X64Reg reg);
  void FlushR(Gen::X64Reg reg, Gen::X64Reg reg2);

  void FlushLockX(Gen::X64Reg reg);
  void FlushLockX(Gen::X64Reg reg1, Gen::X64Reg reg2);

  int SanityCheck() const;
  void KillImmediate(size_t preg, bool doLoad, bool makeDirty);

  // TODO - instead of doload, use "read", "write"
  // read only will not set dirty flag
  void BindToRegister(size_t preg, bool doLoad = true, bool makeDirty = true);
  void StoreFromRegister(size_t preg, FlushMode mode = FlushMode::All);

  const Gen::OpArg& R(size_t preg) const;
  Gen::X64Reg RX(size_t preg) const;

  // Register locking.

  // these are powerpc reg indices
  template <typename T>
  void Lock(T p)
  {
    m_regs[p].Lock();
  }
  template <typename T, typename... Args>
  void Lock(T first, Args... args)
  {
    Lock(first);
    Lock(args...);
  }

  // these are x64 reg indices
  template <typename T>
  void LockX(T x)
  {
    if (m_xregs[x].locked)
      PanicAlert("RegCache: x %i already locked!", x);
    m_xregs[x].locked = true;
  }
  template <typename T, typename... Args>
  void LockX(T first, Args... args)
  {
    LockX(first);
    LockX(args...);
  }

  template <typename T>
  void UnlockX(T x)
  {
    if (!m_xregs[x].locked)
      PanicAlert("RegCache: x %i already unlocked!", x);
    m_xregs[x].locked = false;
  }
  template <typename T, typename... Args>
  void UnlockX(T first, Args... args)
  {
    UnlockX(first);
    UnlockX(args...);
  }

  void UnlockAll();
  void UnlockAllX();

  bool IsFreeX(size_t xreg) const;

  Gen::X64Reg GetFreeXReg();
  int NumFreeRegisters() const;

protected:
  virtual const Gen::X64Reg* GetAllocationOrder(size_t* count) const = 0;

  virtual BitSet32 GetRegUtilization() const = 0;
  virtual BitSet32 CountRegsIn(size_t preg, u32 lookahead) const = 0;

  float ScoreRegister(Gen::X64Reg xreg) const;

  Jit64& m_jit;
  std::array<PPCCachedReg, 32> m_regs;
  std::array<X64CachedReg, NUM_XREGS> m_xregs;
  Gen::XEmitter* m_emitter = nullptr;
};
