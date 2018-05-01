// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/MemoryPatches.h"

#include "Core/PowerPC/PowerPC.h"

void MemoryPatches::Add(u32 address, u32 value)
{
  const u32 old_value = PowerPC::HostRead_U32(address);
  m_patches.emplace_back(address, old_value);
  PowerPC::HostWrite_U32(value, address);
  PowerPC::ScheduleInvalidateCacheThreadSafe(address);
}

const std::vector<MemoryPatch>& MemoryPatches::List() const
{
  return m_patches;
}

void MemoryPatches::Delete(std::size_t index)
{
  if (index >= m_patches.size())
    return;

  const auto& it = m_patches.begin() + index;
  PowerPC::HostWrite_U32(it->old_value, it->address);
  PowerPC::ScheduleInvalidateCacheThreadSafe(it->address);
  m_patches.erase(it);
}

void MemoryPatches::Clear()
{
  m_patches.clear();
}
