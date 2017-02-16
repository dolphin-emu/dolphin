// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>

#include "Common/x64Emitter.h"
#include "Core/PowerPC/PPCAnalyst.h"

class Jit64;

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
    m_regs[p].locked = true;
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
  bool IsBound(size_t preg) const;

  Gen::X64Reg GetFreeXReg();
  int NumFreeRegisters();

protected:
  virtual const Gen::X64Reg* GetAllocationOrder(size_t* count) = 0;

  virtual BitSet32 GetRegUtilization() = 0;
  virtual BitSet32 CountRegsIn(size_t preg, u32 lookahead) = 0;

  float ScoreRegister(Gen::X64Reg xreg);

  Jit64& m_jit;
  std::array<PPCCachedReg, 32> m_regs;
  std::array<X64CachedReg, NUM_XREGS> m_xregs;
  Gen::XEmitter* m_emitter = nullptr;
};
