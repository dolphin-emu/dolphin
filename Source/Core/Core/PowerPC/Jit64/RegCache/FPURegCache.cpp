// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64/RegCache/FPURegCache.h"

#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64Base.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"

using namespace Gen;

FPURegCache::FPURegCache(Jit64& jit) : RegCache{jit}
{
}

void FPURegCache::StoreRegister(preg_t preg, const OpArg& new_loc)
{
  OpArg new_loc_upper = new_loc;
  new_loc_upper.AddMemOffset(sizeof(double));

  const RCRepr repr = m_regs[preg].GetRepr();
  switch (repr)
  {
  case RCRepr::Canonical:
  case RCRepr::PairRounded:
  case RCRepr::DupPhysical:
  case RCRepr::DupPhysicalRounded:
    m_emitter->MOVAPD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(repr);
    break;
  case RCRepr::PairSingles:
    m_emitter->CVTPS2PD(m_regs[preg].Location().GetSimpleReg(), m_regs[preg].Location());
    m_emitter->MOVAPD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(RCRepr::PairRounded);
    break;
  case RCRepr::DupPhysicalSingles:
    m_emitter->CVTPS2PD(m_regs[preg].Location().GetSimpleReg(), m_regs[preg].Location());
    m_emitter->MOVAPD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(RCRepr::DupPhysicalRounded);
    break;
  case RCRepr::DupSingles:
    m_emitter->CVTSS2SD(m_regs[preg].Location().GetSimpleReg(), m_regs[preg].Location());
    m_emitter->MOVSD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_emitter->MOVSD(new_loc_upper, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(RCRepr::DupPhysicalRounded);
  case RCRepr::Dup:
    m_emitter->MOVSD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_emitter->MOVSD(new_loc_upper, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(RCRepr::DupPhysical);
    break;
  case RCRepr::DupRounded:
    m_emitter->MOVSD(new_loc, m_regs[preg].Location().GetSimpleReg());
    m_emitter->MOVSD(new_loc_upper, m_regs[preg].Location().GetSimpleReg());
    m_regs[preg].SetRepr(RCRepr::DupPhysicalRounded);
    break;
  }
}

void FPURegCache::LoadRegister(preg_t preg, X64Reg new_loc)
{
  ASSERT(IsRCReprCanonicalCompatible(m_regs[preg].GetRepr()));
  m_emitter->MOVAPD(new_loc, m_regs[preg].Location());
}

RCRepr FPURegCache::Convert(Gen::X64Reg reg, RCRepr old_repr, RCRepr requested_repr)
{
  switch (requested_repr)
  {
  case RCRepr::Canonical:
    switch (old_repr)
    {
    case RCRepr::Canonical:
    case RCRepr::PairRounded:
    case RCRepr::DupPhysical:
    case RCRepr::DupPhysicalRounded:
      // No conversion required.
      return old_repr;
    case RCRepr::Dup:
      m_emitter->MOVDDUP(reg, ::Gen::R(reg));
      return RCRepr::DupPhysical;
    case RCRepr::DupRounded:
      m_emitter->MOVDDUP(reg, ::Gen::R(reg));
      return RCRepr::DupPhysicalRounded;
    case RCRepr::PairSingles:
      m_emitter->CVTPS2PD(reg, ::Gen::R(reg));
      return RCRepr::PairRounded;
    case RCRepr::DupPhysicalSingles:
      m_emitter->CVTPS2PD(reg, ::Gen::R(reg));
      return RCRepr::DupPhysicalRounded;
    case RCRepr::DupSingles:
      m_emitter->CVTSS2SD(reg, ::Gen::R(reg));
      m_emitter->MOVDDUP(reg, ::Gen::R(reg));
      return RCRepr::DupPhysicalRounded;
    }
    break;
  case RCRepr::Dup:
    switch (old_repr)
    {
    case RCRepr::Canonical:
    case RCRepr::PairRounded:
    case RCRepr::Dup:
    case RCRepr::DupRounded:
    case RCRepr::DupPhysical:
    case RCRepr::DupPhysicalRounded:
      // No conversion required.
      return old_repr;
    case RCRepr::PairSingles:
      m_emitter->CVTPS2PD(reg, ::Gen::R(reg));
      return RCRepr::PairRounded;
    case RCRepr::DupPhysicalSingles:
      m_emitter->CVTPS2PD(reg, ::Gen::R(reg));
      return RCRepr::DupPhysicalRounded;
    case RCRepr::DupSingles:
      m_emitter->CVTSS2SD(reg, ::Gen::R(reg));
      return RCRepr::DupRounded;
    }
    break;
  case RCRepr::PairSingles:
    switch (old_repr)
    {
    case RCRepr::Canonical:
    case RCRepr::PairRounded:
    case RCRepr::Dup:
    case RCRepr::DupRounded:
    case RCRepr::DupPhysical:
    case RCRepr::DupPhysicalRounded:
      ASSERT_MSG(DYNA_REC, false, "Lossy conversion not allowed");
      break;
    case RCRepr::PairSingles:
    case RCRepr::DupPhysicalSingles:
      // No conversion required
      return old_repr;
    case RCRepr::DupSingles:
      m_emitter->MOVSLDUP(reg, ::Gen::R(reg));
      return RCRepr::DupPhysicalSingles;
    }
  case RCRepr::DupSingles:
    switch (old_repr)
    {
    case RCRepr::Canonical:
    case RCRepr::PairRounded:
    case RCRepr::Dup:
    case RCRepr::DupRounded:
    case RCRepr::DupPhysical:
    case RCRepr::DupPhysicalRounded:
      ASSERT_MSG(DYNA_REC, false, "Lossy conversion not allowed");
      break;
    case RCRepr::PairSingles:
    case RCRepr::DupSingles:
    case RCRepr::DupPhysicalSingles:
      // No conversion required
      return old_repr;
    }
  case RCRepr::PairRounded:
  case RCRepr::DupRounded:
  case RCRepr::DupPhysical:
  case RCRepr::DupPhysicalSingles:
  case RCRepr::DupPhysicalRounded:
    ASSERT_MSG(DYNA_REC, false, "Cannot request direct conversion to these representations");
    break;
  }
  ASSERT_MSG(DYNA_REC, false, "Unreachable");
  return old_repr;
}

void FPURegCache::ConvertRegister(preg_t preg, RCRepr requested_repr)
{
  const Gen::X64Reg xr = m_regs[preg].Location().GetSimpleReg();
  const RCRepr new_repr = Convert(xr, m_regs[preg].GetRepr(), requested_repr);
  m_regs[preg].SetRepr(new_repr);
}

const X64Reg* FPURegCache::GetAllocationOrder(size_t* count) const
{
  static const X64Reg allocation_order[] = {XMM6,  XMM7,  XMM8,  XMM9, XMM10, XMM11, XMM12,
                                            XMM13, XMM14, XMM15, XMM2, XMM3,  XMM4,  XMM5};
  *count = sizeof(allocation_order) / sizeof(X64Reg);
  return allocation_order;
}

OpArg FPURegCache::GetDefaultLocation(preg_t preg) const
{
  return PPCSTATE(ps[preg][0]);
}

BitSet32 FPURegCache::GetRegUtilization() const
{
  return m_jit.js.op->gprInReg;
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
