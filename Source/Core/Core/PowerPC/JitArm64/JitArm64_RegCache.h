// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

class JitArm64;

// Dedicated host registers

// memory base register
constexpr Arm64Gen::ARM64Reg MEM_REG = Arm64Gen::ARM64Reg::X28;
// ppcState pointer
constexpr Arm64Gen::ARM64Reg PPC_REG = Arm64Gen::ARM64Reg::X29;
// PC register when calling the dispatcher
constexpr Arm64Gen::ARM64Reg DISPATCHER_PC = Arm64Gen::ARM64Reg::W26;

#ifdef __GNUC__
#define PPCSTATE_OFF(elem)                                                                         \
  ([]() consteval {                                                                                \
    _Pragma("GCC diagnostic push")                                                                 \
        _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"") return offsetof(                  \
            PowerPC::PowerPCState, elem);                                                          \
    _Pragma("GCC diagnostic pop")                                                                  \
  }())

#define PPCSTATE_OFF_ARRAY(elem, i)                                                                \
  (PPCSTATE_OFF(elem[0]) + sizeof(PowerPC::PowerPCState::elem[0]) * (i))
#else
#define PPCSTATE_OFF(elem) (offsetof(PowerPC::PowerPCState, elem))

#define PPCSTATE_OFF_ARRAY(elem, i)                                                                \
  (offsetof(PowerPC::PowerPCState, elem[0]) + sizeof(PowerPC::PowerPCState::elem[0]) * (i))
#endif

#define PPCSTATE_OFF_GPR(i) PPCSTATE_OFF_ARRAY(gpr, i)
#define PPCSTATE_OFF_CR(i) PPCSTATE_OFF_ARRAY(cr.fields, i)
#define PPCSTATE_OFF_SR(i) PPCSTATE_OFF_ARRAY(sr, i)
#define PPCSTATE_OFF_SPR(i) PPCSTATE_OFF_ARRAY(spr, i)

static_assert(std::is_same_v<decltype(PowerPC::PowerPCState::ps[0]), PowerPC::PairedSingle&>);
#define PPCSTATE_OFF_PS0(i) (PPCSTATE_OFF_ARRAY(ps, i) + offsetof(PowerPC::PairedSingle, ps0))
#define PPCSTATE_OFF_PS1(i) (PPCSTATE_OFF_ARRAY(ps, i) + offsetof(PowerPC::PairedSingle, ps1))

// Some asserts to make sure we will be able to load everything
static_assert(PPCSTATE_OFF_SPR(1023) <= 16380, "LDR(32bit) can't reach the last SPR");
static_assert((PPCSTATE_OFF_PS0(0) % 8) == 0, "LDR(64bit VFP) requires FPRs to be 8 byte aligned");
static_assert(PPCSTATE_OFF(xer_ca) < 4096, "STRB can't store xer_ca!");
static_assert(PPCSTATE_OFF(xer_so_ov) < 4096, "STRB can't store xer_so_ov!");

enum class RegType
{
  Register,          // PS0 and PS1, each 64-bit
  LowerPair,         // PS0 only, 64-bit
  Duplicated,        // PS0 and PS1 are identical, host register only stores one lane (64-bit)
  Single,            // PS0 and PS1, each 32-bit
  LowerPairSingle,   // PS0 only, 32-bit
  DuplicatedSingle,  // PS0 and PS1 are identical, host register only stores one lane (32-bit)
};

enum class FlushMode : bool
{
  // Flushes all registers, no exceptions
  All,
  // Flushes registers in a conditional branch
  // Doesn't wipe the state of the registers from the cache
  MaintainState,
};

enum class IgnoreDiscardedRegisters
{
  No,
  Yes,
};

class OpArg
{
public:
  OpArg() = default;

  RegType GetFPRType() const { return m_fpr_type; }
  Arm64Gen::ARM64Reg GetReg() const { return m_reg; }
  void Load(Arm64Gen::ARM64Reg reg, RegType format = RegType::Register)
  {
    m_reg = reg;
    m_fpr_type = format;
    m_in_host_register = true;
  }
  void Discard()
  {
    // Invalidate any previous information
    m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
    m_fpr_type = RegType::Register;
    m_in_ppc_state = false;
    m_in_host_register = false;

    // Arbitrarily large value that won't roll over on a lot of increments
    m_last_used = 0xFFFF;
  }
  void Flush()
  {
    // Invalidate any previous information
    m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
    m_fpr_type = RegType::Register;
    m_in_ppc_state = true;
    m_in_host_register = false;

    // Arbitrarily large value that won't roll over on a lot of increments
    m_last_used = 0xFFFF;
  }

  u32 GetLastUsed() const { return m_last_used; }
  void ResetLastUsed() { m_last_used = 0; }
  void IncrementLastUsed() { ++m_last_used; }
  void SetDirty(bool dirty) { m_in_ppc_state = !dirty; }
  bool IsInPPCState() const { return m_in_ppc_state; }
  bool IsInHostRegister() const { return m_in_host_register; }

private:
  Arm64Gen::ARM64Reg m_reg = Arm64Gen::ARM64Reg::INVALID_REG;  // host register we are in
  RegType m_fpr_type = RegType::Register;                      // for FPRs only

  u32 m_last_used = 0;

  bool m_in_ppc_state = true;
  bool m_in_host_register = false;
};

class HostReg
{
public:
  HostReg() = default;
  HostReg(Arm64Gen::ARM64Reg reg) : m_reg(reg) {}

  bool IsLocked() const { return m_locked; }
  void Lock() { m_locked = true; }
  void Unlock() { m_locked = false; }
  Arm64Gen::ARM64Reg GetReg() const { return m_reg; }

private:
  Arm64Gen::ARM64Reg m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
  bool m_locked = false;
};

class Arm64RegCache
{
public:
  explicit Arm64RegCache(size_t guest_reg_count) : m_guest_registers(guest_reg_count) {}
  virtual ~Arm64RegCache() = default;

  void Init(JitArm64* jit);

  virtual void Start(PPCAnalyst::BlockRegStats& stats) {}
  void DiscardRegisters(BitSet32 regs);
  void ResetRegisters(BitSet32 regs);
  // Flushes the register cache in different ways depending on the mode.
  // A temporary register must be supplied when flushing GPRs with FlushMode::MaintainState,
  // but in other cases it can be set to ARM64Reg::INVALID_REG when convenient for the caller.
  virtual void Flush(FlushMode mode, Arm64Gen::ARM64Reg tmp_reg,
                     IgnoreDiscardedRegisters ignore_discarded_registers) = 0;

  virtual BitSet32 GetCallerSavedUsed() const = 0;

  // Returns a temporary register for use
  // Requires unlocking after done
  Arm64Gen::ARM64Reg GetReg();

  class ScopedARM64Reg
  {
  public:
    inline ScopedARM64Reg() = default;
    ScopedARM64Reg(const ScopedARM64Reg&) = delete;
    explicit inline ScopedARM64Reg(Arm64RegCache& cache) : m_reg(cache.GetReg()), m_gpr(&cache) {}
    inline ScopedARM64Reg(Arm64Gen::ARM64Reg reg) : m_reg(reg) {}
    inline ScopedARM64Reg(ScopedARM64Reg&& scoped_reg) { *this = std::move(scoped_reg); }
    inline ~ScopedARM64Reg() { Unlock(); }

    inline ScopedARM64Reg& operator=(const ScopedARM64Reg&) = delete;
    inline ScopedARM64Reg& operator=(Arm64Gen::ARM64Reg reg)
    {
      Unlock();
      m_reg = reg;
      return *this;
    }
    inline ScopedARM64Reg& operator=(ScopedARM64Reg&& scoped_reg)
    {
      // Taking ownership of an existing scoped register, no need to release.
      m_reg = scoped_reg.m_reg;
      m_gpr = scoped_reg.m_gpr;
      scoped_reg.Invalidate();
      return *this;
    }

    inline Arm64Gen::ARM64Reg GetReg() const { return m_reg; }
    inline operator Arm64Gen::ARM64Reg() const { return GetReg(); }
    inline void Unlock()
    {
      // Only unlock the register if GPR is set.
      if (m_gpr != nullptr)
      {
        m_gpr->Unlock(m_reg);
      }
      Invalidate();
    }

  private:
    inline void Invalidate()
    {
      m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
      m_gpr = nullptr;
    }

    Arm64Gen::ARM64Reg m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
    Arm64RegCache* m_gpr = nullptr;
  };

  // Returns a temporary register
  // Unlocking is implicitly handled through RAII
  inline ScopedARM64Reg GetScopedReg() { return ScopedARM64Reg(*this); }

  void UpdateLastUsed(BitSet32 regs_used);

  // Get available host registers
  u32 GetUnlockedRegisterCount() const;

  // Locks a register so a cache cannot use it
  // Useful for function calls
  template <typename T = Arm64Gen::ARM64Reg, typename... Args>
  void Lock(Args... args)
  {
    for (T reg : {args...})
    {
      FlushByHost(reg);
      LockRegister(reg);
    }
  }

  // Unlocks a locked register
  // Unlocks registers locked with both GetReg and LockRegister
  template <typename T = Arm64Gen::ARM64Reg, typename... Args>
  void Unlock(Args... args)
  {
    for (T reg : {args...})
    {
      FlushByHost(reg);
      UnlockRegister(reg);
    }
  }

protected:
  // Get the order of the host registers
  virtual void GetAllocationOrder() = 0;

  // Flushes the most stale register
  void FlushMostStaleRegister();

  // Lock a register
  void LockRegister(Arm64Gen::ARM64Reg host_reg);

  // Unlock a register
  void UnlockRegister(Arm64Gen::ARM64Reg host_reg);

  // Flushes a guest register by host provided
  virtual void FlushByHost(Arm64Gen::ARM64Reg host_reg,
                           Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG) = 0;

  void DiscardRegister(size_t preg);
  virtual void FlushRegister(size_t preg, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) = 0;

  void IncrementAllUsed()
  {
    for (auto& reg : m_guest_registers)
      reg.IncrementLastUsed();
  }

  JitArm64* m_jit = nullptr;

  // Code emitter
  Arm64Gen::ARM64XEmitter* m_emit = nullptr;

  // Float emitter
  std::unique_ptr<Arm64Gen::ARM64FloatEmitter> m_float_emit;

  // Host side registers that hold the host registers in order of use
  std::vector<HostReg> m_host_registers;

  // Our guest GPRs
  // PowerPC has 32 GPRs and 8 CRs
  // PowerPC also has 32 paired FPRs
  std::vector<OpArg> m_guest_registers;

  // Register stats for the current block
  PPCAnalyst::BlockRegStats* m_reg_stats = nullptr;
};

class Arm64GPRCache : public Arm64RegCache
{
public:
  Arm64GPRCache();

  void Start(PPCAnalyst::BlockRegStats& stats) override;

  // Flushes the register cache in different ways depending on the mode.
  // A temporary register must be supplied when flushing GPRs with FlushMode::MaintainState,
  // but in other cases it can be set to ARM64Reg::INVALID_REG when convenient for the caller.
  void Flush(
      FlushMode mode, Arm64Gen::ARM64Reg tmp_reg,
      IgnoreDiscardedRegisters ignore_discarded_registers = IgnoreDiscardedRegisters::No) override;

  // Returns a guest GPR inside of a host register.
  // Will dump an immediate to the host register as well.
  Arm64Gen::ARM64Reg R(size_t preg) { return BindForRead(GUEST_GPR_OFFSET + preg); }

  // Returns a guest CR inside of a host register.
  Arm64Gen::ARM64Reg CR(size_t preg) { return BindForRead(GUEST_CR_OFFSET + preg); }

  // Set a register to an immediate. Only valid for guest GPRs.
  void SetImmediate(size_t preg, u32 imm, bool dirty = true)
  {
    SetImmediateInternal(GUEST_GPR_OFFSET + preg, imm, dirty);
  }

  // Returns whether a register is set as an immediate. Only valid for guest GPRs.
  bool IsImm(size_t preg) const;

  // Gets the immediate that a register is set to. Only valid for guest GPRs.
  u32 GetImm(size_t preg) const;

  bool IsImm(size_t preg, u32 imm) const { return IsImm(preg) && GetImm(preg) == imm; }

  // Binds a guest GPR to a host register, optionally loading its value.
  //
  // preg: The guest register index.
  // will_read: Whether the caller intends to read from the register.
  // will_write: Whether the caller intends to write to the register.
  //
  // Normally, you should call this function if you intend to write to a register, and shouldn't
  // call this function if you don't intend to write to a register. There is however one situation
  // where calling this function with will_write = false is a useful trick: When emulating a memory
  // load that might have to be rolled back.
  //
  // By calling this function with will_write = false before performing the load, this function
  // guarantees that the guest register will be marked as dirty (needing to be written back to
  // ppcState) only if the guest register previously contained a value that needs to be written back
  // to ppcState. This trick prevents the following problem that otherwise would happen:
  //
  // 1. The caller calls this function with will_read = false and will_write = true.
  // 2. The guest register didn't have a host register allocated, so this function allocates one.
  // 3. This function does *not* write anything to the host register, since will_read was false.
  // 4. The caller emits code for the load.
  // 5. The caller calls Flush (to emit code for jumping to an exception handler).
  // 6. Flush writes the value in the host register to ppcState, even though it was a stale value.
  //
  // By calling this function with will_write = false before the Flush call, no stale values will be
  // flushed. Just remember to call this function again with will_write = true after the Flush call.
  void BindToRegister(size_t preg, bool will_read, bool will_write = true)
  {
    BindForWrite(GUEST_GPR_OFFSET + preg, will_read, will_write);
  }

  // Binds a guest CR to a host register, optionally loading its value.
  // The description of BindToRegister above applies to this function as well.
  void BindCRToRegister(size_t preg, bool will_read, bool will_write = true)
  {
    BindForWrite(GUEST_CR_OFFSET + preg, will_read, will_write);
  }

  BitSet32 GetCallerSavedUsed() const override;

  BitSet32 GetDirtyGPRs() const;

  void StoreRegisters(BitSet32 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG,
                      FlushMode flush_mode = FlushMode::All)
  {
    FlushRegisters(regs, flush_mode, tmp_reg, IgnoreDiscardedRegisters::No);
  }

  void StoreCRRegisters(BitSet8 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG,
                        FlushMode flush_mode = FlushMode::All)
  {
    FlushCRRegisters(regs, flush_mode, tmp_reg, IgnoreDiscardedRegisters::No);
  }

  void DiscardCRRegisters(BitSet8 regs);
  void ResetCRRegisters(BitSet8 regs);

protected:
  // Get the order of the host registers
  void GetAllocationOrder() override;

  // Flushes a guest register by host provided
  void FlushByHost(Arm64Gen::ARM64Reg host_reg,
                   Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG) override;

  void FlushRegister(size_t index, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) override;

private:
  bool IsCallerSaved(Arm64Gen::ARM64Reg reg) const;

  struct GuestRegInfo
  {
    size_t bitsize;
    size_t ppc_offset;
    OpArg& reg;
  };

  const OpArg& GetGuestGPROpArg(size_t preg) const;
  GuestRegInfo GetGuestGPR(size_t preg);
  GuestRegInfo GetGuestCR(size_t preg);
  GuestRegInfo GetGuestByIndex(size_t index);

  Arm64Gen::ARM64Reg BindForRead(size_t index);
  void SetImmediateInternal(size_t index, u32 imm, bool dirty);
  void BindForWrite(size_t index, bool will_read, bool will_write = true);

  void FlushRegisters(BitSet32 regs, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg,
                      IgnoreDiscardedRegisters ignore_discarded_registers);
  void FlushCRRegisters(BitSet8 regs, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg,
                        IgnoreDiscardedRegisters ignore_discarded_registers);

  static constexpr size_t GUEST_GPR_COUNT = 32;
  static constexpr size_t GUEST_CR_COUNT = 8;
  static constexpr size_t GUEST_GPR_OFFSET = 0;
  static constexpr size_t GUEST_CR_OFFSET = GUEST_GPR_COUNT;
};

class Arm64FPRCache : public Arm64RegCache
{
public:
  Arm64FPRCache();

  // Flushes the register cache in different ways depending on the mode.
  // The temporary register can be set to ARM64Reg::INVALID_REG when convenient for the caller.
  void Flush(
      FlushMode mode, Arm64Gen::ARM64Reg tmp_reg,
      IgnoreDiscardedRegisters ignore_discarded_registers = IgnoreDiscardedRegisters::No) override;

  // Returns a guest register inside of a host register
  // Will dump an immediate to the host register as well
  Arm64Gen::ARM64Reg R(size_t preg, RegType format);

  Arm64Gen::ARM64Reg RW(size_t preg, RegType format, bool set_dirty = true);

  BitSet32 GetCallerSavedUsed() const override;

  bool IsSingle(size_t preg, bool lower_only = false) const;

  void FixSinglePrecision(size_t preg);

  void StoreRegisters(BitSet32 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG,
                      FlushMode flush_mode = FlushMode::All)
  {
    FlushRegisters(regs, flush_mode, tmp_reg);
  }

protected:
  // Get the order of the host registers
  void GetAllocationOrder() override;

  // Flushes a guest register by host provided
  void FlushByHost(Arm64Gen::ARM64Reg host_reg,
                   Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG) override;

  void FlushRegister(size_t preg, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) override;

private:
  bool IsCallerSaved(Arm64Gen::ARM64Reg reg) const;
  bool IsTopHalfUsed(Arm64Gen::ARM64Reg reg) const;

  void FlushRegisters(BitSet32 regs, FlushMode mode, Arm64Gen::ARM64Reg tmp_reg);
};
