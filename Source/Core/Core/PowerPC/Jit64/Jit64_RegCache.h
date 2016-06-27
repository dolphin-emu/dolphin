// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/x64Emitter.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/PPCAnalyst.h"

// The register cache manages a set of virtual registers on both the PowerPC
// and X64 side. The important concepts for the register cache are:
//
//
// Locking: a PowerPC register must be locked before you can interact with it.
// This ensures that the register does not move at any time while you may be
// potentially generating code that references it. A locked register *may*
// move under explicit instruction from a Flush(), FlushBatch(), Bind() or
// other call, but will not move indirectly as a result of requesting a
// scratch register or binding a different register.
//
// The lock is held as long as the C++ object representing the lock is in
// scope. If a lock result (the "Any" class) is assigned to another non-
// reference variable, the lifetime of that lock is at least as long as both
// of those variables.
//
// An immediate value may be assigned to a PowerPC register which binds the
// register in a special state named "away". The register can continue to be
// used as-is, but its location will resolve as a Gen::Imm32().
//
//
// Binding: for X64 instructions that require a register, a PowerPC register
// may be bound to one as needed. As with a lock, the register will not be
// moved as long as both the original lock and the bind lock (the "Native"
// class) are in scope. When a binding goes out of scope the register
// continues to hold the PowerPC register's value, but it may be flushed to
// make way for a future binding.
//
// Immediate: a virtual register with a 32-bit value (signed or unsigned) can
// be allocated by the register cache and used (read-only) exactly the same
// way as a standard register.
//
//
// Borrowing: an X64 register may be borrowed for use as a scratch register,
// or to communicate with a method that requires inputs to be in certain
// registers. If a specific register is requested to be borrowed, that
// register will be freed unless it allocated to a register that is bound and
// locked (or a previous borrow) in which case an assertion is raised.
//
// Realization: A lock, binding or borrow is not realized until the first
// operation that queries it state. This allows the register cache to shuffle
// registers around until the register's location needs to be known which
// gives it some extra flexibility in cases where register use makes things
// tight. The register cache will throw an assertion if a lock is never used
// before it goes out of scope.

namespace Jit64Reg
{
constexpr int NUMXREGS = 16;

using namespace Gen;

using reg_t = size_t;

enum class BindMode
{
  // Loads a register with the contents of this register, does not mark it dirty
  Read,
  // Allocates a register without loading the guest register, marks it dirty
  Write,
  // Loads the guest register into a native register, marks it dirty
  ReadWrite,
  // Begins a transaction on this register that will either be rolled back or committed,
  // depending on the method called on Registers. This will implicitly bind the guest
  // register to a native register for the entire scope of the transaction. Attempting to
  // flush, sync or rebind this register before rollback/commit is an error.
  ReadWriteTransactional,
  WriteTransactional,
  // Asserts that the register is already loaded -- doesn't load or mark it as dirty
  Reuse,
};

enum class Type
{
  // PPC GPR
  GPR,
  // PPC FPU
  FPU,
};

enum class RegisterType
{
  Bind,
  Borrow,
  Immediate,
  PPC,
};

enum class FlushMode
{
  FlushAll,
  FlushMaintainState,
  FlushDiscard,
};

extern const std::vector<X64Reg> GPR_ALLOCATION_ORDER;
extern const std::vector<X64Reg> GPR_SCRATCH_ALLOCATION_ORDER;
extern const std::vector<X64Reg> FPU_ALLOCATION_ORDER;
extern const std::vector<X64Reg> FPU_SCRATCH_ALLOCATION_ORDER;
extern const BitSet32 DISALLOW_SCRATCH_GPR;
extern const BitSet32 DISALLOW_SCRATCH_FPU;

// Forward
template <Type T>
class RegisterClass;
template <Type T>
class RegisterClassBase;
class Registers;
class PPC;
class Tx;
class Imm;
template <Type T>
class Native;
template <Type T>
class Any;
class CarryLock;

struct RegisterData
{
  RegisterType type;
  X64Reg xreg;
  reg_t reg;
  u32 val;
  BindMode mode;
  BitSet32 disallowed;
};

// Every register type (borrowed, bound, PowerPC and immediate) is represented
// by an Any<T> (where T is GPR or FPU). This class, however, delegates to the
// real register implementation underneath and is mainly used for lock
// management purposes.
template <Type T>
class Any
{
  friend class Registers;
  friend class Native<T>;
  friend class RegisterClass<T>;
  friend class RegisterClassBase<T>;

private:
  Any(RegisterClassBase<T>* reg, RegisterData data) : m_reg(reg), m_data(data), m_realized(false)
  {
    LockAdvisory(m_data);
  };

  // A lock is only realized when a caller attempts to cast it to an OpArg or bind it
  void RealizeLock();

  Gen::OpArg Location() const
  {
    if (m_data.type == RegisterType::PPC || m_data.type == RegisterType::Bind)
      return m_reg->m_regs[m_data.reg].location;

    _assert_msg_(REGCACHE, 0, "Invalid operation: not a PPC register");
    return OpArg();
  }

  void LockAdvisory(const RegisterData& data);
  void UnlockAdvisory(const RegisterData& data);
  void Lock(RegisterData& data);
  void Unlock();
  void Unlock(const RegisterData& data);

public:
  virtual ~Any() { Unlock(); }
  Any(Any<T>&& other) : m_reg(other.m_reg), m_data(other.m_data)
  {
    LockAdvisory(m_data);

    // Transfer the lock (but don't actually realize it again)
    if (other.m_realized)
    {
      m_realized = true;
      other.m_realized = false;
    }
    else
    {
      m_realized = false;
    }
  }

  Any(const Any<T>& other) : m_reg(other.m_reg), m_data(other.m_data), m_realized(false)
  {
    LockAdvisory(m_data);

    // Realize this lock as well
    if (other.m_realized)
    {
      RealizeLock();
    }
  }

  Any& operator=(const Any<T>& other)
  {
    // We need to be careful in the case of self-assignment
    RegisterData old = m_data;
    m_data = other.m_data;

    // Lock "other" first
    LockAdvisory(m_data);
    if (other.m_realized)
      Lock(m_data);

    // Then unlock our data
    if (m_realized)
      Unlock(old);
    UnlockAdvisory(old);

    // Update our realization state to match other
    m_realized = other.m_realized;

    return *this;
  }

  // TODO: This shouldn't be necessary
  void ForceRealize() { RealizeLock(); }
  bool IsImm() const
  {
    // Does not realize the lock
    if (m_data.type == RegisterType::Immediate)
      return true;
    if (m_data.type == RegisterType::PPC)
      return Location().IsImm();
    return false;
  }

  bool IsZero() const
  {
    // Does not realize the lock
    if (m_data.type == RegisterType::Immediate)
      return m_data.val == 0;
    if (m_data.type == RegisterType::PPC)
      return Location().IsZero();
    return false;
  }

  u32 Imm32() const
  {
    // Does not realize the lock
    if (m_data.type == RegisterType::Immediate)
      return m_data.val;
    if (m_data.type == RegisterType::PPC)
    {
      if (Location().IsImm())
      {
        return Location().Imm32();
      }
    }

    _assert_msg_(REGCACHE, 0, "Invalid operation: not an immediate");
    return 0;
  }

  s32 SImm32() const
  {
    // Does not realize the lock
    if (m_data.type == RegisterType::Immediate)
      return static_cast<s32>(m_data.val);
    if (m_data.type == RegisterType::PPC)
    {
      if (Location().IsImm())
      {
        return Location().SImm32();
      }
    }
    _assert_msg_(REGCACHE, 0, "Invalid operation: not an immediate");
    return 0;
  }

  // Returns the guest register represented by this class.
  reg_t PPCRegister() const
  {
    if (m_data.type == RegisterType::PPC || m_data.type == RegisterType::Bind)
      return m_data.reg;
    _assert_msg_(REGCACHE, 0, "Invalid operation: not a PPC register");
    return 0;
  }

  // Returns true if this register represents a guest register
  bool HasPPCRegister() const
  {
    return (m_data.type == RegisterType::PPC || m_data.type == RegisterType::Bind);
  }

  void SetImm32(u32 imm);

  // If this value is an immediate, places it into a register (if there is
  // room) or back into its long-term memory location. If placed in a
  // register, the register is marked dirty.
  void RealizeImmediate();

  // If this value is not an immediate, loads it into a register.
  void LoadIfNotImmediate();

  // Sync the current value to the default location but otherwise doesn't move the
  // register.
  OpArg Sync();

  // TODO: Remove this
  void Flush();

  // Discards any cached information about this register and returns it to
  // the default location.
  void Unbind();

  bool IsAliasOf(const Any<T>& other) const
  {
    if (HasPPCRegister() && other.HasPPCRegister() && PPCRegister() == other.PPCRegister())
      return true;
    return false;
  }

  bool IsAliasOf(const Any<T>& other1, const Any<T>& other2) const
  {
    return IsAliasOf(other1) || IsAliasOf(other2);
  }

  // True if this register is available in a native register
  bool IsRegBound();

  // Forces this register into a native register. While the binding is in scope, it is an
  // error to access this register's information.
  Native<T> Bind(BindMode mode);

  // Shortcut for common bind pattern
  Native<T> BindWriteAndReadIf(bool cond)
  {
    return Bind(cond ? BindMode::ReadWrite : BindMode::Write);
  }

  // Borrows a copy of a this register in a scratch register. The copy is
  // always in a register, no matter whether the current register is an
  // immediate, has been spilled back to memory, or lives in a register.
  // To avoid weird realization issues, both this register and the copy
  // are immediately realized.
  //
  // This is only valid for GPR-type registers.
  Native<T> BorrowCopy();

  // Sets the value of this register equal to the value of the other
  // register. If both registers live in memory, implicitly binds this
  // register as if Bind() had been called. If this register was already
  // bound, it is marked as dirty.
  //
  // If this register is set to itself, an assertion is triggered. While we
  // could handle this, it requires every call site to correctly identify
  // whether an already-bound register should be dirtied in the case where
  // it is set to itself.
  //
  // This is only valid for GPR-type registers.
  void SetFrom(Any<T> other);

  // Any register, bound or not, can be used as an OpArg. If not bound, this will return
  // a pointer into PPCSTATE.
  operator Gen::OpArg();

private:
  RegisterClassBase<T>* m_reg;
  RegisterData m_data;
  bool m_realized;
};

// Native is a lightweight class used to manage scope like Any<T>, but adds a
// direct cast to X64Reg for borrows and binds.
template <Type T>
class Native final : public Any<T>
{
  friend class Registers;
  friend class Any<T>;
  friend class RegisterClass<T>;
  friend class RegisterClassBase<T>;

private:
  Native(RegisterClassBase<T>* reg, RegisterData data) : Any<T>(reg, data) {}
public:
  // Delegate these to Any<T>
  virtual ~Native() {}
  Native(const Native& other) = default;
  Native(Native&& other) = default;
  Native& operator=(const Native& other)
  {
    _assert_msg_(REGCACHE, 0, "TODO operator=");
    Any<T>::operator=(other);
    return *this;
  }

  operator Gen::X64Reg();
};

template <Type T>
class RegisterClassBase
{
  friend class Any<T>;
  friend class Native<T>;

  struct PPCCachedReg
  {
    Gen::OpArg location;

    // This register's value not in source register, ie: location !=
    // GetDefaultLocation().
    bool away;

    // Advises the register cache that this register will be locked
    // shortly. It still can be spilled back to memory but that may be
    // expensive. This is a reference count.
    int lockAdvise;

    // Advises the register cache that a bind will be active shortly so we
    // can warn callers that they should not use the unbound register.
    // This is a reference count.
    int bindAdvise;

    // Advises the register cache that a a register is bound writable
    // without being read.
    int bindWriteWithoutReadAdvise;

    // A locked register is in use by the currently-generating opcode and cannot
    // be spilled back to memory. This is a reference count.
    int locked;

    // The value of this register is transactional and awaiting Commit()
    // or Rollback()
    bool transaction;
    Gen::OpArg rollbackLocation;
  };

  struct X64CachedReg
  {
    // If locked, this points back to the bound register or INVALID_REG if
    // just borrowed.
    reg_t ppcReg;

    // If bound (ie: ppcReg != INVALID_REG), indicates that this
    // register's contents are dirty and require write-back.
    bool dirty;

    // A native register is locked if it is either explictly bound to a
    // PPC register or borrowed. This is a reference count.
    int locked;

    bool IsFree() const { return locked == 0 && ppcReg == INVALID_REG; }
    bool IsBound() const { return ppcReg != INVALID_REG; }
    bool IsBorrowed() const { return locked > 0 && ppcReg == INVALID_REG; }
  };

public:
  // Borrows a host register. If 'which' is omitted, an appropriate one is chosen.
  // Note that the register that is actually borrowed is indeterminate up until
  // the actual point where a method is called that would require it to be
  // determined. It is an error to borrow a register and never access it.
  Native<T> Borrow(Gen::X64Reg which = Gen::INVALID_REG, bool allowScratch = true);

  // Special form of borrow that requests any register except those in 'which'
  Native<T> BorrowAnyBut(BitSet32 which);

  // Locks a target register for the duration of this scope. This register will
  // no longer be implicitly moved until the end of the scope. Note that the lock
  // is advisory until a method that requires current state of the register to be
  // accessed is called and a register may be moved up until that point.
  // It is an error to lock a register and never access it.
  Any<T> Lock(reg_t register);

  // Advises the register cache that we need these registers. If we have
  // room, we'll try to bind them ahead-of-time.
  void BindBatch(const BitSet32& regs);

  // Flushes the specified registers as they are no longer needed.
  void FlushBatch(const BitSet32& regs);

  BitSet32 InUse() const;

protected:
  RegisterClassBase() = default;

  void Init(Gen::XEmitter* emit, JitBase* jit)
  {
    m_emit = emit;
    m_jit = jit;
  }

  virtual const std::vector<X64Reg>& GetAllocationOrder() const = 0;
  virtual const std::vector<X64Reg>& GetScratchAllocationOrder() const = 0;
  virtual const BitSet32& DisallowScratch() const = 0;
  virtual void LoadRegister(X64Reg newLoc, const OpArg& location) = 0;
  virtual void StoreRegister(const OpArg& newLoc, const OpArg& location) = 0;
  virtual void CopyRegister(X64Reg newLoc, X64Reg location) = 0;
  virtual OpArg GetDefaultLocation(reg_t reg) const = 0;
  virtual BitSet32 GetRegUtilization() const = 0;
  virtual BitSet32 GetRegsIn(int i) const = 0;

  X64Reg BindToRegister(reg_t preg, bool doLoad, bool makeDirty, bool tx);
  void CheckUnlocked() const;
  BitSet32 CountRegsIn(reg_t preg, u32 lookahead) const;
  template <typename FunctionObject>
  void ForEachPPC(FunctionObject fn)
  {
    reg_t i = 0;
    for (auto& reg : m_regs)
      fn(i++, reg);
  }
  template <typename FunctionObject>
  void ForEachPPC(FunctionObject fn) const
  {
    reg_t i = 0;
    for (auto& reg : m_regs)
      fn(i++, reg);
  }
  template <typename FunctionObject>
  void ForEachX64(FunctionObject fn)
  {
    int xr = 0;
    for (auto& xreg : m_xregs)
      fn((X64Reg)xr++, xreg);
  }
  template <typename FunctionObject>
  void ForEachX64(FunctionObject fn) const
  {
    int xr = 0;
    for (auto& xreg : m_xregs)
      fn((X64Reg)xr++, xreg);
  }
  X64Reg GetFreeXReg(const std::vector<X64Reg>& candidates, BitSet32 disallowed) const;
  X64Reg GetFreeXReg(BitSet32 disallowed);
  void Init();
  size_t NumFreeRegisters() const;
  void ReleaseXReg(X64Reg xr);
  int SanityCheck() const;
  float ScoreRegister(X64Reg xr) const;
  void StoreFromRegister(reg_t preg, FlushMode mode);
  void UnlockAllBorrowedNative()
  {
    // Free all borrowed native registers
    for (auto& xreg : m_xregs)
      if (xreg.ppcReg == INVALID_REG)
        xreg.locked = 0;
  }
  void UnlockAllPPC()
  {
    // Unlock all PPC registers
    for (auto& reg : m_regs)
      reg.locked = 0;
  }
  void CopyFrom(const RegisterClassBase<T>& other)
  {
    m_regs = other.m_regs;
    m_xregs = other.m_xregs;

    UnlockAllPPC();
    UnlockAllBorrowedNative();
  }

public:
  // TODO: not public
  void Flush();

  // TODO: not public
  void Sync();

  void Commit();
  void Rollback();

  Gen::XEmitter* m_emit;
  JitBase* m_jit;
  std::array<PPCCachedReg, 32 + 32> m_regs;
  std::array<X64CachedReg, NUMXREGS> m_xregs;
};

template <Type T>
class RegisterClass
{
};

template <>
class RegisterClass<Type::FPU> : public RegisterClassBase<Type::FPU>
{
  friend class Registers;

protected:
  const std::vector<X64Reg>& GetAllocationOrder() const override { return FPU_ALLOCATION_ORDER; }
  const std::vector<X64Reg>& GetScratchAllocationOrder() const override
  {
    return FPU_SCRATCH_ALLOCATION_ORDER;
  }
  const BitSet32& DisallowScratch() const override { return DISALLOW_SCRATCH_FPU; };
  void LoadRegister(X64Reg newLoc, const OpArg& location) override
  {
    m_emit->MOVAPD(newLoc, location);
  }
  void StoreRegister(const OpArg& newLoc, const OpArg& location) override
  {
    m_emit->MOVAPD(newLoc, location.GetSimpleReg());
  }
  void CopyRegister(X64Reg newLoc, X64Reg location) override
  {
    _assert_msg_(REGCACHE, 0, "Unimplemented");
  }
  OpArg GetDefaultLocation(reg_t reg) const override { return PPCSTATE(ps[reg][0]); }
  BitSet32 GetRegUtilization() const override { return m_jit->js.op->fprInXmm; }
  BitSet32 GetRegsIn(int i) const override { return m_jit->js.op[i].fregsIn; }
};

class CarryLock
{
  template <Type T>
  friend class RegisterClass;

public:
  ~CarryLock() { m_lock--; }
private:
  CarryLock(u32& lock) : m_lock(lock) { m_lock++; }
  u32& m_lock;
};

template <>
class RegisterClass<Type::GPR> : public RegisterClassBase<Type::GPR>
{
  friend class Registers;

public:
  // A virtual register that contains zero and cannot be updated
  Any<Type::GPR> Zero() { return Imm32(0); }
  // A virtual register that contains an immediate and cannot be updated
  Any<Type::GPR> Imm32(u32 immediate);

  CarryLock LockCarry() { return CarryLock(m_carryLock); }
protected:
  const std::vector<X64Reg>& GetAllocationOrder() const override { return GPR_ALLOCATION_ORDER; }
  const std::vector<X64Reg>& GetScratchAllocationOrder() const override
  {
    return GPR_SCRATCH_ALLOCATION_ORDER;
  }
  const BitSet32& DisallowScratch() const override { return DISALLOW_SCRATCH_GPR; };
  void LoadRegister(X64Reg newLoc, const OpArg& location) override
  {
    // Is carry precious?
    if (m_carryLock && !m_emit->FlagsLocked())
    {
      m_emit->LockFlags();
      m_emit->MOV(32, R(newLoc), location);
      m_emit->UnlockFlags();
    }
    else
    {
      m_emit->MOV(32, R(newLoc), location);
    }
  }
  void StoreRegister(const OpArg& newLoc, const OpArg& location) override
  {
    m_emit->MOV(32, newLoc, location);
  }
  void CopyRegister(X64Reg newLoc, X64Reg location) override
  {
    m_emit->MOV(32, R(newLoc), R(location));
  }
  OpArg GetDefaultLocation(reg_t reg) const override { return PPCSTATE(gpr[reg]); }
  BitSet32 GetRegUtilization() const override { return m_jit->js.op->gprInReg; }
  BitSet32 GetRegsIn(int i) const override { return m_jit->js.op[i].regsIn; };
private:
  u32 m_carryLock = 0;
};

using GPRRegisters = RegisterClass<Type::GPR>;
using FPURegisters = RegisterClass<Type::FPU>;

using GPRRegister = Any<Type::GPR>;
using GPRNative = Native<Type::GPR>;
using FPURegister = Any<Type::FPU>;
using FPUNative = Native<Type::FPU>;

class Registers
{
  template <Type T>
  friend class Any;
  template <Type T>
  friend class RegisterClass;
  template <Type T>
  friend class RegisterClassBase;

public:
  void Init();

  Registers(Gen::XEmitter* emit, JitBase* jitBase) : m_emit(emit), m_jit(jitBase)
  {
    fpu.Init(m_emit, m_jit);
    gpr.Init(m_emit, m_jit);
  }

  // Create an independent copy of the register cache state for a branch
  Registers Branch();

  int SanityCheck() const;

  void Flush()
  {
    gpr.Flush();
    fpu.Flush();
  }
  void Commit()
  {
    gpr.Commit();
    fpu.Commit();
  }
  void Rollback()
  {
    gpr.Rollback();
    fpu.Rollback();
  }

  FPURegisters fpu;
  GPRRegisters gpr;

private:
  Gen::XEmitter* m_emit;
  JitBase* m_jit;
};
};
