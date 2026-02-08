// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/RegCache/GPRRegCache.h"

#include "Common/x64Reg.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

GPRRegCache::GPRRegCache(Jit64& jit) : RegCache{jit}
{
}

void GPRRegCache::StoreRegister(preg_t preg, const OpArg& new_loc)
{
  ASSERT_MSG(DYNA_REC, !m_regs[preg].IsDiscarded(), "Discarded register - {}", preg);
  m_emitter->MOV(32, new_loc, m_regs[preg].Location().value());
}

void GPRRegCache::LoadRegister(preg_t preg, X64Reg new_loc)
{
  ASSERT_MSG(DYNA_REC, !m_regs[preg].IsDiscarded(), "Discarded register - {}", preg);
  m_emitter->MOV(32, ::Gen::R(new_loc), m_regs[preg].Location().value());
}

OpArg GPRRegCache::GetDefaultLocation(preg_t preg) const
{
  return PPCSTATE_GPR(preg);
}

std::span<const X64Reg> GPRRegCache::GetAllocationOrder() const
{
  static constexpr X64Reg allocation_order[] = {
#ifdef _WIN32
      RSI, RDI, R13, R14, R15, R8,
      R9,  R10, R11, R12, RCX
#else
      R12, R13, R14, R15, RSI, RDI,
      R8,  R9,  R10, R11, RCX
#endif
  };
  return allocation_order;
}

void GPRRegCache::SetImmediate32(preg_t preg, u32 imm_value, bool dirty)
{
  // "dirty" can be false to avoid redundantly flushing an immediate when
  // processing speculative constants.
  DiscardRegContentsIfCached(preg);
  m_regs[preg].SetToImm32(imm_value, dirty);
}

BitSet32 GPRRegCache::GetRegUtilization() const
{
  return m_jit.js.op->gprInUse;
}

BitSet32 GPRRegCache::CountRegsIn(preg_t preg, u32 lookahead) const
{
  BitSet32 regs_used;

  for (u32 i = 1; i < lookahead; i++)
  {
    BitSet32 regs_in = m_jit.js.op[i].regsIn;
    regs_used |= regs_in;
    if (regs_in[preg])
      return regs_used;
  }

  return regs_used;
}
