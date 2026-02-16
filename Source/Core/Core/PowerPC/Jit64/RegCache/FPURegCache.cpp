// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/RegCache/FPURegCache.h"

#include <array>

#include "Common/x64Reg.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

FPURegCache::FPURegCache(Jit64& jit, XEmitter& emitter) : RegCache{jit, emitter}
{
}

bool FPURegCache::IsImm(preg_t preg) const
{
  return false;
}

u32 FPURegCache::Imm32(preg_t preg) const
{
  ASSERT_MSG(DYNA_REC, false, "FPURegCache doesn't support immediates");
  return 0;
}

s32 FPURegCache::SImm32(preg_t preg) const
{
  ASSERT_MSG(DYNA_REC, false, "FPURegCache doesn't support immediates");
  return 0;
}

OpArg FPURegCache::R(preg_t preg) const
{
  if (m_state.m_guests_in_host_register[preg])
  {
    return ::Gen::R(m_state.m_guests_host_register[preg]);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, m_state.m_guests_in_ppc_state[preg], "FPR {} missing!", preg);
    return GetPPCStateLocation(preg);
  }
}

void FPURegCache::StoreRegister(preg_t preg, const OpArg& new_loc,
                                IgnoreDiscardedRegisters ignore_discarded_registers)
{
  if (m_state.m_guests_in_host_register[preg])
  {
    m_emitter.MOVAPD(new_loc, m_state.m_guests_host_register[preg]);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, ignore_discarded_registers != IgnoreDiscardedRegisters::No,
               "FPR {} not in host register", preg);
  }
}

void FPURegCache::LoadRegister(preg_t preg, X64Reg new_loc)
{
  ASSERT_MSG(DYNA_REC, m_state.m_guests_in_ppc_state[preg], "FPR {} not in PPCState", preg);
  m_emitter.MOVAPD(new_loc, GetPPCStateLocation(preg));
}

void FPURegCache::DiscardImm(preg_t preg)
{
  // FPURegCache doesn't support immediates, so no need to do anything
}

static constexpr std::array<X64Reg, 14> ALLOCATION_ORDER = {
    XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, XMM2, XMM3, XMM4, XMM5};

std::span<const X64Reg> FPURegCache::GetAllocationOrder() const
{
  return ALLOCATION_ORDER;
}

BitSetHost FPURegCache::GetAllocatableRegisters() const
{
  // Force loop unrolling
  return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    return BitSetHost(((1 << ALLOCATION_ORDER[Is]) | ...));
  }(std::make_index_sequence<ALLOCATION_ORDER.size()>{});
}

OpArg FPURegCache::GetPPCStateLocation(preg_t preg) const
{
  return PPCSTATE_PS0(preg);
}

BitSetGuest FPURegCache::GetRegUtilization() const
{
  return m_jit.js.op->fprInXmm;
}

BitSetGuest FPURegCache::CountRegsIn(preg_t preg, u32 lookahead) const
{
  BitSetGuest regs_used;

  for (u32 i = 1; i < lookahead; i++)
  {
    BitSetGuest regs_in = m_jit.js.op[i].fregsIn;
    regs_used |= regs_in;
    if (regs_in[preg])
      return regs_used;
  }

  return regs_used;
}
