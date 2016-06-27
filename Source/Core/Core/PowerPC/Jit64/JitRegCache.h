// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cinttypes>

#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/PPCAnalyst.h"

enum FlushMode
{
  FLUSH_ALL,
  FLUSH_MAINTAIN_STATE,
};

// FPR shape is an optimization to allow deferring conversions (and thereby
// allowing them to be omitted entirely in many cases).
//
// This optimization introduces a number of implicit float-to-double
// conversions, but no implicit double-to-float conversions and so is correct
// even if flush-to-zero is modified (as all denormal floats are normal
// doubles).
enum FPRShape
{
  // The default shape, a pair of doubles. This is the only shape which
  // can exist in PowerPC::ppcState (other shapes only exist in registers).
  // It is also the only shape which can always be converted to safely.
  SHAPE_DEFAULT,

  // A single 32-bit floating point value, loaded from memory. This may
  // contain any pattern of bits, and must be converted to double losslessly,
  // safely checking for NaN.
  SHAPE_LAZY_SINGLE,

  // A single 32-bit floating point value which is the result of a floating
  // point operation. It is "safe" because the NaN results will not contain
  // arbitrary bits, so it may be converted to double without preserving NaN
  // values.
  SHAPE_SAFE_LAZY_SINGLE,

  // Potential future shapes:
  //
  //  SHAPE_PAIRED_SINGLES
  //    A pair of 32-bit floats. Seems like a good idea - could improve paired
  //    singles code by avoiding repeated CVTPD2PS, CVTPS2PD rounding.
  //
  //  SHAPE_LAZY_DOUBLE
  //    The value has been promoted to double, but not duplicated. This
  //    might avoid MOVDDUP in places, although there's already a similar
  //    optimization implemented, so I doubt this would impact performance
  //    much.
  //
  //  SHAPE_DOUBLE_PRESERVED
  //    Value has been promoted to double, but the high part hasn't been
  //    loaded yet. Could spill by only writing the low 64-bits and never
  //    loading the full register. I suspect this wouldn't be great for
  //    performance, but it might be worth trying.
};

struct PPCCachedReg
{
  Gen::OpArg location;
  bool away;  // value not in source register
  bool locked;
  uint8_t shape;
};

struct X64CachedReg
{
  size_t ppcReg;
  bool dirty;
  bool free;
  bool locked;
};

typedef int XReg;
typedef int PReg;

#define NUMXREGS 16

class RegCache
{
protected:
  std::array<PPCCachedReg, 32> regs;
  std::array<X64CachedReg, NUMXREGS> xregs;

  virtual const Gen::X64Reg* GetAllocationOrder(size_t* count) = 0;

  virtual BitSet32 GetRegUtilization() = 0;
  virtual BitSet32 CountRegsIn(size_t preg, u32 lookahead) = 0;

  EmuCodeBlock* emit;

  float ScoreRegister(Gen::X64Reg xreg);

public:
  RegCache();
  virtual ~RegCache() {}
  void Start();

  void DiscardRegContentsIfCached(size_t preg);
  void SetEmitter(EmuCodeBlock* emitter) { emit = emitter; }
  void FlushR(Gen::X64Reg reg);
  void FlushR(Gen::X64Reg reg, Gen::X64Reg reg2)
  {
    FlushR(reg);
    FlushR(reg2);
  }

  void FlushLockX(Gen::X64Reg reg)
  {
    FlushR(reg);
    LockX(reg);
  }
  void FlushLockX(Gen::X64Reg reg1, Gen::X64Reg reg2)
  {
    FlushR(reg1);
    FlushR(reg2);
    LockX(reg1);
    LockX(reg2);
  }

  void Flush(FlushMode mode = FLUSH_ALL, BitSet32 regsToFlush = BitSet32::AllTrue(32));
  void Flush(PPCAnalyst::CodeOp* op) { Flush(); }
  int SanityCheck() const;
  void KillImmediate(size_t preg, bool doLoad, bool makeDirty);

  // TODO - instead of doload, use "read", "write"
  // read only will not set dirty flag
  void BindToRegister(size_t preg, bool doLoad = true, bool makeDirty = true,
                      int shape = SHAPE_DEFAULT);
  void StoreFromRegister(size_t preg, FlushMode mode = FLUSH_ALL);
  virtual void StoreRegister(size_t preg, const Gen::OpArg& newLoc) = 0;
  virtual void LoadRegister(size_t preg, Gen::X64Reg newLoc) = 0;
  virtual void ConvertRegister(size_t preg, int shape) = 0;

  const Gen::OpArg& R(size_t preg) { return regs[preg].location; }
  Gen::X64Reg RX(size_t preg)
  {
    if (IsBound(preg))
      return regs[preg].location.GetSimpleReg();

    PanicAlert("Unbound register - %zu", preg);
    return Gen::INVALID_REG;
  }
  virtual Gen::OpArg GetDefaultLocation(size_t reg) const = 0;

  // Register locking.

  void LockAnyShape(size_t p) { regs[p].locked = true; }
  // these are powerpc reg indices
  template <typename T>
  void Lock(T p)
  {
    regs[p].locked = true;
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
    if (xregs[x].locked)
      PanicAlert("RegCache: x %i already locked!", x);
    xregs[x].locked = true;
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
    if (!xregs[x].locked)
      PanicAlert("RegCache: x %i already unlocked!", x);
    xregs[x].locked = false;
  }
  template <typename T, typename... Args>
  void UnlockX(T first, Args... args)
  {
    UnlockX(first);
    UnlockX(args...);
  }

  void UnlockAll();
  void UnlockAllX();

  bool IsFreeX(size_t xreg) const { return xregs[xreg].free && !xregs[xreg].locked; }
  bool IsBound(size_t preg) const { return regs[preg].away && regs[preg].location.IsSimpleReg(); }
  Gen::X64Reg GetFreeXReg();
  int NumFreeRegisters();
};

class GPRRegCache final : public RegCache
{
public:
  void StoreRegister(size_t preg, const Gen::OpArg& newLoc) override;
  void LoadRegister(size_t preg, Gen::X64Reg newLoc) override;
  Gen::OpArg GetDefaultLocation(size_t reg) const override;
  const Gen::X64Reg* GetAllocationOrder(size_t* count) override;
  void SetImmediate32(size_t preg, u32 immValue);
  BitSet32 GetRegUtilization() override;
  BitSet32 CountRegsIn(size_t preg, u32 lookahead) override;
  void ConvertRegister(size_t preg, int shape) override;
};

class FPURegCache final : public RegCache
{
public:
  void StoreRegister(size_t preg, const Gen::OpArg& newLoc) override;
  void LoadRegister(size_t preg, Gen::X64Reg newLoc) override;
  const Gen::X64Reg* GetAllocationOrder(size_t* count) override;
  Gen::OpArg GetDefaultLocation(size_t reg) const override;
  BitSet32 GetRegUtilization() override;
  BitSet32 CountRegsIn(size_t preg, u32 lookahead) override;
  void ConvertRegister(size_t preg, int shape) override;

  bool IsLazySingle(size_t preg)
  {
    return regs[preg].shape == SHAPE_LAZY_SINGLE || regs[preg].shape == SHAPE_SAFE_LAZY_SINGLE;
  }

  const Gen::OpArg& R(size_t preg) { return regs[preg].location; }
  Gen::X64Reg RX(size_t preg)
  {
    if (IsBound(preg))
    {
      return regs[preg].location.GetSimpleReg();
    }

    PanicAlert("Unbound register - %zu", preg);
    return Gen::INVALID_REG;
  }

  const Gen::OpArg& R_lazy_single(size_t preg)
  {
    if (!IsLazySingle(preg))
      PanicAlert("Not a lazy single!");
    return regs[preg].location;
  }

  Gen::X64Reg RX_lazy_single(size_t preg)
  {
    if (IsBound(preg))
    {
      return R_lazy_single(preg).GetSimpleReg();
    }

    PanicAlert("Unbound register - %zu", preg);
    return Gen::INVALID_REG;
  }

  void MarkSafeLazySingle(size_t preg) { regs[preg].shape = SHAPE_SAFE_LAZY_SINGLE; }
  // these are powerpc reg indices
  template <typename T>
  void EnsureDefaultShape(T p)
  {
    if (regs[p].shape != SHAPE_DEFAULT)
      ConvertRegister(p, SHAPE_DEFAULT);
  }
  template <typename T, typename... Args>
  void EnsureDefaultShape(T first, Args... args)
  {
    EnsureDefaultShape(first);
    EnsureDefaultShape(args...);
  }
};
