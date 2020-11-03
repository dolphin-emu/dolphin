// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/ScriptHooks.h"

bool ScriptHooks::HasBreakPoint(u32 address) const
{
#if __cplusplus >= 202002L
  return m_hooks.contains(address);
#else
  return m_hooks.count(address) > 0;
#endif
}

void ScriptHooks::ExecuteBreakPoint(u32 address)
{
  auto range = m_hooks.equal_range(address);
  for (auto it = range.first; it != range.second; it++)
    it->second.Execute();
}
