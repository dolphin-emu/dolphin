// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/JitBase.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "disasm.h"

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
