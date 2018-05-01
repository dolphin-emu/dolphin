// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"

struct MemoryPatch
{
  u32 address;
  u32 old_value;

  MemoryPatch(u32 address, u32 old_value) : address(address), old_value(old_value) {}
};

class MemoryPatches
{
public:
  void Add(u32 address, u32 value);
  const std::vector<MemoryPatch>& List() const;
  void Delete(std::size_t index);
  void Clear();

private:
  std::vector<MemoryPatch> m_patches;
};
