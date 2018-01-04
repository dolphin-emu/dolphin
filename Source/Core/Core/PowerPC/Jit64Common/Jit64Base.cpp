// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/Jit64Base.h"

#include <disasm.h>
#include <sstream>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Common/x64Reg.h"
#include "Core/ARBruteForcer.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/PPCAnalyst.h"

// This generates some fairly heavy trampolines, but it doesn't really hurt.
// Only instructions that access I/O will get these, and there won't be that
// many of them in a typical program/game.
bool Jitx86Base::HandleFault(uintptr_t access_address, SContext* ctx)
{
  // TODO: do we properly handle off-the-end?
  const auto base_ptr = reinterpret_cast<uintptr_t>(Memory::physical_base);
  if (access_address >= base_ptr && access_address < base_ptr + 0x100010000)
    return BackPatch(static_cast<u32>(access_address - base_ptr), ctx);

  const auto logical_base_ptr = reinterpret_cast<uintptr_t>(Memory::logical_base);
  if (access_address >= logical_base_ptr && access_address < logical_base_ptr + 0x100010000)
    return BackPatch(static_cast<u32>(access_address - logical_base_ptr), ctx);

  return false;
}

bool Jitx86Base::BackPatch(u32 emAddress, SContext* ctx)
{
  u8* codePtr = reinterpret_cast<u8*>(ctx->CTX_PC);

  if (!IsInSpace(codePtr))
    return false;  // this will become a regular crash real soon after this

  auto it = m_back_patch_info.find(codePtr);
  if (it == m_back_patch_info.end())
  {
    if (ARBruteForcer::ch_bruteforce)
      Core::KillDolphinAndRestart();
    else
      PanicAlert("BackPatch: no register use entry for address %p", codePtr);
    return false;
  }

  TrampolineInfo& info = it->second;

  u8* exceptionHandler = nullptr;
  if (jo.memcheck)
  {
    auto it2 = m_exception_handler_at_loc.find(codePtr);
    if (it2 != m_exception_handler_at_loc.end())
      exceptionHandler = it2->second;
  }

  // In the trampoline code, we jump back into the block at the beginning
  // of the next instruction. The next instruction comes immediately
  // after the backpatched operation, or BACKPATCH_SIZE bytes after the start
  // of the backpatched operation, whichever comes last. (The JIT inserts NOPs
  // into the original code if necessary to ensure there is enough space
  // to insert the backpatch jump.)

  js.generatingTrampoline = true;
  js.trampolineExceptionHandler = exceptionHandler;

  // Generate the trampoline.
  const u8* trampoline = trampolines.GenerateTrampoline(info);
  js.generatingTrampoline = false;
  js.trampolineExceptionHandler = nullptr;

  u8* start = info.start;

  // Patch the original memory operation.
  XEmitter emitter(start);
  emitter.JMP(trampoline, true);
  // NOPs become dead code
  const u8* end = info.start + info.len;
  for (const u8* i = emitter.GetCodePtr(); i < end; ++i)
    emitter.INT3();

  // Rewind time to just before the start of the write block. If we swapped memory
  // before faulting (eg: the store+swap was not an atomic op like MOVBE), let's
  // swap it back so that the swap can happen again (this double swap isn't ideal but
  // only happens the first time we fault).
  if (info.nonAtomicSwapStoreSrc != Gen::INVALID_REG)
  {
    u64* ptr = ContextRN(ctx, info.nonAtomicSwapStoreSrc);
    switch (info.accessSize << 3)
    {
    case 8:
      // No need to swap a byte
      break;
    case 16:
      *ptr = Common::swap16(static_cast<u16>(*ptr));
      break;
    case 32:
      *ptr = Common::swap32(static_cast<u32>(*ptr));
      break;
    case 64:
      *ptr = Common::swap64(static_cast<u64>(*ptr));
      break;
    default:
      _dbg_assert_(DYNA_REC, 0);
      break;
    }
  }

  // This is special code to undo the LEA in SafeLoadToReg if it clobbered the address
  // register in the case where reg_value shared the same location as opAddress.
  if (info.offsetAddedToAddress)
  {
    u64* ptr = ContextRN(ctx, info.op_arg.GetSimpleReg());
    *ptr -= static_cast<u32>(info.offset);
  }

  ctx->CTX_PC = reinterpret_cast<u64>(trampoline);

  return true;
}

void LogGeneratedX86(size_t size, const PPCAnalyst::CodeBuffer* code_buffer, const u8* normalEntry,
                     const JitBlock* b)
{
  for (size_t i = 0; i < size; i++)
  {
    const PPCAnalyst::CodeOp& op = code_buffer->codebuffer[i];
    std::string temp = StringFromFormat(
        "%08x %s", op.address, GekkoDisassembler::Disassemble(op.inst.hex, op.address).c_str());
    DEBUG_LOG(DYNA_REC, "IR_X86 PPC: %s\n", temp.c_str());
  }

  disassembler x64disasm;
  x64disasm.set_syntax_intel();

  u64 disasmPtr = reinterpret_cast<u64>(normalEntry);
  const u8* end = normalEntry + b->codeSize;

  while (reinterpret_cast<u8*>(disasmPtr) < end)
  {
    char sptr[1000] = "";
    disasmPtr += x64disasm.disasm64(disasmPtr, disasmPtr, reinterpret_cast<u8*>(disasmPtr), sptr);
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
      ss << static_cast<u32>(*(normalEntry + i));
    }
    DEBUG_LOG(DYNA_REC, "IR_X86 bin: %s\n\n\n", ss.str().c_str());
  }
}
