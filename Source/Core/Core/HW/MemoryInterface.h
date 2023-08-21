// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

namespace Core
{
class System;
}
namespace MMIO
{
class Mapping;
}
class PointerWrap;

namespace MemoryInterface
{
class MemoryInterfaceManager
{
public:
  MemoryInterfaceManager();
  MemoryInterfaceManager(const MemoryInterfaceManager&) = delete;
  MemoryInterfaceManager(MemoryInterfaceManager&&) = delete;
  MemoryInterfaceManager& operator=(const MemoryInterfaceManager&) = delete;
  MemoryInterfaceManager& operator=(MemoryInterfaceManager&&) = delete;
  ~MemoryInterfaceManager();

  void Init();
  void Shutdown();

  void DoState(PointerWrap& p);
  void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

private:
  union MIRegion
  {
    u32 hex = 0;
    struct
    {
      u16 first_page;
      u16 last_page;
    };
  };

  union MIProtType
  {
    u16 hex = 0;

    BitField<0, 2, u16> reg0;
    BitField<2, 2, u16> reg1;
    BitField<4, 2, u16> reg2;
    BitField<6, 2, u16> reg3;
    BitField<8, 8, u16> reserved;
  };

  union MIIRQMask
  {
    u16 hex = 0;

    BitField<0, 1, u16> reg0;
    BitField<1, 1, u16> reg1;
    BitField<2, 1, u16> reg2;
    BitField<3, 1, u16> reg3;
    BitField<4, 1, u16> all_regs;
    BitField<5, 11, u16> reserved;
  };

  union MIIRQFlag
  {
    u16 hex = 0;

    BitField<0, 1, u16> reg0;
    BitField<1, 1, u16> reg1;
    BitField<2, 1, u16> reg2;
    BitField<3, 1, u16> reg3;
    BitField<4, 1, u16> all_regs;
    BitField<5, 11, u16> reserved;
  };

  union MIProtAddr
  {
    u32 hex = 0;
    struct
    {
      u16 lo;
      u16 hi;
    };
    BitField<0, 5, u32> reserved_1;
    BitField<5, 25, u32> addr;
    BitField<30, 2, u32> reserved_2;
  };

  union MITimer
  {
    u32 hex = 0;
    struct
    {
      u16 lo;
      u16 hi;
    };
  };

  struct MIMemStruct
  {
    std::array<MIRegion, 4> regions;
    MIProtType prot_type;
    MIIRQMask irq_mask;
    MIIRQFlag irq_flag;
    u16 unknown1 = 0;
    MIProtAddr prot_addr;
    std::array<MITimer, 10> timers;
    u16 unknown2 = 0;
  };

  MIMemStruct m_mi_mem;
};
}  // namespace MemoryInterface
