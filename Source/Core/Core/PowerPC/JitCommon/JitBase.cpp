// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PowerPC.h"

JitBase* jit;

void Jit(u32 em_address)
{
  jit->Jit(em_address);
}

bool JitBase::MergeAllowedNextInstructions(int count)
{
  if (CPU::GetState() == CPU::CPU_STEPPING || js.instructionsLeft < count)
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
  jo.fastmem = SConfig::GetInstance().bFastmem && !any_watchpoints;
  jo.memcheck = SConfig::GetInstance().bMMU || any_watchpoints;
  jo.alwaysUseMemFuncs = any_watchpoints;
}
