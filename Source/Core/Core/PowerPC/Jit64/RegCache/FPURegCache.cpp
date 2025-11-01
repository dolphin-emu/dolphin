// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64/RegCache/FPURegCache.h"

#include "Common/x64Reg.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

FPURegCache::FPURegCache(Jit64& jit) : RegCache{jit}
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
  if (m_regs[preg].IsInHostRegister())
  {
    return ::Gen::R(m_regs[preg].GetHostRegister());
  }
  else
  {
    ASSERT_MSG(DYNA_REC, m_regs[preg].IsInDefaultLocation(), "FPR {} missing!", preg);
    return m_regs[preg].GetDefaultLocation();
  }
}

void FPURegCache::StoreRegister(preg_t preg, const OpArg& new_loc,
                                IgnoreDiscardedRegisters ignore_discarded_registers)
{
  if (m_regs[preg].IsInHostRegister())
  {
    m_emitter->MOVAPD(new_loc, m_regs[preg].GetHostRegister());
  }
  else
  {
    ASSERT_MSG(DYNA_REC, ignore_discarded_registers != IgnoreDiscardedRegisters::No,
               "FPR {} not in host register", preg);
  }
}

void FPURegCache::LoadRegister(preg_t preg, X64Reg new_loc)
{
  ASSERT_MSG(DYNA_REC, m_regs[preg].IsInDefaultLocation(), "FPR {} not in default location", preg);
  m_emitter->MOVAPD(new_loc, m_regs[preg].GetDefaultLocation());
}

void FPURegCache::DiscardImm(preg_t preg)
{
  // FPURegCache doesn't support immediates, so no need to do anything
}

std::span<const X64Reg> FPURegCache::GetAllocationOrder() const
{
  static constexpr X64Reg allocation_order[] = {XMM6,  XMM7,  XMM8,  XMM9, XMM10, XMM11, XMM12,
                                                XMM13, XMM14, XMM15, XMM2, XMM3,  XMM4,  XMM5};
  return allocation_order;
}

OpArg FPURegCache::GetDefaultLocation(preg_t preg) const
{
  return PPCSTATE_PS0(preg);
}

BitSet32 FPURegCache::GetRegUtilization() const
{
  return m_jit.js.op->fprInXmm;
}

BitSet32 FPURegCache::CountRegsIn(preg_t preg, u32 lookahead) const
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
