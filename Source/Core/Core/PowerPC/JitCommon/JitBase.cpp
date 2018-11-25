// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/JitCommon/JitBase.h"

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

const u8* JitCommonBase::Dispatch(JitCommonBase& jit)
{
  return jit.GetBlockCache()->Dispatch();
}

void JitTrampoline(JitCommonBase& jit, u32 em_address)
{
  jit.Jit(em_address);
}

JitCommonBase::JitCommonBase() : m_code_buffer(code_buffer_size)
{
}

JitCommonBase::~JitCommonBase() = default;

bool JitCommonBase::CanMergeNextInstructions(int count) const
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

void JitCommonBase::UpdateMemoryOptions()
{
  bool any_watchpoints = PowerPC::memchecks.HasAny();
  jo.fastmem = SConfig::GetInstance().bFastmem && (MSR.DR || !any_watchpoints);
  jo.memcheck = SConfig::GetInstance().bMMU || any_watchpoints;
}
