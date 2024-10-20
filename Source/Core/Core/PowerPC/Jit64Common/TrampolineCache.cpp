// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Jit64Common/TrampolineCache.h"

#include <string>

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/MsgHandler.h"
#include "Common/x64Emitter.h"
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64Common/Jit64Constants.h"
#include "Core/PowerPC/Jit64Common/Jit64PowerPCState.h"
#include "Core/PowerPC/Jit64Common/TrampolineInfo.h"
#include "Core/PowerPC/PowerPC.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

void TrampolineCache::ClearCodeSpace()
{
  X64CodeBlock::ClearCodeSpace();
}

const u8* TrampolineCache::GenerateTrampoline(const TrampolineInfo& info)
{
  if (info.read)
  {
    return GenerateReadTrampoline(info);
  }

  return GenerateWriteTrampoline(info);
}

const u8* TrampolineCache::GenerateReadTrampoline(const TrampolineInfo& info)
{
  if (GetSpaceLeft() < 1024)
    PanicAlertFmt("Trampoline cache full");

  const u8* trampoline = GetCodePtr();

  SafeLoadToReg(info.op_reg, info.op_arg, info.accessSize << 3, info.offset, info.inst,
                info.registersInUse, info.signExtend,
                info.flags | SAFE_LOADSTORE_FORCE_SLOW_ACCESS);

  JMP(info.start + info.len, Jump::Near);

  Common::JitRegister::Register(trampoline, GetCodePtr(), "JIT_ReadTrampoline_{:x}", info.pc);
  return trampoline;
}

const u8* TrampolineCache::GenerateWriteTrampoline(const TrampolineInfo& info)
{
  if (GetSpaceLeft() < 1024)
    PanicAlertFmt("Trampoline cache full");

  const u8* trampoline = GetCodePtr();

  // Don't treat FIFO writes specially for now because they require a burst
  // check anyway.

  SafeWriteRegToReg(info.op_arg, info.op_reg, info.accessSize << 3, info.offset, info.inst,
                    info.registersInUse, info.flags | SAFE_LOADSTORE_FORCE_SLOW_ACCESS);

  JMP(info.start + info.len, Jump::Near);

  Common::JitRegister::Register(trampoline, GetCodePtr(), "JIT_WriteTrampoline_{:x}", info.pc);
  return trampoline;
}
