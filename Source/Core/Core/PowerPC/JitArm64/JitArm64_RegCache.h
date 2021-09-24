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

#define PPCSTATE_OFF(elem) (offsetof(PowerPC::PowerPCState, elem))

#define PPCSTATE_OFF_ARRAY(elem, i)                                                                \
  (offsetof(PowerPC::PowerPCState, elem[0]) + sizeof(PowerPC::PowerPCState::elem[0]) * (i))

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
  NotLoaded,
  Discarded,   // Reg is not loaded because we know it won't be read before the next write
  Register,    // Reg type is register
  Immediate,   // Reg is really a IMM
  LowerPair,   // Only the lower pair of a paired register
  Duplicated,  // The lower reg is the same as the upper one (physical upper doesn't actually have
               // the duplicated value)
  Single,      // Both registers are loaded as single
  LowerPairSingle,   // Only the lower pair of a paired register, as single
  DuplicatedSingle,  // The lower one contains both registers, as single
};

enum class FlushMode
{
  // Flushes all registers, no exceptions
  All,
  // Flushes registers in a conditional branch
  // Doesn't wipe the state of the registers from the cache
  MaintainState,
};

class OpArg
{
public:
  OpArg() = default;

  RegType GetType() const { return m_type; }
  Arm64Gen::ARM64Reg GetReg() const { return m_reg; }
  u32 GetImm() const { return m_value; }
  void Load(Arm64Gen::ARM64Reg reg, RegType type = RegType::Register)
  {
    m_type = type;
    m_reg = reg;
  }
  void LoadToImm(u32 imm)
  {
    m_type = RegType::Immediate;
    m_value = imm;

    m_reg = Arm64Gen::ARM64Reg::INVALID_REG;
  }
  void Discard()
  {
    // Invalidate any previous information
    m_type = RegType::Discarded;
    m_reg = Arm64Gen::ARM64Reg::INVALID_REG;

    // Arbitrarily large value that won't roll over on a lot of increments
    m_last_used = 0xFFFF;
  }
  void Flush()
  {
    // Invalidate any previous information
    m_type = RegType::NotLoaded;
    m_reg = Arm64Gen::ARM64Reg::INVALID_REG;

    // Arbitrarily large value that won't roll over on a lot of increments
    m_last_used = 0xFFFF;
  }

  u32 GetLastUsed() const { return m_last_used; }
  void ResetLastUsed() { m_last_used = 0; }
  void IncrementLastUsed() { ++m_last_used; }
  void SetDirty(bool dirty) { m_dirty = dirty; }
  bool IsDirty() const { return m_dirty; }

private:
  // For REG_REG
  RegType m_type = RegType::NotLoaded;                         // store type
  Arm64Gen::ARM64Reg m_reg = Arm64Gen::ARM64Reg::INVALID_REG;  // host register we are in

  // For REG_IMM
  u32 m_value = 0;  // IMM value

  u32 m_last_used = 0;

  bool m_dirty = false;
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

  bool operator==(Arm64Gen::ARM64Reg reg) const { return reg == m_reg; }
  bool operator!=(Arm64Gen::ARM64Reg reg) const { return !operator==(reg); }

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
  virtual void Flush(FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) = 0;

  virtual BitSet32 GetCallerSavedUsed() const = 0;

  // Returns a temporary register for use
  // Requires unlocking after done
  Arm64Gen::ARM64Reg GetReg();

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
  virtual void FlushRegister(size_t preg, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg) = 0;

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
  void Flush(FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) override;

  // Returns a guest GPR inside of a host register
  // Will dump an immediate to the host register as well
  Arm64Gen::ARM64Reg R(size_t preg) { return R(GetGuestGPR(preg)); }
  // Returns a guest CR inside of a host register
  Arm64Gen::ARM64Reg CR(size_t preg) { return R(GetGuestCR(preg)); }
  // Set a register to an immediate, only valid for guest GPRs
  void SetImmediate(size_t preg, u32 imm) { SetImmediate(GetGuestGPR(preg), imm); }
  // Returns if a register is set as an immediate, only valid for guest GPRs
  bool IsImm(size_t preg) const { return GetGuestGPROpArg(preg).GetType() == RegType::Immediate; }
  // Gets the immediate that a register is set to, only valid for guest GPRs
  u32 GetImm(size_t preg) const { return GetGuestGPROpArg(preg).GetImm(); }
  // Binds a guest GPR to a host register, optionally loading its value
  void BindToRegister(size_t preg, bool do_load) { BindToRegister(GetGuestGPR(preg), do_load); }
  // Binds a guest CR to a host register, optionally loading its value
  void BindCRToRegister(size_t preg, bool do_load) { BindToRegister(GetGuestCR(preg), do_load); }
  BitSet32 GetCallerSavedUsed() const override;

  void StoreRegisters(BitSet32 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG)
  {
    FlushRegisters(regs, false, tmp_reg);
  }
  void StoreCRRegisters(BitSet32 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG)
  {
    FlushCRRegisters(regs, false, tmp_reg);
  }

protected:
  // Get the order of the host registers
  void GetAllocationOrder() override;

  // Flushes a guest register by host provided
  void FlushByHost(Arm64Gen::ARM64Reg host_reg,
                   Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG) override;

  void FlushRegister(size_t index, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg) override;

private:
  bool IsCalleeSaved(Arm64Gen::ARM64Reg reg) const;

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

  Arm64Gen::ARM64Reg R(const GuestRegInfo& guest_reg);
  void SetImmediate(const GuestRegInfo& guest_reg, u32 imm);
  void BindToRegister(const GuestRegInfo& guest_reg, bool do_load);

  void FlushRegisters(BitSet32 regs, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg);
  void FlushCRRegisters(BitSet32 regs, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg);
};

class Arm64FPRCache : public Arm64RegCache
{
public:
  Arm64FPRCache();

  // Flushes the register cache in different ways depending on the mode.
  // The temporary register can be set to ARM64Reg::INVALID_REG when convenient for the caller.
  void Flush(FlushMode mode, Arm64Gen::ARM64Reg tmp_reg) override;

  // Returns a guest register inside of a host register
  // Will dump an immediate to the host register as well
  Arm64Gen::ARM64Reg R(size_t preg, RegType type);

  Arm64Gen::ARM64Reg RW(size_t preg, RegType type);

  BitSet32 GetCallerSavedUsed() const override;

  bool IsSingle(size_t preg, bool lower_only = false) const;

  void FixSinglePrecision(size_t preg);

  void StoreRegisters(BitSet32 regs, Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG)
  {
    FlushRegisters(regs, false, tmp_reg);
  }

protected:
  // Get the order of the host registers
  void GetAllocationOrder() override;

  // Flushes a guest register by host provided
  void FlushByHost(Arm64Gen::ARM64Reg host_reg,
                   Arm64Gen::ARM64Reg tmp_reg = Arm64Gen::ARM64Reg::INVALID_REG) override;

  void FlushRegister(size_t preg, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg) override;

private:
  bool IsCalleeSaved(Arm64Gen::ARM64Reg reg) const;
  bool IsTopHalfUsed(Arm64Gen::ARM64Reg reg) const;

  void FlushRegisters(BitSet32 regs, bool maintain_state, Arm64Gen::ARM64Reg tmp_reg);
};
