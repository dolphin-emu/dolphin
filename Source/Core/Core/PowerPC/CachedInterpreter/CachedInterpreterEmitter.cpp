// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/CachedInterpreter/CachedInterpreterEmitter.h"

#include <algorithm>
#include <cstring>

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

void CachedInterpreterEmitter::Write(AnyCallback callback, const void* operands, std::size_t size)
{
  DEBUG_ASSERT(reinterpret_cast<std::uintptr_t>(m_code) % alignof(AnyCallback) == 0);
  if (m_code + sizeof(callback) + size >= m_code_end)
  {
    m_write_failed = true;
    return;
  }
  std::memcpy(m_code, &callback, sizeof(callback));
  m_code += sizeof(callback);
  if (size == 0)
    return;
  std::memcpy(m_code, operands, size);
  m_code += size;
}

s32 CachedInterpreterEmitter::PoisonCallback(PowerPC::PowerPCState& ppc_state, const void* operands)
{
  ASSERT_MSG(DYNA_REC, false,
             "The Cached Interpreter reached a poisoned callback. This should never happen!");
  return 0;
}

void CachedInterpreterCodeBlock::PoisonMemory()
{
  DEBUG_ASSERT(reinterpret_cast<std::uintptr_t>(region) % alignof(AnyCallback) == 0);
  DEBUG_ASSERT(region_size % sizeof(AnyCallback) == 0);
  std::fill(reinterpret_cast<AnyCallback*>(region),
            reinterpret_cast<AnyCallback*>(region + region_size), AnyCallbackCast(PoisonCallback));
}
