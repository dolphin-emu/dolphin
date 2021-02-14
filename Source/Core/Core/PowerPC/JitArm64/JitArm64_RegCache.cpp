// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitArm64/JitArm64_RegCache.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

#include "Common/Assert.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/JitArm64/Jit.h"

using namespace Arm64Gen;

void Arm64RegCache::Init(ARM64XEmitter* emitter)
{
  m_emit = emitter;
  m_float_emit.reset(new ARM64FloatEmitter(m_emit));
  GetAllocationOrder();
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
  return INVALID_REG;
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
             "Don't try locking a register that isn't in the cache. Reg %d", host_reg);
  reg->Lock();
}

void Arm64RegCache::UnlockRegister(ARM64Reg host_reg)
{
  auto reg = std::find(m_host_registers.begin(), m_host_registers.end(), host_reg);
  ASSERT_MSG(DYNA_REC, reg != m_host_registers.end(),
             "Don't try unlocking a register that isn't in the cache. Reg %d", host_reg);
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

    if (last_used > most_stale_amount &&
        (reg.GetType() != RegType::NotLoaded && reg.GetType() != RegType::Immediate))
    {
      most_stale_preg = i;
      most_stale_amount = last_used;
    }
  }

  FlushRegister(most_stale_preg, false);
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

bool Arm64GPRCache::IsCalleeSaved(ARM64Reg reg) const
{
  static constexpr std::array<ARM64Reg, 11> callee_regs{{
      X28,
      X27,
      X26,
      X25,
      X24,
      X23,
      X22,
      X21,
      X20,
      X19,
      INVALID_REG,
  }};

  return std::find(callee_regs.begin(), callee_regs.end(), EncodeRegTo64(reg)) != callee_regs.end();
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

void Arm64GPRCache::FlushRegister(size_t index, bool maintain_state)
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
      UnlockRegister(DecodeReg(host_reg));
      reg.Flush();
    }
  }
  else if (reg.GetType() == RegType::Immediate)
  {
    if (!reg.GetImm())
    {
      m_emit->STR(IndexType::Unsigned, bitsize == 64 ? ZR : WZR, PPC_REG,
                  u32(guest_reg.ppc_offset));
    }
    else
    {
      ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());

      m_emit->MOVI2R(host_reg, reg.GetImm());
      m_emit->STR(IndexType::Unsigned, host_reg, PPC_REG, u32(guest_reg.ppc_offset));

      UnlockRegister(DecodeReg(host_reg));
    }

    if (!maintain_state)
      reg.Flush();
  }
}

void Arm64GPRCache::FlushRegisters(BitSet32 regs, bool maintain_state)
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
              UnlockRegister(DecodeReg(RX1));
              UnlockRegister(DecodeReg(RX2));
              reg1.Flush();
              reg2.Flush();
            }
            ++i;
            continue;
          }
        }
      }

      FlushRegister(GUEST_GPR_OFFSET + i, maintain_state);
    }
  }
}

void Arm64GPRCache::FlushCRRegisters(BitSet32 regs, bool maintain_state)
{
  for (size_t i = 0; i < GUEST_CR_COUNT; ++i)
  {
    if (regs[i])
    {
      FlushRegister(GUEST_CR_OFFSET + i, maintain_state);
    }
  }
}

void Arm64GPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
  FlushRegisters(BitSet32(~0U), mode == FlushMode::MaintainState);
  FlushCRRegisters(BitSet32(~0U), mode == FlushMode::MaintainState);
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
  return INVALID_REG;
}

void Arm64GPRCache::SetImmediate(const GuestRegInfo& guest_reg, u32 imm)
{
  OpArg& reg = guest_reg.reg;
  if (reg.GetType() == RegType::Register)
    UnlockRegister(DecodeReg(reg.GetReg()));
  reg.LoadToImm(imm);
}

void Arm64GPRCache::BindToRegister(const GuestRegInfo& guest_reg, bool do_load)
{
  OpArg& reg = guest_reg.reg;
  const size_t bitsize = guest_reg.bitsize;

  reg.ResetLastUsed();

  reg.SetDirty(true);
  if (reg.GetType() == RegType::NotLoaded)
  {
    const ARM64Reg host_reg = bitsize != 64 ? GetReg() : EncodeRegTo64(GetReg());
    reg.Load(host_reg);
    if (do_load)
      m_emit->LDR(IndexType::Unsigned, host_reg, PPC_REG, u32(guest_reg.ppc_offset));
  }
}

void Arm64GPRCache::GetAllocationOrder()
{
  // Callee saved registers first in hopes that we will keep everything stored there first
  static constexpr std::array<ARM64Reg, 29> allocation_order{{
      // Callee saved
      W27,
      W26,
      W25,
      W24,
      W23,
      W22,
      W21,
      W20,
      W19,

      // Caller saved
      W17,
      W16,
      W15,
      W14,
      W13,
      W12,
      W11,
      W10,
      W9,
      W8,
      W7,
      W6,
      W5,
      W4,
      W3,
      W2,
      W1,
      W0,
      W30,
  }};

  for (ARM64Reg reg : allocation_order)
    m_host_registers.push_back(HostReg(reg));
}

BitSet32 Arm64GPRCache::GetCallerSavedUsed() const
{
  BitSet32 registers(0);
  for (const auto& it : m_host_registers)
  {
    if (it.IsLocked() && !IsCalleeSaved(it.GetReg()))
      registers[DecodeReg(it.GetReg())] = true;
  }
  return registers;
}

void Arm64GPRCache::FlushByHost(ARM64Reg host_reg)
{
  host_reg = DecodeReg(host_reg);
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const OpArg& reg = m_guest_registers[i];
    if (reg.GetType() == RegType::Register && DecodeReg(reg.GetReg()) == host_reg)
    {
      FlushRegister(i, false);
      return;
    }
  }
}

// FPR Cache
constexpr size_t GUEST_FPR_COUNT = 32;

Arm64FPRCache::Arm64FPRCache() : Arm64RegCache(GUEST_FPR_COUNT)
{
}

void Arm64FPRCache::Flush(FlushMode mode, PPCAnalyst::CodeOp* op)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const RegType reg_type = m_guest_registers[i].GetType();

    if (reg_type != RegType::NotLoaded && reg_type != RegType::Immediate)
    {
      // XXX: Determine if we can keep a register in the lower 64bits
      // Which will allow it to be callee saved.
      FlushRegister(i, mode == FlushMode::MaintainState);
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
    m_float_emit->FCVTL(64, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
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
    m_float_emit->FCVT(64, 32, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
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
      m_float_emit->LDR(64, IndexType::Unsigned, tmp_reg, PPC_REG, u32(PPCSTATE_OFF_PS1(preg)));
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

    m_float_emit->FCVT(64, 32, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
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
                      u32(PPCSTATE_OFF_PS0(preg)));
    return host_reg;
  }
  default:
    DEBUG_ASSERT_MSG(DYNA_REC, false, "Invalid OpArg Type!");
    break;
  }
  // We've got an issue if we end up here
  return INVALID_REG;
}

ARM64Reg Arm64FPRCache::RW(size_t preg, RegType type)
{
  OpArg& reg = m_guest_registers[preg];

  bool was_dirty = reg.IsDirty();

  IncrementAllUsed();
  reg.ResetLastUsed();

  reg.SetDirty(true);

  // If not loaded at all, just alloc a new one.
  if (reg.GetType() == RegType::NotLoaded)
  {
    reg.Load(GetReg(), type);
    return reg.GetReg();
  }

  // Only the lower value will be overwritten, so we must be extra careful to store PSR1 if dirty.
  if ((type == RegType::LowerPair || type == RegType::LowerPairSingle) && was_dirty)
  {
    // We must *not* change host_reg as this register might still be in use. So it's fine to
    // store this register, but it's *not* fine to convert it to double. So for double convertion,
    // a temporary register needs to be used.
    ARM64Reg host_reg = reg.GetReg();
    ARM64Reg flush_reg = host_reg;

    switch (reg.GetType())
    {
    case RegType::Single:
      flush_reg = GetReg();
      m_float_emit->FCVTL(64, EncodeRegToDouble(flush_reg), EncodeRegToDouble(host_reg));
      [[fallthrough]];
    case RegType::Register:
      // We are doing a full 128bit store because it takes 2 cycles on a Cortex-A57 to do a 128bit
      // store.
      // It would take longer to do an insert to a temporary and a 64bit store than to just do this.
      m_float_emit->STR(128, IndexType::Unsigned, flush_reg, PPC_REG, u32(PPCSTATE_OFF_PS0(preg)));
      break;
    case RegType::DuplicatedSingle:
      flush_reg = GetReg();
      m_float_emit->FCVT(64, 32, EncodeRegToDouble(flush_reg), EncodeRegToDouble(host_reg));
      [[fallthrough]];
    case RegType::Duplicated:
      // Store PSR1 (which is equal to PSR0) in memory.
      m_float_emit->STR(64, IndexType::Unsigned, flush_reg, PPC_REG, u32(PPCSTATE_OFF_PS1(preg)));
      break;
    default:
      // All other types doesn't store anything in PSR1.
      break;
    }

    if (host_reg != flush_reg)
      Unlock(flush_reg);
  }

  reg.Load(reg.GetReg(), type);
  return reg.GetReg();
}

void Arm64FPRCache::GetAllocationOrder()
{
  static constexpr std::array<ARM64Reg, 32> allocation_order{{
      // Callee saved
      Q8,
      Q9,
      Q10,
      Q11,
      Q12,
      Q13,
      Q14,
      Q15,

      // Caller saved
      Q16,
      Q17,
      Q18,
      Q19,
      Q20,
      Q21,
      Q22,
      Q23,
      Q24,
      Q25,
      Q26,
      Q27,
      Q28,
      Q29,
      Q30,
      Q31,
      Q7,
      Q6,
      Q5,
      Q4,
      Q3,
      Q2,
      Q1,
      Q0,
  }};

  for (ARM64Reg reg : allocation_order)
    m_host_registers.push_back(HostReg(reg));
}

void Arm64FPRCache::FlushByHost(ARM64Reg host_reg)
{
  for (size_t i = 0; i < m_guest_registers.size(); ++i)
  {
    const OpArg& reg = m_guest_registers[i];
    const RegType reg_type = reg.GetType();

    if ((reg_type != RegType::NotLoaded && reg_type != RegType::Immediate) &&
        reg.GetReg() == host_reg)
    {
      FlushRegister(i, false);
      return;
    }
  }
}

bool Arm64FPRCache::IsCalleeSaved(ARM64Reg reg) const
{
  static constexpr std::array<ARM64Reg, 9> callee_regs{{
      Q8,
      Q9,
      Q10,
      Q11,
      Q12,
      Q13,
      Q14,
      Q15,
      INVALID_REG,
  }};

  return std::find(callee_regs.begin(), callee_regs.end(), reg) != callee_regs.end();
}

void Arm64FPRCache::FlushRegister(size_t preg, bool maintain_state)
{
  OpArg& reg = m_guest_registers[preg];
  const ARM64Reg host_reg = reg.GetReg();
  const bool dirty = reg.IsDirty();
  RegType type = reg.GetType();

  // If we're in single mode, just convert it back to a double.
  if (type == RegType::Single)
  {
    if (dirty)
      m_float_emit->FCVTL(64, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
    type = RegType::Register;
  }
  if (type == RegType::DuplicatedSingle || type == RegType::LowerPairSingle)
  {
    if (dirty)
      m_float_emit->FCVT(64, 32, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));

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
                        u32(PPCSTATE_OFF_PS0(preg)));
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
                          PPCSTATE_OFF_PS0(preg));
      }
      else
      {
        m_float_emit->STR(64, IndexType::Unsigned, host_reg, PPC_REG, u32(PPCSTATE_OFF_PS0(preg)));
        m_float_emit->STR(64, IndexType::Unsigned, host_reg, PPC_REG, u32(PPCSTATE_OFF_PS1(preg)));
      }
    }

    if (!maintain_state)
    {
      UnlockRegister(host_reg);
      reg.Flush();
    }
  }
}

void Arm64FPRCache::FlushRegisters(BitSet32 regs, bool maintain_state)
{
  for (int j : regs)
    FlushRegister(j, maintain_state);
}

BitSet32 Arm64FPRCache::GetCallerSavedUsed() const
{
  BitSet32 registers(0);
  for (const auto& it : m_host_registers)
  {
    if (it.IsLocked())
      registers[it.GetReg() - Q0] = true;
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
  case RegType::Register:  // PS0 and PS1 needs to be converted
    m_float_emit->FCVTN(32, EncodeRegToDouble(host_reg), EncodeRegToDouble(host_reg));
    reg.Load(host_reg, RegType::Single);
    break;
  default:
    break;
  }
}
