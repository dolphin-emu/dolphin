// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/JitCommon/JitBase.h"

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

const u8* JitBase::Dispatch(JitBase& jit)
{
  return jit.GetBlockCache()->Dispatch();
}

void JitTrampoline(JitBase& jit, u32 em_address)
{
  jit.Jit(em_address);
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

void JitBase::UpdateMemoryAndExceptionOptions()
{
  bool any_watchpoints = PowerPC::memchecks.HasAny();
  jo.fastmem = SConfig::GetInstance().bFastmem && jo.fastmem_arena && (MSR.DR || !any_watchpoints);
  jo.memcheck = SConfig::GetInstance().bMMU || any_watchpoints;
  jo.fp_exceptions = SConfig::GetInstance().bFloatExceptions;
  jo.div_by_zero_exceptions = SConfig::GetInstance().bDivideByZeroExceptions;
}

bool JitBase::ShouldHandleFPExceptionForInstruction(const PPCAnalyst::CodeOp* op)
{
  if (jo.fp_exceptions)
    return (op->opinfo->flags & FL_FLOAT_EXCEPTION) != 0;
  else if (jo.div_by_zero_exceptions)
    return (op->opinfo->flags & FL_FLOAT_DIV) != 0;
  else
    return false;
}
