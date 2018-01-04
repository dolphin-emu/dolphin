// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Jit64Common/FarCodeCache.h"

void FarCodeCache::Init()
{
  m_enabled = true;
}

void FarCodeCache::Shutdown()
{
  m_enabled = false;
}

bool FarCodeCache::Enabled() const
{
  return m_enabled;
}
