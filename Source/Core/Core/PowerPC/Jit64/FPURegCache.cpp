// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/FPURegCache.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64Base.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

FPURegCache::FPURegCache(Jit64& jit) : RegCache{jit}
{
}

void FPURegCache::StoreRegister(size_t preg, const OpArg& new_loc)
{
  m_emitter->MOVAPD(new_loc, m_regs[preg].location.GetSimpleReg());
}

void FPURegCache::LoadRegister(size_t preg, X64Reg new_loc)
{
  m_emitter->MOVAPD(new_loc, m_regs[preg].location);
}

const X64Reg* FPURegCache::GetAllocationOrder(size_t* count)
{
  static const X64Reg allocation_order[] = {XMM6,  XMM7,  XMM8,  XMM9, XMM10, XMM11, XMM12,
                                            XMM13, XMM14, XMM15, XMM2, XMM3,  XMM4,  XMM5};
  *count = sizeof(allocation_order) / sizeof(X64Reg);
  return allocation_order;
}

OpArg FPURegCache::GetDefaultLocation(size_t reg) const
{
  return PPCSTATE(ps[reg][0]);
}

BitSet32 FPURegCache::GetRegUtilization()
{
  return m_jit.js.op->gprInReg;
}

BitSet32 FPURegCache::CountRegsIn(size_t preg, u32 lookahead)
{
  BitSet32 regs_used;

  for (u32 i = 1; i < lookahead; i++)
  {
    BitSet32 regs_in = m_jit.js.op[i].fregsIn;
    regs_used |= regs_in;
    if (regs_in[preg])
      return regs_used;
  }

  return regs_used;
}
