// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/JitArm64/Jit.h"

using namespace Arm64Gen;

void Arm64RegCache::Init(JitArm64* jit)
{
  m_jit = jit;
  m_emit = jit;
  m_float_emit.reset(new ARM64FloatEmitter(m_emit));
  GetAllocationOrder();
}

void Arm64RegCache::DiscardRegisters(BitSet32 regs)
{
  for (int i : regs)
    DiscardRegister(i);
}

void Arm64RegCache::ResetRegisters(BitSet32 regs)
{
  for (int i : regs)
  {
    OpArg& reg = m_guest_registers[i];
    ARM64Reg host_reg = reg.GetReg();

    ASSERT_MSG(DYNA_REC, host_reg == ARM64Reg::INVALID_REG,
               "Attempted to reset a loaded register (did you mean to flush it?)");
    reg.Flush();
  }
}

ARM64Reg Arm64RegCache::GetReg()
{
  // If we have no registers left, dump the most stale register first
  if (GetUnlockedRegisterCount() == 0)
    FlushMostStaleRegister();

  for (auto& it : m_host_registers)
  {
    if (!it.IsLocked())
    {
      it.Lock();
      return it.GetReg();
    }
  }
  // Holy cow, how did you run out of registers?
  // We can't return anything reasonable in this case. Return INVALID_REG and watch the failure
  // happen
  ASSERT_MSG(DYNA_REC, 0, "All available registers are locked!");
  return ARM64Reg::INVALID_REG;
}

void Arm64RegCache::UpdateLastUsed(BitSet32 regs_used)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    OpArg& reg = m_guest_registers[i];
    if (i < 32 && regs_used[i])
      reg.ResetLastUsed();
    else
      reg.IncrementLastUsed();
  }
}

u32 Arm64RegCache::GetUnlockedRegisterCount() const
{
  u32 unlocked_registers = 0;
  for (const auto& it : m_host_registers)
  {
    if (!it.IsLocked())
      ++unlocked_registers;
  }
  return unlocked_registers;
}

void Arm64RegCache::LockRegister(ARM64Reg host_reg)
{
  auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
  ASSERT_MSG(DYNA_REC, reg != m_host_registers.end(),
             "Don't try locking a register that isn't in the cache. Reg {}",
             static_cast<int>(host_reg));
  reg->Lock();
}

void Arm64RegCache::UnlockRegister(ARM64Reg host_reg)
{
  auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
  ASSERT_MSG(DYNA_REC, reg != m_host_registers.end(),
             "Don't try unlocking a register that isn't in the cache. Reg {}",
             static_cast<int>(host_reg));
  reg->Unlock();
}

void Arm64RegCache::FlushMostStaleRegister()
{
  size_t most_stale_preg = 0;
  u32 most_stale_amount = 0;

  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const auto& reg = m_guest_registers[i];
    const u32 last_used = reg.GetLastUsed();

    if (last_used > most_stale_amount && reg.GetType() != RegType::NotLoaded &&
        reg.GetType() != RegType::Discarded && reg.GetType() != RegType::Immediate)
    {
      most_stale_preg = i;
      most_stale_amount = last_used;
    }
  }

  FlushRegister(most_stale_preg, false, ARM64Reg::INVALID_REG);
}

void Arm64RegCache::DiscardRegister(size_t preg)
{
  OpArg& reg = m_guest_registers[preg];
  ARM64Reg host_reg = reg.GetReg();

  reg.Discard();
  if (host_reg != ARM64Reg::INVALID_REG)
    UnlockRegister(host_reg);
}

// GPR Cache
constexpr size_t GUEST_GPR_COUNT = 32;
constexpr size_t GUEST_CR_COUNT = 8;
constexpr size_t GUEST_GPR_OFFSET = 0;
constexpr size_t GUEST_CR_OFFSET = GUEST_GPR_COUNT;

Arm64GPRCache::Arm64GPRCache() : Arm64RegCache(GUEST_GPR_COUNT + GUEST_CR_COUNT)
{
}

void Arm64GPRCache::Start(PPCAnalyst::BlockRegStats& stats)
{
}

bool Arm64GPRCache::IsCallerSaved(ARM64Reg reg) const
{
  return ARM64XEmitter::CALLER_SAVED_GPRS[DecodeReg(reg)];
}

const OpArg& Arm64GPRCache::GetGuestGPROpArg(size_t preg) const
{
  ASSERT(preg < GUEST_GPR_COUNT);
  return m_guest_registers[preg];
}

Arm64GPRCache::GuestRegInfo Arm64GPRCache::GetGuestGPR(size_t preg)
{
  ASSERT(preg < GUEST_GPR_COUNT);
  return {32, PPCSTATE_OFF_GPR(preg), m_guest_registers[GUEST_GPR_OFFSET + preg]};
}

Arm64GPRCache::GuestRegInfo Arm64GPRCache::GetGuestCR(size_t preg)
{
  ASSERT(preg < GUEST_CR_COUNT);
  return {64, PPCSTATE_OFF_CR(preg), m_guest_registers[GUEST_CR_OFFSET + preg]};
}

Arm64GPRCache::GuestRegInfo Arm64GPRCache::GetGuestByIndex(size_t index)
{
  if (index >= GUEST_GPR_OFFSET && index < GUEST_GPR_OFFSET + GUEST_GPR_COUNT)
    return GetGuestGPR(index - GUEST_GPR_OFFSET);
  if (index >= GUEST_CR_OFFSET && index < GUEST_CR_OFFSET + GUEST_CR_COUNT)
    return GetGuestCR(index - GUEST_CR_OFFSET);
  ASSERT_MSG(DYNA_REC, false, "Invalid index for guest register");
  return GetGuestGPR(0);
}

void Arm64GPRCache::FlushRegister(size_t index, bool maintain_state, ARM64Reg tmp_reg)
{
  GuestRegInfo guest_reg = GetGuestByIndex(index);
  OpArg& reg = guest_reg.reg;
  size_t bitsize = guest_reg.bitsize;

  if (reg.GetType() == RegType::Register)
  {
    ARM64Reg host_reg = reg.GetReg();
    if (reg.IsDirty())
      m_emit->STR(IndexType::Unsigned, host_reg, PPC_REG, u32(guest_reg.ppc_offset));

    if (!maintain_state)
    {
      UnlockRegister(EncodeRegTo32(host_reg));
      reg.Flush();
    }
  }
  else if (reg.GetType() == RegType::Immediate)
  {
    if (reg.IsDirty())
    {
      if (!reg.GetImm())
      {
        m_emit->STR(IndexType::Unsigned, bitsize == 64 ? ARM64Reg::ZR : ARM64Reg::WZR, PPC_REG,
                    u32(guest_reg.ppc_offset));
      }
      else
      {
        bool allocated_tmp_reg = false;
        if (tmp_reg != ARM64Reg::INVALID_REG)
        {
          ASSERT(IsGPR(tmp_reg));
        }
        else
        {
          ASSERT_MSG(DYNA_REC, !maintain_state,
                     "Flushing immediate while maintaining state requires temporary register");
          tmp_reg = GetReg();
          allocated_tmp_reg = true;
        }

        const ARM64Reg encoded_tmp_reg = bitsize != 64 ? tmp_reg : EncodeRegTo64(tmp_reg);

        m_emit->MOVI2R(encoded_tmp_reg, reg.GetImm());
        m_emit->STR(IndexType::Unsigned, encoded_tmp_reg, PPC_REG, u32(guest_reg.ppc_offset));

        if (allocated_tmp_reg)
          UnlockRegister(tmp_reg);
      }

      if (!maintain_state)
        reg.Flush();
    }
  }
}

void Arm64GPRCache::FlushRegisters(BitSet32 regs, bool maintain_state, ARM64Reg tmp_reg)
{
  for (size_t i = 0; i < GUEST_GPR_COUNT; ++i)
  {
    if (regs[i])
    {
      if (i + 1 < GUEST_GPR_COUNT && regs[i + 1])
      {
        // We've got two guest registers in a row to store
        OpArg& reg1 = m_guest_registers[i];
        OpArg& reg2 = m_guest_registers[i + 1];
        if (reg1.IsDirty() && reg2.IsDirty() && reg1.GetType() == RegType::Register &&
            reg2.GetType() == RegType::Register)
        {
          const size_t ppc_offset = GetGuestByIndex(i).ppc_offset;
          if (ppc_offset <= 252)
          {
            ARM64Reg RX1 = R(GetGuestByIndex(i));
            ARM64Reg RX2 = R(GetGuestByIndex(i + 1));
            m_emit->STP(IndexType::Signed, RX1, RX2, PPC_REG, u32(ppc_offset));
            if (!maintain_state)
            {
              UnlockRegister(EncodeRegTo32(RX1));
              UnlockRegister(EncodeRegTo32(RX2));
              reg1.Flush();
              reg2.Flush();
            }
            ++i;
            continue;
          }
        }
      }

      FlushRegister(GUEST_GPR_OFFSET + i, maintain_state, tmp_reg);
    }
  }
}

void Arm64GPRCache::FlushCRRegisters(BitSet32 regs, bool maintain_state, ARM64Reg tmp_reg)
{
  for (size_t i = 0; i < GUEST_CR_COUNT; ++i)
  {
    if (regs[i])
    {
      FlushRegister(GUEST_CR_OFFSET + i, maintain_state, tmp_reg);
    }
  }
}

void Arm64GPRCache::Flush(FlushMode mode, ARM64Reg tmp_reg)
{
  FlushRegisters(BitSet32(~0U), mode == FlushMode::MaintainState, tmp_reg);
  FlushCRRegisters(BitSet32(~0U), mode == FlushMode::MaintainState, tmp_reg);
}

ARM64Reg Arm64GPRCache::R(const GuestRegInfo& guest_reg)
{
  OpArg& reg = guest_reg.reg;
  size_t bitsize = guest_reg.bitsize;

  IncrementAllUsed();
  reg.ResetLastUsed();

  switch (reg.GetType())
  {
  case RegType::Register:  // already in a reg
    return reg.GetReg();
  case RegType::Immediate:  // Is an immediate
  {
    ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());
    m_emit->MOVI2R(host_reg, reg.GetImm());
    reg.Load(host_reg);
    reg.SetDirty(true);
    return host_reg;
  }
  break;
  case RegType::Discarded:
    ASSERT_MSG(DYNA_REC, false, "Attempted to read discarded register");
    break;
  case RegType::NotLoaded:  // Register isn't loaded at /all/
  {
    // This is a bit annoying. We try to keep these preloaded as much as possible
    // This can also happen on cases where PPCAnalyst isn't feeing us proper register usage
    // statistics
    ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());
    reg.Load(host_reg);
    reg.SetDirty(false);
    m_emit->LDR(IndexType::Unsigned, host_reg, PPC_REG, u32(guest_reg.ppc_offset));
    return host_reg;
  }
  break;
  default:
    ERROR_LOG_FMT(DYNA_REC, "Invalid OpArg Type!");
    break;
  }
  // We've got an issue if we end up here
  return ARM64Reg::INVALID_REG;
}

void Arm64GPRCache::SetImmediate(const GuestRegInfo& guest_reg, u32 imm, bool dirty)
{
  OpArg& reg = guest_reg.reg;
  if (reg.GetType() == RegType::Register)
    UnlockRegister(EncodeRegTo32(reg.GetReg()));
  reg.LoadToImm(imm);
  reg.SetDirty(dirty);
}

void Arm64GPRCache::BindToRegister(const GuestRegInfo& guest_reg, bool will_read, bool will_write)
{
  OpArg& reg = guest_reg.reg;
  const size_t bitsize = guest_reg.bitsize;

  reg.ResetLastUsed();

  const RegType reg_type = reg.GetType();
  if (reg_type == RegType::NotLoaded || reg_type == RegType::Discarded)
  {
    const ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());
    reg.Load(host_reg);
    reg.SetDirty(will_write);
    if (will_read)
    {
      ASSERT_MSG(DYNA_REC, reg_type != RegType::Discarded, "Attempted to load a discarded value");
      m_emit->LDR(IndexType::Unsigned, host_reg, PPC_REG, u32(guest_reg.ppc_offset));
    }
  }
  else if (reg_type == RegType::Immediate)
  {
    const ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());
    if (will_read || !will_write)
    {
      // TODO: Emitting this instruction when (!will_read && !will_write) would be unnecessary if we
      // had some way to indicate to Flush that the immediate value should be written to ppcState
      // even though there is a host register allocated
      m_emit->MOVI2R(host_reg, reg.GetImm());
    }
    reg.Load(host_reg);
    if (will_write)
      reg.SetDirty(true);
  }
  else if (will_write)
  {
    reg.SetDirty(true);
  }
}

void Arm64GPRCache::GetAllocationOrder()
{
  // Callee saved registers first in hopes that we will keep everything stored there first
  static constexpr auto allocation_order = {
      // Callee saved
      ARM64Reg::W27,
      ARM64Reg::W26,
      ARM64Reg::W25,
      ARM64Reg::W24,
      ARM64Reg::W23,
      ARM64Reg::W22,
      ARM64Reg::W21,
      ARM64Reg::W20,
      ARM64Reg::W19,

      // Caller saved
      ARM64Reg::W17,
      ARM64Reg::W16,
      ARM64Reg::W15,
      ARM64Reg::W14,
      ARM64Reg::W13,
      ARM64Reg::W12,
      ARM64Reg::W11,
      ARM64Reg::W10,
      ARM64Reg::W9,
      ARM64Reg::W8,
      ARM64Reg::W7,
      ARM64Reg::W6,
      ARM64Reg::W5,
      ARM64Reg::W4,
      ARM64Reg::W3,
      ARM64Reg::W2,
      ARM64Reg::W1,
      ARM64Reg::W0,
      ARM64Reg::W30,
  };

  for (ARM64Reg reg : allocation_order)
    m_host_registers.push_back(HostReg(reg));
}

BitSet32 Arm64GPRCache::GetCallerSavedUsed() const
{
  BitSet32 registers(0);
  for (const auto& it : m_host_registers)
  {
    if (it.IsLocked() && IsCallerSaved(it.GetReg()))
      registers[DecodeReg(it.GetReg())] = true;
  }
  return registers;
}

void Arm64GPRCache::FlushByHost(ARM64Reg host_reg, ARM64Reg tmp_reg)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const OpArg& reg = m_guest_registers[i];
    if (reg.GetType() == RegType::Register && DecodeReg(reg.GetReg()) == DecodeReg(host_reg))
    {
      FlushRegister(i, false, tmp_reg);
      return;
    }
  }
}

// FPR Cache
constexpr size_t GUEST_FPR_COUNT = 32;

Arm64FPRCache::Arm64FPRCache() : Arm64RegCache(GUEST_FPR_COUNT)
{
}

void Arm64FPRCache::Flush(FlushMode mode, ARM64Reg tmp_reg)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const RegType reg_type = m_guest_registers[i].GetType();

    if (reg_type != RegType::NotLoaded && reg_type != RegType::Discarded &&
        reg_type != RegType::Immediate)
    {
      FlushRegister(i, mode == FlushMode::MaintainState, tmp_reg);
    }
  }
}

ARM64Reg Arm64FPRCache::R(size_t preg, RegType type)
{
  OpArg& reg = m_guest_registers[preg];
  IncrementAllUsed();
  reg.ResetLastUsed();
  ARM64Reg host_reg = reg.GetReg();

  switch (reg.GetType())
  {
  case RegType::Single:
  {
    // We're asked for singles, so just return the register.
    if (type == RegType::Single || type == RegType::LowerPairSingle)
      return host_reg;

    // Else convert this register back to doubles.
    const ARM64Reg tmp_reg = GetReg();
    m_jit->ConvertSingleToDoublePair(preg, host_reg, host_reg, tmp_reg);
    UnlockRegister(tmp_reg);

    reg.Load(host_reg, RegType::Register);
    [[fallthrough]];
  }
  case RegType::Register:  // already in a reg
  {
    return host_reg;
  }
  case RegType::LowerPairSingle:
  {
    // We're asked for the lower single, so just return the register.
    if (type == RegType::LowerPairSingle)
      return host_reg;

    // Else convert this register back to a double.
    const ARM64Reg tmp_reg = GetReg();
    m_jit->ConvertSingleToDoubleLower(preg, host_reg, host_reg, tmp_reg);
    UnlockRegister(tmp_reg);

    reg.Load(host_reg, RegType::LowerPair);
    [[fallthrough]];
  }
  case RegType::LowerPair:
  {
    if (type == RegType::Register)
    {
      // Load the high 64bits from the file and insert them in to the high 64bits of the host
      // register
      const ARM64Reg tmp_reg = GetReg();
      m_float_emit->LDR(64, IndexType::Unsigned, tmp_reg, PPC_REG,
                        static_cast<s32>(PPCSTATE_OFF_PS1(preg)));
      m_float_emit->INS(64, host_reg, 1, tmp_reg, 0);
      UnlockRegister(tmp_reg);

      // Change it over to a full 128bit register
      reg.Load(host_reg, RegType::Register);
    }
    return host_reg;
  }
  case RegType::DuplicatedSingle:
  {
    if (type == RegType::LowerPairSingle)
      return host_reg;

    if (type == RegType::Single)
    {
      // Duplicate to the top and change over
      m_float_emit->INS(32, host_reg, 1, host_reg, 0);
      reg.Load(host_reg, RegType::Single);
      return host_reg;
    }

    const ARM64Reg tmp_reg = GetReg();
    m_jit->ConvertSingleToDoubleLower(preg, host_reg, host_reg, tmp_reg);
    UnlockRegister(tmp_reg);

    reg.Load(host_reg, RegType::Duplicated);
    [[fallthrough]];
  }
  case RegType::Duplicated:
  {
    if (type == RegType::Register)
    {
      // We are requesting a full 128bit register
      // but we are only available in the lower 64bits
      // Duplicate to the top and change over
      m_float_emit->INS(64, host_reg, 1, host_reg, 0);
      reg.Load(host_reg, RegType::Register);
    }
    return host_reg;
  }
  case RegType::Discarded:
    ASSERT_MSG(DYNA_REC, false, "Attempted to read discarded register");
    break;
  case RegType::NotLoaded:  // Register isn't loaded at /all/
  {
    host_reg = GetReg();
    u32 load_size;
    if (type == RegType::Register)
    {
      load_size = 128;
      reg.Load(host_reg, RegType::Register);
    }
    else
    {
      load_size = 64;
      reg.Load(host_reg, RegType::LowerPair);
    }
    reg.SetDirty(false);
    m_float_emit->LDR(load_size, IndexType::Unsigned, host_reg, PPC_REG,
                      static_cast<s32>(PPCSTATE_OFF_PS0(preg)));
    return host_reg;
  }
  default:
    DEBUG_ASSERT_MSG(DYNA_REC, false, "Invalid OpArg Type!");
    break;
  }
  // We've got an issue if we end up here
  return ARM64Reg::INVALID_REG;
}

ARM64Reg Arm64FPRCache::RW(size_t preg, RegType type, bool set_dirty)
{
  OpArg& reg = m_guest_registers[preg];

  IncrementAllUsed();
  reg.ResetLastUsed();

  // Only the lower value will be overwritten, so we must be extra careful to store PSR1 if dirty.
  if (reg.IsDirty() && (type == RegType::LowerPair || type == RegType::LowerPairSingle))
  {
    // We must *not* change host_reg as this register might still be in use. So it's fine to
    // store this register, but it's *not* fine to convert it to double. So for double conversion,
    // a temporary register needs to be used.
    ARM64Reg host_reg = reg.GetReg();
    ARM64Reg flush_reg = host_reg;

    switch (reg.GetType())
    {
    case RegType::Single:
      // For a store-safe register, conversion is just one instruction regardless of whether
      // we're whether we're converting a pair, so ConvertSingleToDoublePair followed by a
      // 128-bit store is faster than INS followed by ConvertSingleToDoubleLower and a
      // 64-bit store. But for registers which are not store-safe, the latter is better.
      flush_reg = GetReg();
      if (!m_jit->IsFPRStoreSafe(preg))
      {
        ARM64Reg scratch_reg = GetReg();
        m_float_emit->INS(32, flush_reg, 0, host_reg, 1);
        m_jit->ConvertSingleToDoubleLower(preg, flush_reg, flush_reg, scratch_reg);
        m_float_emit->STR(64, IndexType::Unsigned, flush_reg, PPC_REG, u32(PPCSTATE_OFF_PS1(preg)));
        Unlock(scratch_reg);
        reg.Load(host_reg, RegType::LowerPairSingle);
        break;
      }
      else
      {
        m_jit->ConvertSingleToDoublePair(preg, flush_reg, host_reg, flush_reg);
        m_float_emit->STR(128, IndexType::Unsigned, flush_reg, PPC_REG,
                          u32(PPCSTATE_OFF_PS0(preg)));
        reg.SetDirty(false);
      }
      break;
    case RegType::Register:
      // We are doing a full 128bit store because it takes 2 cycles on a Cortex-A57 to do a 128bit
      // store.
      // It would take longer to do an insert to a temporary and a 64bit store than to just do this.
      m_float_emit->STR(128, IndexType::Unsigned, flush_reg, PPC_REG,
                        static_cast<s32>(PPCSTATE_OFF_PS0(preg)));
      reg.SetDirty(false);
      break;
    case RegType::DuplicatedSingle:
      flush_reg = GetReg();
      m_jit->ConvertSingleToDoubleLower(preg, flush_reg, host_reg, flush_reg);
      [[fallthrough]];
    case RegType::Duplicated:
      // Store PSR1 (which is equal to PSR0) in memory.
      m_float_emit->STR(64, IndexType::Unsigned, flush_reg, PPC_REG,
                        static_cast<s32>(PPCSTATE_OFF_PS1(preg)));
      reg.Load(host_reg, reg.GetType() == RegType::DuplicatedSingle ? RegType::LowerPairSingle :
                                                                      RegType::LowerPair);
      break;
    default:
      // All other types doesn't store anything in PSR1.
      break;
    }

    if (host_reg != flush_reg)
      Unlock(flush_reg);
  }

  if (reg.GetType() == RegType::NotLoaded || reg.GetType() == RegType::Discarded)
  {
    // If not loaded at all, just alloc a new one.
    reg.Load(GetReg(), type);
    reg.SetDirty(set_dirty);
  }
  else if (set_dirty)
  {
    reg.Load(reg.GetReg(), type);
    reg.SetDirty(true);
  }

  return reg.GetReg();
}

void Arm64FPRCache::GetAllocationOrder()
{
  static constexpr auto allocation_order = {
      // Callee saved
      ARM64Reg::Q8,
      ARM64Reg::Q9,
      ARM64Reg::Q10,
      ARM64Reg::Q11,
      ARM64Reg::Q12,
      ARM64Reg::Q13,
      ARM64Reg::Q14,
      ARM64Reg::Q15,

      // Caller saved
      ARM64Reg::Q16,
      ARM64Reg::Q17,
      ARM64Reg::Q18,
      ARM64Reg::Q19,
      ARM64Reg::Q20,
      ARM64Reg::Q21,
      ARM64Reg::Q22,
      ARM64Reg::Q23,
      ARM64Reg::Q24,
      ARM64Reg::Q25,
      ARM64Reg::Q26,
      ARM64Reg::Q27,
      ARM64Reg::Q28,
      ARM64Reg::Q29,
      ARM64Reg::Q30,
      ARM64Reg::Q31,
      ARM64Reg::Q7,
      ARM64Reg::Q6,
      ARM64Reg::Q5,
      ARM64Reg::Q4,
      ARM64Reg::Q3,
      ARM64Reg::Q2,
      ARM64Reg::Q1,
      ARM64Reg::Q0,
  };

  for (ARM64Reg reg : allocation_order)
    m_host_registers.push_back(HostReg(reg));
}

void Arm64FPRCache::FlushByHost(ARM64Reg host_reg, ARM64Reg tmp_reg)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const OpArg& reg = m_guest_registers[i];
    const RegType reg_type = reg.GetType();

    if (reg_type != RegType::NotLoaded && reg_type != RegType::Discarded &&
        reg_type != RegType::Immediate && reg.GetReg() == host_reg)
    {
      FlushRegister(i, false, tmp_reg);
      return;
    }
  }
}

bool Arm64FPRCache::IsCallerSaved(ARM64Reg reg) const
{
  return ARM64XEmitter::CALLER_SAVED_FPRS[DecodeReg(reg)];
}

bool Arm64FPRCache::IsTopHalfUsed(ARM64Reg reg) const
{
  for (const OpArg& r : m_guest_registers)
  {
    if (r.GetReg() != ARM64Reg::INVALID_REG && DecodeReg(r.GetReg()) == DecodeReg(reg))
      return r.GetType() == RegType::Register;
  }

  return false;
}

void Arm64FPRCache::FlushRegister(size_t preg, bool maintain_state, ARM64Reg tmp_reg)
{
  OpArg& reg = m_guest_registers[preg];
  const ARM64Reg host_reg = reg.GetReg();
  const bool dirty = reg.IsDirty();
  RegType type = reg.GetType();

  bool allocated_tmp_reg = false;
  if (tmp_reg != ARM64Reg::INVALID_REG)
  {
    ASSERT(IsVector(tmp_reg));
  }
  else if (GetUnlockedRegisterCount() > 0)
  {
    // Calling GetReg here with 0 registers free could cause problems for two reasons:
    //
    // 1. When GetReg needs to flush, it calls this function, which can lead to infinite recursion
    // 2. When GetReg needs to flush, it does not respect maintain_state == true
    //
    // So if we have 0 registers free, just don't allocate a temporary register.
    // The emitted code will still work but might be a little less efficient.

    tmp_reg = GetReg();
    allocated_tmp_reg = true;
  }

  // If we're in single mode, just convert it back to a double.
  if (type == RegType::Single)
  {
    if (dirty)
      m_jit->ConvertSingleToDoublePair(preg, host_reg, host_reg, tmp_reg);
    type = RegType::Register;
  }
  if (type == RegType::DuplicatedSingle || type == RegType::LowerPairSingle)
  {
    if (dirty)
      m_jit->ConvertSingleToDoubleLower(preg, host_reg, host_reg, tmp_reg);

    if (type == RegType::DuplicatedSingle)
      type = RegType::Duplicated;
    else
      type = RegType::LowerPair;
  }

  if (type == RegType::Register || type == RegType::LowerPair)
  {
    u32 store_size;
    if (type == RegType::Register)
      store_size = 128;
    else
      store_size = 64;

    if (dirty)
    {
      m_float_emit->STR(store_size, IndexType::Unsigned, host_reg, PPC_REG,
                        static_cast<s32>(PPCSTATE_OFF_PS0(preg)));
    }

    if (!maintain_state)
    {
      UnlockRegister(host_reg);
      reg.Flush();
    }
  }
  else if (type == RegType::Duplicated)
  {
    if (dirty)
    {
      if (PPCSTATE_OFF_PS0(preg) <= 504)
      {
        m_float_emit->STP(64, IndexType::Signed, host_reg, host_reg, PPC_REG,
                          static_cast<s32>(PPCSTATE_OFF_PS0(preg)));
      }
      else
      {
        m_float_emit->STR(64, IndexType::Unsigned, host_reg, PPC_REG,
                          static_cast<s32>(PPCSTATE_OFF_PS0(preg)));
        m_float_emit->STR(64, IndexType::Unsigned, host_reg, PPC_REG,
                          static_cast<s32>(PPCSTATE_OFF_PS1(preg)));
      }
    }

    if (!maintain_state)
    {
      UnlockRegister(host_reg);
      reg.Flush();
    }
  }

  if (allocated_tmp_reg)
    UnlockRegister(tmp_reg);
}

void Arm64FPRCache::FlushRegisters(BitSet32 regs, bool maintain_state, ARM64Reg tmp_reg)
{
  for (int j : regs)
    FlushRegister(j, maintain_state, tmp_reg);
}

BitSet32 Arm64FPRCache::GetCallerSavedUsed() const
{
  BitSet32 registers(0);
  for (const auto& it : m_host_registers)
  {
    if (it.IsLocked() && (IsCallerSaved(it.GetReg()) || IsTopHalfUsed(it.GetReg())))
      registers[DecodeReg(it.GetReg())] = true;
  }
  return registers;
}

bool Arm64FPRCache::IsSingle(size_t preg, bool lower_only) const
{
  const RegType type = m_guest_registers[preg].GetType();
  return type == RegType::Single || type == RegType::DuplicatedSingle ||
         (lower_only && type == RegType::LowerPairSingle);
}

void Arm64FPRCache::FixSinglePrecision(size_t preg)
{
  OpArg& reg = m_guest_registers[preg];
  ARM64Reg host_reg = reg.GetReg();
  switch (reg.GetType())
  {
  case RegType::Duplicated:  // only PS0 needs to be converted
    m_float_emit->FCVT(32, 64, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
    reg.Load(host_reg, RegType::DuplicatedSingle);
    break;
  case RegType::Register:  // PS0 and PS1 need to be converted
    m_float_emit->FCVTN(32, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
    reg.Load(host_reg, RegType::Single);
    break;
  default:
    break;
  }
}
