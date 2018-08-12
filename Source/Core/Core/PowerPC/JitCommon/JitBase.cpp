// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitCommon/JitBase.h"

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

JitBase* g_jit;

const u8* JitBase::Dispatch(JitBase& jit)
{
  return jit.GetBlockCache()->Dispatch();
}

void JitTrampoline(JitBase& jit, u32 em_address)
{
  jit.Jit(em_address);
}

u32 Helper_Mask(u8 mb, u8 me)
{
  // first make 001111111111111 part
  u32 begin = 0xFFFFFFFF >> mb;
  // then make 000000000001111 part, which is used to flip the bits of the first one
  u32 end = 0x7FFFFFFF >> me;
  // do the bitflip
  u32 mask = begin ^ end;
  // and invert if backwards
  if (me < mb)
    return ~mask;
  else
    return mask;
}

JitBase::JitBase() : m_code_buffer(code_buffer_size)
{
}

JitBase::~JitBase() = default;

bool JitBase::CanMergeNextInstructions(int count) const
{
  if (CPU::IsStepping() || js.instructionsLeft < count)
    return false;
  // Be careful: a breakpoint kills flags in between instructions
  for (int i = 1; i <= count; i++)
  {
    if (SConfig::GetInstance().bEnableDebugging &&
        PowerPC::breakpoints.IsAddressBreakPoint(js.op[i].address))
      return false;
    if (js.op[i].isBranchTarget)
      return false;
  }
  return true;
}

void JitBase::UpdateMemoryOptions()
{
  bool any_watchpoints = PowerPC::memchecks.HasAny();
  jo.fastmem = SConfig::GetInstance().bFastmem && (MSR.DR || !any_watchpoints);
  jo.memcheck = SConfig::GetInstance().bMMU || any_watchpoints;
}
