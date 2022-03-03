// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
