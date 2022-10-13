// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

namespace MMIO
{
class Mapping;
}
class PointerWrap;

namespace MemoryInterface
{
class MemoryInterfaceState
{
public:
  MemoryInterfaceState();
  MemoryInterfaceState(const MemoryInterfaceState&) = delete;
  MemoryInterfaceState(MemoryInterfaceState&&) = delete;
  MemoryInterfaceState& operator=(const MemoryInterfaceState&) = delete;
  MemoryInterfaceState& operator=(MemoryInterfaceState&&) = delete;
  ~MemoryInterfaceState();

  struct Data;
  Data& GetData() { return *m_data; }

private:
  std::unique_ptr<Data> m_data;
};

void Init();
void Shutdown();

void DoState(PointerWrap& p);
void RegisterMMIO(MMIO::Mapping* mmio, u32 base);
}  // namespace MemoryInterface
