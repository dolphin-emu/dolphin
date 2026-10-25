// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitPPC64/JitPPC64_RegCache.h"

#include "Common/Assert.h"
#include "Core/PowerPC/JitPPC64/Jit.h"

using namespace PPC64Gen;

namespace
{
constexpr u32 FIRST_GUEST_HOST_REG = 14;
constexpr u32 LAST_GUEST_HOST_REG = 27;
constexpr u32 FIRST_SCRATCH_REG = 5;
constexpr u32 LAST_SCRATCH_REG = 11;

constexpr u32 FIRST_GUEST_HOST_VSR = 52;
constexpr u32 LAST_GUEST_HOST_VSR = 63;
constexpr u32 FIRST_SCRATCH_VSR = 32;
constexpr u32 LAST_SCRATCH_VSR = 47;
}  // namespace

void PPC64GPRCache::Init(JitPPC64* jit)
{
  m_jit = jit;
  Start();
}

void PPC64GPRCache::Start()
{
  m_guest = {};
  m_host_in_use = {};
  m_instruction = 0;
}

void PPC64GPRCache::Load(GPR host, u32 preg)
{
  if (preg < CR_INDEX)
    m_jit->LWZ(host, PPC_REG, PPCSTATE_OFF_GPR(preg));
  else
    m_jit->LD(host, PPC_REG, PPCSTATE_OFF_CR(preg - CR_INDEX));
}

void PPC64GPRCache::Store(GPR host, u32 preg)
{
  if (preg < CR_INDEX)
    m_jit->STW(host, PPC_REG, PPCSTATE_OFF_GPR(preg));
  else
    m_jit->STD(host, PPC_REG, PPCSTATE_OFF_CR(preg - CR_INDEX));
}

GPR PPC64GPRCache::AllocateGuestHostReg()
{
  for (u32 i = FIRST_GUEST_HOST_REG; i <= LAST_GUEST_HOST_REG; i++)
  {
    if (!m_host_in_use[i])
      return static_cast<GPR>(i);
  }

  // Evict the guest register that was used least recently, but not one that the current
  // instruction uses.
  u32 victim = static_cast<u32>(m_guest.size());
  for (u32 preg = 0; preg < m_guest.size(); preg++)
  {
    const GuestReg& guest = m_guest[preg];
    if (!guest.bound || guest.last_used == m_instruction)
      continue;
    if (victim == m_guest.size() || guest.last_used < m_guest[victim].last_used)
      victim = preg;
  }

  ASSERT_MSG(DYNA_REC, victim != m_guest.size(), "Out of host registers for guest registers");
  if (victim == m_guest.size())
    return GPR::R0;

  const GPR host = m_guest[victim].host;
  Release(victim);
  return host;
}

GPR PPC64GPRCache::Bind(u32 preg, bool load)
{
  GuestReg& guest = m_guest[preg];
  guest.last_used = m_instruction;
  if (guest.bound)
    return guest.host;

  const GPR host = AllocateGuestHostReg();
  m_host_in_use[DecodeReg(host)] = true;
  guest.bound = true;
  guest.dirty = false;
  guest.host = host;
  if (load)
    Load(host, preg);
  return host;
}

GPR PPC64GPRCache::R(u32 preg)
{
  return Bind(preg, true);
}

GPR PPC64GPRCache::RW(u32 preg, bool load)
{
  const GPR host = Bind(preg, load);
  m_guest[preg].dirty = true;
  return host;
}

void PPC64GPRCache::Release(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (guest.dirty)
    Store(guest.host, preg);
  m_host_in_use[DecodeReg(guest.host)] = false;
  guest = {};
}

void PPC64GPRCache::StoreIfDirty(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (!guest.bound || !guest.dirty)
    return;
  Store(guest.host, preg);
  guest.dirty = false;
}

void PPC64GPRCache::Discard(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (!guest.bound)
    return;
  m_host_in_use[DecodeReg(guest.host)] = false;
  guest = {};
}

void PPC64GPRCache::Flush(FlushMode mode)
{
  for (u32 preg = 0; preg < m_guest.size(); preg++)
  {
    const GuestReg& guest = m_guest[preg];
    if (!guest.bound)
      continue;

    if (mode == FlushMode::All)
      Release(preg);
    else if (guest.dirty)
      Store(guest.host, preg);
  }
}

PPC64GPRCache::ScopedReg PPC64GPRCache::GetScopedReg()
{
  for (u32 i = FIRST_SCRATCH_REG; i <= LAST_SCRATCH_REG; i++)
  {
    if (m_host_in_use[i])
      continue;

    m_host_in_use[i] = true;
    return ScopedReg(this, static_cast<GPR>(i));
  }

  ASSERT_MSG(DYNA_REC, false, "Out of scratch registers");
  return {};
}

BitSet32 PPC64GPRCache::GetScratchRegsInUse() const
{
  BitSet32 scratch{};
  for (u32 i = FIRST_SCRATCH_REG; i <= LAST_SCRATCH_REG; i++)
    scratch[i] = true;
  return m_host_in_use & scratch;
}

void PPC64GPRCache::UnlockReg(GPR reg)
{
  ASSERT(m_host_in_use[DecodeReg(reg)]);
  m_host_in_use[DecodeReg(reg)] = false;
}

void PPC64FPRCache::Init(JitPPC64* jit)
{
  m_jit = jit;
  Start();
}

void PPC64FPRCache::Start()
{
  m_guest = {};
  m_host_in_use = 0;
  m_scratch_in_use = 0;
  m_instruction = 0;
}

void PPC64FPRCache::Load(VSR host, u32 preg)
{
  // R0 isn't allocated, so it's free here.
  m_jit->LI(GPR::R0, PPCSTATE_OFF_PS(preg));
  m_jit->LXVD2X(host, PPC_REG, GPR::R0);
}

void PPC64FPRCache::Store(VSR host, u32 preg)
{
  m_jit->LI(GPR::R0, PPCSTATE_OFF_PS(preg));
  m_jit->STXVD2X(host, PPC_REG, GPR::R0);
}

VSR PPC64FPRCache::AllocateGuestHostReg()
{
  for (u32 i = FIRST_GUEST_HOST_VSR; i <= LAST_GUEST_HOST_VSR; i++)
  {
    if (!(m_host_in_use & (u64{1} << i)))
      return static_cast<VSR>(i);
  }

  // Evict the guest register that was used least recently, but not one that the current
  // instruction uses.
  u32 victim = static_cast<u32>(m_guest.size());
  for (u32 preg = 0; preg < m_guest.size(); preg++)
  {
    const GuestReg& guest = m_guest[preg];
    if (!guest.bound || guest.last_used == m_instruction)
      continue;
    if (victim == m_guest.size() || guest.last_used < m_guest[victim].last_used)
      victim = preg;
  }

  ASSERT_MSG(DYNA_REC, victim != m_guest.size(), "Out of host registers for guest FPRs");
  if (victim == m_guest.size())
    return VSR::VS0;

  const VSR host = m_guest[victim].host;
  Release(victim);
  return host;
}

VSR PPC64FPRCache::Bind(u32 preg, bool load)
{
  GuestReg& guest = m_guest[preg];
  guest.last_used = m_instruction;
  if (guest.bound)
    return guest.host;

  const VSR host = AllocateGuestHostReg();
  m_host_in_use |= u64{1} << DecodeReg(host);
  guest.bound = true;
  guest.dirty = false;
  guest.host = host;
  if (load)
    Load(host, preg);
  return host;
}

VSR PPC64FPRCache::R(u32 preg)
{
  return Bind(preg, true);
}

VSR PPC64FPRCache::RW(u32 preg, bool load)
{
  const VSR host = Bind(preg, load);
  m_guest[preg].dirty = true;
  return host;
}

void PPC64FPRCache::Release(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (guest.dirty)
    Store(guest.host, preg);
  m_host_in_use &= ~(u64{1} << DecodeReg(guest.host));
  guest = {};
}

void PPC64FPRCache::StoreIfDirty(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (!guest.bound || !guest.dirty)
    return;
  Store(guest.host, preg);
  guest.dirty = false;
}

void PPC64FPRCache::EmitStoreValue(u32 preg)
{
  if (m_guest[preg].bound)
    Store(m_guest[preg].host, preg);
}

void PPC64FPRCache::EmitLoadValue(u32 preg)
{
  if (m_guest[preg].bound)
    Load(m_guest[preg].host, preg);
}

void PPC64FPRCache::Discard(u32 preg)
{
  GuestReg& guest = m_guest[preg];
  if (!guest.bound)
    return;
  m_host_in_use &= ~(u64{1} << DecodeReg(guest.host));
  guest = {};
}

void PPC64FPRCache::Flush(FlushMode mode)
{
  for (u32 preg = 0; preg < m_guest.size(); preg++)
  {
    const GuestReg& guest = m_guest[preg];
    if (!guest.bound)
      continue;

    if (mode == FlushMode::All)
      Release(preg);
    else if (guest.dirty)
      Store(guest.host, preg);
  }
}

PPC64FPRCache::ScopedReg PPC64FPRCache::GetScopedReg()
{
  for (u32 i = FIRST_SCRATCH_VSR; i <= LAST_SCRATCH_VSR; i++)
  {
    const u32 bit = 1u << (i - FIRST_SCRATCH_VSR);
    if (m_scratch_in_use & bit)
      continue;

    m_scratch_in_use |= bit;
    return ScopedReg(this, static_cast<VSR>(i));
  }

  ASSERT_MSG(DYNA_REC, false, "Out of scratch VSX registers");
  return {};
}

void PPC64FPRCache::UnlockReg(VSR reg)
{
  const u32 bit = 1u << (DecodeReg(reg) - FIRST_SCRATCH_VSR);
  ASSERT(m_scratch_in_use & bit);
  m_scratch_in_use &= ~bit;
}
