// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  m_emitter->MOV(32, new_loc, m_regs[preg].Location());
}

void GPRRegCache::LoadRegister(preg_t preg, X64Reg new_loc)
{
  m_emitter->MOV(32, ::Gen::R(new_loc), m_regs[preg].Location());
}

OpArg GPRRegCache::GetDefaultLocation(preg_t preg) const
{
  return PPCSTATE(gpr[preg]);
}

const X64Reg* GPRRegCache::GetAllocationOrder(size_t* count) const
{
  static const X64Reg allocation_order[] = {
// R12, when used as base register, for example in a LEA, can generate bad code! Need to look into
// this.
#ifdef _WIN32
      RSI, RDI, R13, R14, R15, R8,
      R9,  R10, R11, R12, RCX
#else
      R12, R13, R14, R15, RSI, RDI,
      R8,  R9,  R10, R11, RCX
#endif
  };
  *count = sizeof(allocation_order) / sizeof(X64Reg);
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
  return m_jit.js.op->gprInReg;
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
