// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>
#include <cstddef>

#include "Common/x64Emitter.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/Jit64/RegCache/CachedReg.h"

class Jit64;

using preg_t = size_t;

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

  virtual Gen::OpArg GetDefaultLocation(preg_t preg) const = 0;

  void Start();

  void DiscardRegContentsIfCached(preg_t preg);
  void SetEmitter(Gen::XEmitter* emitter);

  void Flush(FlushMode mode = FlushMode::All, BitSet32 regsToFlush = BitSet32::AllTrue(32));

  void FlushLockX(Gen::X64Reg reg);
  void FlushLockX(Gen::X64Reg reg1, Gen::X64Reg reg2);

  bool SanityCheck() const;
  void KillImmediate(preg_t preg, bool doLoad, bool makeDirty);

  // TODO - instead of doload, use "read", "write"
  // read only will not set dirty flag
  void BindToRegister(preg_t preg, bool doLoad = true, bool makeDirty = true);
  void StoreFromRegister(preg_t preg, FlushMode mode = FlushMode::All);

  const Gen::OpArg& R(preg_t preg) const;
  Gen::X64Reg RX(preg_t preg) const;

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
    if (m_xregs[x].IsLocked())
      PanicAlert("RegCache: x %i already locked!", x);
    m_xregs[x].Lock();
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
    if (!m_xregs[x].IsLocked())
      PanicAlert("RegCache: x %i already unlocked!", x);
    m_xregs[x].Unlock();
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
  virtual void StoreRegister(preg_t preg, const Gen::OpArg& new_loc) = 0;
  virtual void LoadRegister(preg_t preg, Gen::X64Reg new_loc) = 0;

  virtual const Gen::X64Reg* GetAllocationOrder(size_t* count) const = 0;

  virtual BitSet32 GetRegUtilization() const = 0;
  virtual BitSet32 CountRegsIn(preg_t preg, u32 lookahead) const = 0;

  void FlushX(Gen::X64Reg reg);

  float ScoreRegister(Gen::X64Reg xreg) const;

  Jit64& m_jit;
  std::array<PPCCachedReg, 32> m_regs;
  std::array<X64CachedReg, NUM_XREGS> m_xregs;
  Gen::XEmitter* m_emitter = nullptr;
};
