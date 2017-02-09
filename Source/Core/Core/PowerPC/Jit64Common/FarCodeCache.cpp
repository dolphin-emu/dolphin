// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/FarCodeCache.h"

alignas(JIT_MEM_ALIGNMENT) std::array<u8, FARCODE_SIZE> FarCodeCache::code_area;

void FarCodeCache::Init()
{
  SetCodeSpace(code_area.data(), code_area.size());
  m_enabled = true;
}

void FarCodeCache::Shutdown()
{
  ReleaseCodeSpace();
  m_enabled = false;
}

bool FarCodeCache::Enabled() const
{
  return m_enabled;
}
