// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/DSP/Jit/x64/DSPJitRegCache.h"

#include <cstddef>

#include "Common/Assert.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"

#include "Core/DSP/DSPCore.h"
#include "Core/DSP/Jit/x64/DSPEmitter.h"

using namespace Gen;

namespace DSP::JIT::x64
{
// Ordered in order of prefered use.
// Not all of these are actually available
constexpr std::array<X64Reg, 15> s_allocation_order = {
    {R8, R9, R10, R11, R12, R13, R14, R15, RSI, RDI, RBX, RCX, RDX, RAX, RBP}};

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
static Gen::OpArg GetRegisterPointer(size_t reg)
{
  switch (reg)
  {
  case DSP_REG_AR0:
  case DSP_REG_AR1:
  case DSP_REG_AR2:
  case DSP_REG_AR3:
    return MDisp(
        R15, static_cast<int>(offsetof(SDSP, r.ar) + sizeof(SDSP::r.ar[0]) * (reg - DSP_REG_AR0)));
  case DSP_REG_IX0:
  case DSP_REG_IX1:
  case DSP_REG_IX2:
  case DSP_REG_IX3:
    return MDisp(
        R15, static_cast<int>(offsetof(SDSP, r.ix) + sizeof(SDSP::r.ix[0]) * (reg - DSP_REG_IX0)));
  case DSP_REG_WR0:
  case DSP_REG_WR1:
  case DSP_REG_WR2:
  case DSP_REG_WR3:
    return MDisp(
        R15, static_cast<int>(offsetof(SDSP, r.wr) + sizeof(SDSP::r.wr[0]) * (reg - DSP_REG_WR0)));
  case DSP_REG_ST0:
  case DSP_REG_ST1:
  case DSP_REG_ST2:
  case DSP_REG_ST3:
    return MDisp(
        R15, static_cast<int>(offsetof(SDSP, r.st) + sizeof(SDSP::r.st[0]) * (reg - DSP_REG_ST0)));
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ac[0].h) +
                                       sizeof(SDSP::r.ac[0]) * (reg - DSP_REG_ACH0)));
  case DSP_REG_CR:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.cr)));
  case DSP_REG_SR:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.sr)));
  case DSP_REG_PRODL:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.prod.l)));
  case DSP_REG_PRODM:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.prod.m)));
  case DSP_REG_PRODH:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.prod.h)));
  case DSP_REG_PRODM2:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.prod.m2)));
  case DSP_REG_AXL0:
  case DSP_REG_AXL1:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ax[0].l) +
                                       sizeof(SDSP::r.ax[0]) * (reg - DSP_REG_AXL0)));
  case DSP_REG_AXH0:
  case DSP_REG_AXH1:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ax[0].h) +
                                       sizeof(SDSP::r.ax[0]) * (reg - DSP_REG_AXH0)));
  case DSP_REG_ACL0:
  case DSP_REG_ACL1:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ac[0].l) +
                                       sizeof(SDSP::r.ac[0]) * (reg - DSP_REG_ACL0)));
  case DSP_REG_ACM0:
  case DSP_REG_ACM1:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ac[0].m) +
                                       sizeof(SDSP::r.ac[0]) * (reg - DSP_REG_ACM0)));
  case DSP_REG_AX0_32:
  case DSP_REG_AX1_32:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ax[0].val) +
                                       sizeof(SDSP::r.ax[0]) * (reg - DSP_REG_AX0_32)));
  case DSP_REG_ACC0_64:
  case DSP_REG_ACC1_64:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.ac[0].val) +
                                       sizeof(SDSP::r.ac[0]) * (reg - DSP_REG_ACC0_64)));
  case DSP_REG_PROD_64:
    return MDisp(R15, static_cast<int>(offsetof(SDSP, r.prod.val)));
  default:
    ASSERT_MSG(DSPLLE, 0, "cannot happen");
    return M(static_cast<void*>(nullptr));
  }
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#define STATIC_REG_ACCS
//#undef STATIC_REG_ACCS

DSPJitRegCache::DSPJitRegCache(DSPEmitter& emitter) : m_emitter(emitter), m_is_temporary(false)
{
  for (X64CachedReg& xreg : m_xregs)
  {
    xreg.guest_reg = DSP_REG_STATIC;
    xreg.pushed = false;
  }

  m_xregs[RAX].guest_reg = DSP_REG_STATIC;  // reserved for MUL/DIV
  m_xregs[RDX].guest_reg = DSP_REG_STATIC;  // reserved for MUL/DIV
  m_xregs[RCX].guest_reg = DSP_REG_STATIC;  // reserved for shifts

  m_xregs[RBX].guest_reg = DSP_REG_STATIC;  // extended op backing store

  m_xregs[RSP].guest_reg = DSP_REG_STATIC;  // stack pointer

  m_xregs[RBP].guest_reg = DSP_REG_NONE;  // definitely usable in dsplle because
                                          // all external calls are protected

  m_xregs[RSI].guest_reg = DSP_REG_NONE;
  m_xregs[RDI].guest_reg = DSP_REG_NONE;

#ifdef STATIC_REG_ACCS
  m_xregs[R8].guest_reg = DSP_REG_STATIC;  // acc0
  m_xregs[R9].guest_reg = DSP_REG_STATIC;  // acc1
#else
  m_xregs[R8].guest_reg = DSP_REG_NONE;
  m_xregs[R9].guest_reg = DSP_REG_NONE;
#endif
  m_xregs[R10].guest_reg = DSP_REG_NONE;
  m_xregs[R11].guest_reg = DSP_REG_NONE;
  m_xregs[R12].guest_reg = DSP_REG_NONE;
  m_xregs[R13].guest_reg = DSP_REG_NONE;
  m_xregs[R14].guest_reg = DSP_REG_NONE;
  m_xregs[R15].guest_reg = DSP_REG_STATIC;  // reserved for SDSP pointer

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    m_regs[i].mem = GetRegisterPointer(i);
    m_regs[i].size = 0;
    m_regs[i].dirty = false;
    m_regs[i].used = false;
    m_regs[i].last_use_ctr = -1;
    m_regs[i].parentReg = DSP_REG_NONE;
    m_regs[i].shift = 0;
    m_regs[i].host_reg = INVALID_REG;
    m_regs[i].loc = m_regs[i].mem;
  }

  for (unsigned int i = 0; i < 32; i++)
  {
    m_regs[i].size = 2;
  }
// special composite registers
#ifdef STATIC_REG_ACCS
  m_regs[DSP_REG_ACC0_64].host_reg = R8;
  m_regs[DSP_REG_ACC1_64].host_reg = R9;
#endif
  for (unsigned int i = 0; i < 2; i++)
  {
    m_regs[i + DSP_REG_ACC0_64].size = 8;
    m_regs[i + DSP_REG_ACL0].parentReg = i + DSP_REG_ACC0_64;
    m_regs[i + DSP_REG_ACM0].parentReg = i + DSP_REG_ACC0_64;
    m_regs[i + DSP_REG_ACH0].parentReg = i + DSP_REG_ACC0_64;
    m_regs[i + DSP_REG_ACL0].shift = 0;
    m_regs[i + DSP_REG_ACM0].shift = 16;
    m_regs[i + DSP_REG_ACH0].shift = 32;
  }

  m_regs[DSP_REG_PROD_64].size = 8;
  m_regs[DSP_REG_PRODL].parentReg = DSP_REG_PROD_64;
  m_regs[DSP_REG_PRODM].parentReg = DSP_REG_PROD_64;
  m_regs[DSP_REG_PRODH].parentReg = DSP_REG_PROD_64;
  m_regs[DSP_REG_PRODM2].parentReg = DSP_REG_PROD_64;
  m_regs[DSP_REG_PRODL].shift = 0;
  m_regs[DSP_REG_PRODM].shift = 16;
  m_regs[DSP_REG_PRODH].shift = 32;
  m_regs[DSP_REG_PRODM2].shift = 48;

  for (unsigned int i = 0; i < 2; i++)
  {
    m_regs[i + DSP_REG_AX0_32].size = 4;
    m_regs[i + DSP_REG_AXL0].parentReg = i + DSP_REG_AX0_32;
    m_regs[i + DSP_REG_AXH0].parentReg = i + DSP_REG_AX0_32;
    m_regs[i + DSP_REG_AXL0].shift = 0;
    m_regs[i + DSP_REG_AXH0].shift = 16;
  }
}

DSPJitRegCache::DSPJitRegCache(const DSPJitRegCache& cache)
    : m_regs(cache.m_regs), m_xregs(cache.m_xregs), m_emitter(cache.m_emitter), m_is_temporary(true)
{
}

DSPJitRegCache& DSPJitRegCache::operator=(const DSPJitRegCache& cache)
{
  ASSERT_MSG(DSPLLE, &m_emitter == &cache.m_emitter, "emitter does not match");
  ASSERT_MSG(DSPLLE, m_is_temporary, "register cache not temporary??");
  m_is_merged = false;

  m_xregs = cache.m_xregs;
  m_regs = cache.m_regs;

  return *this;
}

DSPJitRegCache::~DSPJitRegCache()
{
  ASSERT_MSG(DSPLLE, !m_is_temporary || m_is_merged, "temporary cache not merged");
}

void DSPJitRegCache::Drop()
{
  m_is_merged = true;
}

void DSPJitRegCache::FlushRegs(DSPJitRegCache& cache, bool emit)
{
  cache.m_is_merged = true;

  // drop all guest register not used by cache
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    m_regs[i].used = false;  // used is restored later
    if (m_regs[i].loc.IsSimpleReg() && !cache.m_regs[i].loc.IsSimpleReg())
    {
      MovToMemory(i);
    }
  }

  // try to move guest regs in the wrong host reg to the correct one
  int movcnt;
  do
  {
    movcnt = 0;
    for (size_t i = 0; i < m_regs.size(); i++)
    {
      X64Reg simple = m_regs[i].loc.GetSimpleReg();
      X64Reg simple_cache = cache.m_regs[i].loc.GetSimpleReg();

      if (simple_cache != simple && m_xregs[simple_cache].guest_reg == DSP_REG_NONE)
      {
        MovToHostReg(i, simple_cache, true);
        movcnt++;
      }
    }
  } while (movcnt != 0);

  // free all host regs that are not used for the same guest reg
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    const auto& reg = m_regs[i];
    const auto& cached_reg = cache.m_regs[i];

    if (cached_reg.loc.GetSimpleReg() != reg.loc.GetSimpleReg() && reg.loc.IsSimpleReg())
    {
      MovToMemory(i);
    }
  }

  // load all guest regs that are in memory and should be in host reg
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (cache.m_regs[i].loc.IsSimpleReg())
    {
      MovToHostReg(i, cache.m_regs[i].loc.GetSimpleReg(), true);
      RotateHostReg(i, cache.m_regs[i].shift, true);
    }
    else if (cache.m_regs[i].loc.IsImm())
    {
      // TODO: Immediates?
    }

    m_regs[i].used = cache.m_regs[i].used;
    m_regs[i].dirty |= cache.m_regs[i].dirty;
    m_regs[i].last_use_ctr = cache.m_regs[i].last_use_ctr;
  }

  // sync the freely used xregs
  if (!emit)
  {
    for (size_t i = 0; i < m_xregs.size(); i++)
    {
      if (cache.m_xregs[i].guest_reg == DSP_REG_USED && m_xregs[i].guest_reg == DSP_REG_NONE)
      {
        m_xregs[i].guest_reg = DSP_REG_USED;
      }
      if (cache.m_xregs[i].guest_reg == DSP_REG_NONE && m_xregs[i].guest_reg == DSP_REG_USED)
      {
        m_xregs[i].guest_reg = DSP_REG_NONE;
      }
    }
  }

  // consistency checks
  for (size_t i = 0; i < m_xregs.size(); i++)
  {
    ASSERT_MSG(DSPLLE, m_xregs[i].guest_reg == cache.m_xregs[i].guest_reg,
               "cache and current xreg guest_reg mismatch for {}", i);
  }

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    ASSERT_MSG(DSPLLE, m_regs[i].loc.IsImm() == cache.m_regs[i].loc.IsImm(),
               "cache and current reg loc mismatch for {}", i);
    ASSERT_MSG(DSPLLE, m_regs[i].loc.GetSimpleReg() == cache.m_regs[i].loc.GetSimpleReg(),
               "cache and current reg loc mismatch for {}", i);
    ASSERT_MSG(DSPLLE, m_regs[i].dirty || !cache.m_regs[i].dirty,
               "cache and current reg dirty mismatch for {}", i);
    ASSERT_MSG(DSPLLE, m_regs[i].used == cache.m_regs[i].used,
               "cache and current reg used mismatch for {}", i);
    ASSERT_MSG(DSPLLE, m_regs[i].shift == cache.m_regs[i].shift,
               "cache and current reg shift mismatch for {}", i);
  }

  m_use_ctr = cache.m_use_ctr;
}

void DSPJitRegCache::FlushMemBackedRegs()
{
  // also needs to undo any dynamic changes to static allocated regs
  // this should have the same effect as
  // merge(DSPJitRegCache(emitter));

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    ASSERT_MSG(DSPLLE, !m_regs[i].used, "register {} still in use", static_cast<u32>(i));

    if (m_regs[i].used)
    {
      m_emitter.INT3();
    }

    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToHostReg(i, m_regs[i].host_reg, true);
      RotateHostReg(i, 0, true);
    }
    else if (m_regs[i].parentReg == DSP_REG_NONE)
    {
      MovToMemory(i);
    }
  }
}

void DSPJitRegCache::FlushRegs()
{
  FlushMemBackedRegs();

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToMemory(i);
    }

    ASSERT_MSG(DSPLLE, !m_regs[i].loc.IsSimpleReg(), "register {} is still a simple reg", i);
  }

  ASSERT_MSG(DSPLLE, m_xregs[RSP].guest_reg == DSP_REG_STATIC, "wrong xreg state for {}",
             Common::ToUnderlying(RSP));
  ASSERT_MSG(DSPLLE, m_xregs[RBX].guest_reg == DSP_REG_STATIC, "wrong xreg state for {}",
             Common::ToUnderlying(RBX));
  ASSERT_MSG(DSPLLE, m_xregs[RBP].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(RBP));
  ASSERT_MSG(DSPLLE, m_xregs[RSI].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(RSI));
  ASSERT_MSG(DSPLLE, m_xregs[RDI].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(RDI));
#ifdef STATIC_REG_ACCS
  ASSERT_MSG(DSPLLE, m_xregs[R8].guest_reg == DSP_REG_STATIC, "wrong xreg state for {}",
             Common::ToUnderlying(R8));
  ASSERT_MSG(DSPLLE, m_xregs[R9].guest_reg == DSP_REG_STATIC, "wrong xreg state for {}",
             Common::ToUnderlying(R9));
#else
  ASSERT_MSG(DSPLLE, m_xregs[R8].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R8));
  ASSERT_MSG(DSPLLE, m_xregs[R9].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R9));
#endif
  ASSERT_MSG(DSPLLE, m_xregs[R10].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R10));
  ASSERT_MSG(DSPLLE, m_xregs[R11].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R11));
  ASSERT_MSG(DSPLLE, m_xregs[R12].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R12));
  ASSERT_MSG(DSPLLE, m_xregs[R13].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R13));
  ASSERT_MSG(DSPLLE, m_xregs[R14].guest_reg == DSP_REG_NONE, "wrong xreg state for {}",
             Common::ToUnderlying(R14));
  ASSERT_MSG(DSPLLE, m_xregs[R15].guest_reg == DSP_REG_STATIC, "wrong xreg state for {}",
             Common::ToUnderlying(R15));

  m_use_ctr = 0;
}

void DSPJitRegCache::LoadRegs(bool emit)
{
  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToHostReg(i, m_regs[i].host_reg, emit);
    }
  }
}

void DSPJitRegCache::SaveRegs()
{
  FlushRegs();

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToMemory(i);
    }

    ASSERT_MSG(DSPLLE, !m_regs[i].loc.IsSimpleReg(), "register {} is still a simple reg", i);
  }
}

void DSPJitRegCache::PushRegs()
{
  FlushMemBackedRegs();

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToMemory(i);
    }

    ASSERT_MSG(DSPLLE, !m_regs[i].loc.IsSimpleReg(), "register {} is still a simple reg", i);
  }

  int push_count = 0;
  for (X64CachedReg& xreg : m_xregs)
  {
    if (xreg.guest_reg == DSP_REG_USED)
      push_count++;
  }

  // hardcoding alignment to 16 bytes
  if (push_count & 1)
  {
    m_emitter.SUB(64, R(RSP), Imm32(8));
  }

  for (size_t i = 0; i < m_xregs.size(); i++)
  {
    if (m_xregs[i].guest_reg == DSP_REG_USED)
    {
      m_emitter.PUSH(static_cast<X64Reg>(i));
      m_xregs[i].pushed = true;
      m_xregs[i].guest_reg = DSP_REG_NONE;
    }

    ASSERT_MSG(DSPLLE,
               m_xregs[i].guest_reg == DSP_REG_NONE || m_xregs[i].guest_reg == DSP_REG_STATIC,
               "register {} is still used", i);
  }
}

void DSPJitRegCache::PopRegs()
{
  int push_count = 0;
  for (int i = static_cast<int>(m_xregs.size() - 1); i >= 0; i--)
  {
    if (m_xregs[i].pushed)
    {
      push_count++;

      m_emitter.POP(static_cast<X64Reg>(i));
      m_xregs[i].pushed = false;
      m_xregs[i].guest_reg = DSP_REG_USED;
    }
  }

  // hardcoding alignment to 16 bytes
  if (push_count & 1)
  {
    m_emitter.ADD(64, R(RSP), Imm32(8));
  }

  for (size_t i = 0; i < m_regs.size(); i++)
  {
    if (m_regs[i].host_reg != INVALID_REG)
    {
      MovToHostReg(i, m_regs[i].host_reg, true);
    }
  }
}

X64Reg DSPJitRegCache::MakeABICallSafe(X64Reg reg)
{
  return reg;
}

void DSPJitRegCache::MovToHostReg(size_t reg, X64Reg host_reg, bool load)
{
  ASSERT_MSG(DSPLLE, reg < m_regs.size(), "bad register name {}", reg);
  ASSERT_MSG(DSPLLE, m_regs[reg].parentReg == DSP_REG_NONE, "register {} is proxy for {}", reg,
             m_regs[reg].parentReg);
  ASSERT_MSG(DSPLLE, !m_regs[reg].used, "moving to host reg in use guest reg {}", reg);
  X64Reg old_reg = m_regs[reg].loc.GetSimpleReg();
  if (old_reg == host_reg)
  {
    return;
  }

  if (m_xregs[host_reg].guest_reg != DSP_REG_STATIC)
  {
    m_xregs[host_reg].guest_reg = reg;
  }

  if (load)
  {
    switch (m_regs[reg].size)
    {
    case 2:
      m_emitter.MOV(16, R(host_reg), m_regs[reg].loc);
      break;
    case 4:
      m_emitter.MOV(32, R(host_reg), m_regs[reg].loc);
      break;
    case 8:
      m_emitter.MOV(64, R(host_reg), m_regs[reg].loc);
      break;
    default:
      ASSERT_MSG(DSPLLE, 0, "unsupported memory size");
      break;
    }
  }

  m_regs[reg].loc = R(host_reg);
  if (old_reg != INVALID_REG && m_xregs[old_reg].guest_reg != DSP_REG_STATIC)
  {
    m_xregs[old_reg].guest_reg = DSP_REG_NONE;
  }
}

void DSPJitRegCache::MovToHostReg(size_t reg, bool load)
{
  ASSERT_MSG(DSPLLE, reg < m_regs.size(), "bad register name {}", reg);
  ASSERT_MSG(DSPLLE, m_regs[reg].parentReg == DSP_REG_NONE, "register {} is proxy for {}", reg,
             m_regs[reg].parentReg);
  ASSERT_MSG(DSPLLE, !m_regs[reg].used, "moving to host reg in use guest reg {}", reg);

  if (m_regs[reg].loc.IsSimpleReg())
  {
    return;
  }

  X64Reg tmp;
  if (m_regs[reg].host_reg != INVALID_REG)
  {
    tmp = m_regs[reg].host_reg;
  }
  else
  {
    tmp = FindSpillFreeXReg();
  }

  if (tmp == INVALID_REG)
  {
    return;
  }

  MovToHostReg(reg, tmp, load);
}

void DSPJitRegCache::RotateHostReg(size_t reg, int shift, bool emit)
{
  ASSERT_MSG(DSPLLE, reg < m_regs.size(), "bad register name {}", reg);
  ASSERT_MSG(DSPLLE, m_regs[reg].parentReg == DSP_REG_NONE, "register {} is proxy for {}", reg,
             m_regs[reg].parentReg);
  ASSERT_MSG(DSPLLE, m_regs[reg].loc.IsSimpleReg(), "register {} is not a simple reg", reg);
  ASSERT_MSG(DSPLLE, !m_regs[reg].used, "rotating in use guest reg {}", reg);

  if (shift > m_regs[reg].shift && emit)
  {
    switch (m_regs[reg].size)
    {
    case 2:
      m_emitter.ROR(16, m_regs[reg].loc, Imm8(shift - m_regs[reg].shift));
      break;
    case 4:
      m_emitter.ROR(32, m_regs[reg].loc, Imm8(shift - m_regs[reg].shift));
      break;
    case 8:
      m_emitter.ROR(64, m_regs[reg].loc, Imm8(shift - m_regs[reg].shift));
      break;
    }
  }
  else if (shift < m_regs[reg].shift && emit)
  {
    switch (m_regs[reg].size)
    {
    case 2:
      m_emitter.ROL(16, m_regs[reg].loc, Imm8(m_regs[reg].shift - shift));
      break;
    case 4:
      m_emitter.ROL(32, m_regs[reg].loc, Imm8(m_regs[reg].shift - shift));
      break;
    case 8:
      m_emitter.ROL(64, m_regs[reg].loc, Imm8(m_regs[reg].shift - shift));
      break;
    }
  }
  m_regs[reg].shift = shift;
}

void DSPJitRegCache::MovToMemory(size_t reg)
{
  ASSERT_MSG(DSPLLE, reg < m_regs.size(), "bad register name {}", reg);
  ASSERT_MSG(DSPLLE, m_regs[reg].parentReg == DSP_REG_NONE, "register {} is proxy for {}", reg,
             m_regs[reg].parentReg);
  ASSERT_MSG(DSPLLE, !m_regs[reg].used, "moving to memory in use guest reg {}", reg);

  if (m_regs[reg].used)
  {
    m_emitter.INT3();
  }

  if (!m_regs[reg].loc.IsSimpleReg() && !m_regs[reg].loc.IsImm())
  {
    return;
  }

  // but first, check for any needed rotations
  if (m_regs[reg].loc.IsSimpleReg())
  {
    RotateHostReg(reg, 0, true);
  }
  else
  {
    // TODO: Immediates?
  }

  ASSERT_MSG(DSPLLE, m_regs[reg].shift == 0, "still shifted??");

  // move to mem
  OpArg tmp = m_regs[reg].mem;

  if (m_regs[reg].dirty)
  {
    switch (m_regs[reg].size)
    {
    case 2:
      m_emitter.MOV(16, tmp, m_regs[reg].loc);
      break;
    case 4:
      m_emitter.MOV(32, tmp, m_regs[reg].loc);
      break;
    case 8:
      m_emitter.MOV(64, tmp, m_regs[reg].loc);
      break;
    default:
      ASSERT_MSG(DSPLLE, 0, "unsupported memory size");
      break;
    }
    m_regs[reg].dirty = false;
  }

  if (m_regs[reg].loc.IsSimpleReg())
  {
    X64Reg hostreg = m_regs[reg].loc.GetSimpleReg();
    if (m_xregs[hostreg].guest_reg != DSP_REG_STATIC)
    {
      m_xregs[hostreg].guest_reg = DSP_REG_NONE;
    }
  }

  m_regs[reg].last_use_ctr = -1;
  m_regs[reg].loc = tmp;
}

OpArg DSPJitRegCache::GetReg(int reg, bool load)
{
  int real_reg;
  int shift;
  if (m_regs[reg].parentReg != DSP_REG_NONE)
  {
    real_reg = m_regs[reg].parentReg;

    // always load and rotate since we need the other
    // parts of the register
    load = true;

    shift = m_regs[reg].shift;
  }
  else
  {
    real_reg = reg;
    shift = 0;
  }

  ASSERT_MSG(DSPLLE, !m_regs[real_reg].used, "register {} already in use", real_reg);

  if (m_regs[real_reg].used)
  {
    m_emitter.INT3();
  }
  // no need to actually emit code for load or rotate if caller doesn't
  // use the contents, but see above for a reason to force the load
  MovToHostReg(real_reg, load);

  // TODO: actually handle INVALID_REG
  ASSERT_MSG(DSPLLE, m_regs[real_reg].loc.IsSimpleReg(), "did not get host reg for {}", reg);

  RotateHostReg(real_reg, shift, load);
  const OpArg oparg = m_regs[real_reg].loc;
  m_regs[real_reg].used = true;

  // do some register specific fixup
  switch (reg)
  {
  case DSP_REG_ACC0_64:
  case DSP_REG_ACC1_64:
    if (load)
    {
      // need to do this because interpreter only does 48 bits
      // (and PutReg does the same)
      m_emitter.SHL(64, oparg, Imm8(64 - 40));  // sign extend
      m_emitter.SAR(64, oparg, Imm8(64 - 40));
    }
    break;
  default:
    break;
  }

  return oparg;
}

void DSPJitRegCache::PutReg(int reg, bool dirty)
{
  int real_reg = reg;
  if (m_regs[reg].parentReg != DSP_REG_NONE)
    real_reg = m_regs[reg].parentReg;

  OpArg oparg = m_regs[real_reg].loc;

  switch (reg)
  {
  case DSP_REG_ACH0:
  case DSP_REG_ACH1:
    if (dirty)
    {
      // no need to extend to full 64bit here until interpreter
      // uses that
      if (oparg.IsSimpleReg())
      {
        // register is already shifted correctly
        // (if at all)

        // sign extend from the bottom 8 bits.
        m_emitter.MOVSX(16, 8, oparg.GetSimpleReg(), oparg);
      }
      else if (oparg.IsImm())
      {
        // TODO: Immediates?
        ASSERT(false);
      }
      else
      {
        // This works on the memory, so use reg instead
        // of real_reg, since it has the right loc
        X64Reg tmp = GetFreeXReg();
        // Sign extend from the bottom 8 bits.
        m_emitter.MOVSX(16, 8, tmp, m_regs[reg].loc);
        m_emitter.MOV(16, m_regs[reg].loc, R(tmp));
        PutXReg(tmp);
      }
    }
    break;
  case DSP_REG_ACC0_64:
  case DSP_REG_ACC1_64:
    if (dirty)
    {
      m_emitter.SHL(64, oparg, Imm8(64 - 40));  // sign extend
      m_emitter.SAR(64, oparg, Imm8(64 - 40));
    }
    break;
  case DSP_REG_CR:
  case DSP_REG_PRODH:
    if (dirty)
    {
      if (oparg.IsSimpleReg())
      {
        // register is already shifted correctly
        // (if at all)

        // Zero extend from the bottom 8 bits.
        m_emitter.MOVZX(16, 8, oparg.GetSimpleReg(), oparg);
      }
      else if (oparg.IsImm())
      {
        // TODO: Immediates?
        ASSERT(false);
      }
      else
      {
        // This works on the memory, so use reg instead
        // of real_reg, since it has the right loc
        X64Reg tmp = GetFreeXReg();
        // Zero extend from the bottom 8 bits.
        m_emitter.MOVZX(16, 8, tmp, m_regs[reg].loc);
        m_emitter.MOV(16, m_regs[reg].loc, R(tmp));
        PutXReg(tmp);
      }
    }
    break;
  case DSP_REG_SR:
    if (dirty)
    {
      if (oparg.IsSimpleReg())
      {
        // register is already shifted correctly
        // (if at all)

        // Clear SR_100, which always reads back as 0
        m_emitter.AND(16, R(oparg.GetSimpleReg()), Gen::Imm16(~SR_100));
      }
      else if (oparg.IsImm())
      {
        // TODO: Immediates?
        ASSERT(false);
      }
      else
      {
        // Clear SR_100, which always reads back as 0
        m_emitter.AND(16, m_regs[reg].loc, Gen::Imm16(~SR_100));
      }
    }
    break;
  default:
    break;
  }

  m_regs[real_reg].used = false;

  if (m_regs[real_reg].loc.IsSimpleReg())
  {
    m_regs[real_reg].dirty |= dirty;
    m_regs[real_reg].last_use_ctr = m_use_ctr;
    m_use_ctr++;
  }
}

void DSPJitRegCache::ReadReg(int sreg, X64Reg host_dreg, RegisterExtension extend)
{
  const OpArg reg = GetReg(sreg);

  switch (m_regs[sreg].size)
  {
  case 2:
    switch (extend)
    {
    case RegisterExtension::Sign:
      m_emitter.MOVSX(64, 16, host_dreg, reg);
      break;
    case RegisterExtension::Zero:
      m_emitter.MOVZX(64, 16, host_dreg, reg);
      break;
    case RegisterExtension::None:
      m_emitter.MOV(16, R(host_dreg), reg);
      break;
    }
    break;
  case 4:
    switch (extend)
    {
    case RegisterExtension::Sign:
      m_emitter.MOVSX(64, 32, host_dreg, reg);
      break;
    case RegisterExtension::Zero:
      m_emitter.MOVZX(64, 32, host_dreg, reg);
      break;
    case RegisterExtension::None:
      m_emitter.MOV(32, R(host_dreg), reg);
      break;
    }
    break;
  case 8:
    m_emitter.MOV(64, R(host_dreg), reg);
    break;
  default:
    ASSERT_MSG(DSPLLE, 0, "unsupported memory size");
    break;
  }
  PutReg(sreg, false);
}

void DSPJitRegCache::WriteReg(int dreg, OpArg arg)
{
  const OpArg reg = GetReg(dreg, false);
  if (arg.IsImm())
  {
    switch (m_regs[dreg].size)
    {
    case 2:
      m_emitter.MOV(16, reg, Imm16(arg.Imm16()));
      break;
    case 4:
      m_emitter.MOV(32, reg, Imm32(arg.Imm32()));
      break;
    case 8:
      m_emitter.MOV(64, reg, Imm64(arg.Imm64()));
      break;
    default:
      ASSERT_MSG(DSPLLE, 0, "unsupported memory size");
      break;
    }
  }
  else
  {
    switch (m_regs[dreg].size)
    {
    case 2:
      m_emitter.MOV(16, reg, arg);
      break;
    case 4:
      m_emitter.MOV(32, reg, arg);
      break;
    case 8:
      m_emitter.MOV(64, reg, arg);
      break;
    default:
      ASSERT_MSG(DSPLLE, 0, "unsupported memory size");
      break;
    }
  }
  PutReg(dreg, true);
}

X64Reg DSPJitRegCache::SpillXReg()
{
  int max_use_ctr_diff = 0;
  X64Reg least_recent_use_reg = INVALID_REG;
  for (X64Reg reg : s_allocation_order)
  {
    if (m_xregs[reg].guest_reg <= DSP_REG_MAX_MEM_BACKED && !m_regs[m_xregs[reg].guest_reg].used)
    {
      int use_ctr_diff = m_use_ctr - m_regs[m_xregs[reg].guest_reg].last_use_ctr;
      if (use_ctr_diff >= max_use_ctr_diff)
      {
        max_use_ctr_diff = use_ctr_diff;
        least_recent_use_reg = reg;
      }
    }
  }

  if (least_recent_use_reg != INVALID_REG)
  {
    MovToMemory(m_xregs[least_recent_use_reg].guest_reg);
    return least_recent_use_reg;
  }

  // just choose one.
  for (X64Reg reg : s_allocation_order)
  {
    if (m_xregs[reg].guest_reg <= DSP_REG_MAX_MEM_BACKED && !m_regs[m_xregs[reg].guest_reg].used)
    {
      MovToMemory(m_xregs[reg].guest_reg);
      return reg;
    }
  }

  return INVALID_REG;
}

void DSPJitRegCache::SpillXReg(X64Reg reg)
{
  if (m_xregs[reg].guest_reg <= DSP_REG_MAX_MEM_BACKED)
  {
    ASSERT_MSG(DSPLLE, !m_regs[m_xregs[reg].guest_reg].used,
               "to be spilled host reg {:#x} (guest reg {:#x}) still in use!",
               Common::ToUnderlying(reg), m_xregs[reg].guest_reg);

    MovToMemory(m_xregs[reg].guest_reg);
  }
  else
  {
    ASSERT_MSG(DSPLLE, m_xregs[reg].guest_reg == DSP_REG_NONE,
               "to be spilled host reg {:#x} still in use!", Common::ToUnderlying(reg));
  }
}

X64Reg DSPJitRegCache::FindFreeXReg() const
{
  for (X64Reg x : s_allocation_order)
  {
    if (m_xregs[x].guest_reg == DSP_REG_NONE)
    {
      return x;
    }
  }

  return INVALID_REG;
}

X64Reg DSPJitRegCache::FindSpillFreeXReg()
{
  X64Reg reg = FindFreeXReg();
  if (reg == INVALID_REG)
  {
    reg = SpillXReg();
  }
  return reg;
}

X64Reg DSPJitRegCache::GetFreeXReg()
{
  X64Reg reg = FindSpillFreeXReg();

  ASSERT_MSG(DSPLLE, reg != INVALID_REG, "could not find register");
  if (reg == INVALID_REG)
  {
    m_emitter.INT3();
  }

  m_xregs[reg].guest_reg = DSP_REG_USED;
  return reg;
}

void DSPJitRegCache::GetXReg(X64Reg reg)
{
  if (m_xregs[reg].guest_reg == DSP_REG_STATIC)
  {
    ERROR_LOG_FMT(DSPLLE, "Trying to get statically used XReg {}", Common::ToUnderlying(reg));
    return;
  }

  if (m_xregs[reg].guest_reg != DSP_REG_NONE)
  {
    SpillXReg(reg);
  }
  ASSERT_MSG(DSPLLE, m_xregs[reg].guest_reg == DSP_REG_NONE, "register already in use");
  m_xregs[reg].guest_reg = DSP_REG_USED;
}

void DSPJitRegCache::PutXReg(X64Reg reg)
{
  if (m_xregs[reg].guest_reg == DSP_REG_STATIC)
  {
    ERROR_LOG_FMT(DSPLLE, "Trying to put statically used XReg {}", Common::ToUnderlying(reg));
    return;
  }

  ASSERT_MSG(DSPLLE, m_xregs[reg].guest_reg == DSP_REG_USED, "PutXReg without get(Free)XReg");

  m_xregs[reg].guest_reg = DSP_REG_NONE;
}

}  // namespace DSP::JIT::x64
