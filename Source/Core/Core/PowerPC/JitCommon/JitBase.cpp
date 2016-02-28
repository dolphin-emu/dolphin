// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <sstream>
#include <string>

#include "disasm.h"

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/CPU.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PowerPC.h"

JitBase* jit;

void Jit(u32 em_address)
{
  jit->Jit(em_address);
}

u32 Helper_Mask(u8 mb, u8 me)
{
  u32 mask = ((u32)-1 >> mb) ^ (me >= 31 ? 0 : (u32)-1 >> (me + 1));
  return mb > me ? ~mask : mask;
}

void LogGeneratedX86(int size, PPCAnalyst::CodeBuffer* code_buffer, const u8* normalEntry,
                     JitBlock* b)
{
  for (int i = 0; i < size; i++)
  {
    const PPCAnalyst::CodeOp& op = code_buffer->codebuffer[i];
    std::string temp = StringFromFormat(
        "%08x %s", op.address, GekkoDisassembler::Disassemble(op.inst.hex, op.address).c_str());
    DEBUG_LOG(DYNA_REC, "IR_X86 PPC: %s\n", temp.c_str());
  }

  disassembler x64disasm;
  x64disasm.set_syntax_intel();

  u64 disasmPtr = (u64)normalEntry;
  const u8* end = normalEntry + b->codeSize;

  while ((u8*)disasmPtr < end)
  {
    char sptr[1000] = "";
    disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, (u8*)disasmPtr, sptr);
    DEBUG_LOG(DYNA_REC, "IR_X86 x86: %s", sptr);
  }

  if (b->codeSize <= 250)
  {
    std::stringstream ss;
    ss << std::hex;
    for (u8 i = 0; i <= b->codeSize; i++)
    {
      ss.width(2);
      ss.fill('0');
      ss << (u32) * (normalEntry + i);
    }
    DEBUG_LOG(DYNA_REC, "IR_X86 bin: %s\n\n\n", ss.str().c_str());
  }
}

void JitBase::HandleInvalidInstruction()
{
  u32 hex = PowerPC::HostRead_U32(PC);
  std::string disasm = GekkoDisassembler::Disassemble(hex, PC);
  NOTICE_LOG(POWERPC, "Last PC = %08x : %s", PC, disasm.c_str());
  Dolphin_Debugger::PrintCallstack();
  NOTICE_LOG(POWERPC, "\nCPU: Invalid instruction %08x at PC = %08x  LR = %08x\n", hex, PC, LR);
  for (int i = 0; i < 32; i += 4)
    NOTICE_LOG(POWERPC, "r%d: 0x%08x r%d: 0x%08x r%d:0x%08x r%d: 0x%08x", i, rGPR[i], i + 1,
               rGPR[i + 1], i + 2, rGPR[i + 2], i + 3, rGPR[i + 3]);
  _assert_msg_(POWERPC, 0, "\nCPU: Invalid instruction %08x\nat PC = %08x  LR = %08x\n%s", hex, PC,
               LR, disasm.c_str());
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
