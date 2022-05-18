// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/MemoryInterface.h"

#include <array>

#include "Common/BitField.h"
#include "Common/BitField2.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/HW/MMIO.h"

namespace MemoryInterface
{
// internal hardware addresses
enum
{
  MI_REGION0_FIRST = 0x000,
  MI_REGION0_LAST = 0x002,
  MI_REGION1_FIRST = 0x004,
  MI_REGION1_LAST = 0x006,
  MI_REGION2_FIRST = 0x008,
  MI_REGION2_LAST = 0x00A,
  MI_REGION3_FIRST = 0x00C,
  MI_REGION3_LAST = 0x00E,
  MI_PROT_TYPE = 0x010,
  MI_IRQMASK = 0x01C,
  MI_IRQFLAG = 0x01E,
  MI_UNKNOWN1 = 0x020,
  MI_PROT_ADDR_LO = 0x022,
  MI_PROT_ADDR_HI = 0x024,
  MI_TIMER0_HI = 0x032,
  MI_TIMER0_LO = 0x034,
  MI_TIMER1_HI = 0x036,
  MI_TIMER1_LO = 0x038,
  MI_TIMER2_HI = 0x03A,
  MI_TIMER2_LO = 0x03C,
  MI_TIMER3_HI = 0x03E,
  MI_TIMER3_LO = 0x040,
  MI_TIMER4_HI = 0x042,
  MI_TIMER4_LO = 0x044,
  MI_TIMER5_HI = 0x046,
  MI_TIMER5_LO = 0x048,
  MI_TIMER6_HI = 0x04A,
  MI_TIMER6_LO = 0x04C,
  MI_TIMER7_HI = 0x04E,
  MI_TIMER7_LO = 0x050,
  MI_TIMER8_HI = 0x052,
  MI_TIMER8_LO = 0x054,
  MI_TIMER9_HI = 0x056,
  MI_TIMER9_LO = 0x058,
  MI_UNKNOWN2 = 0x05A,
};

union MIRegion
{
  u32 hex;
  struct
  {
    u16 first_page;
    u16 last_page;
  };

  constexpr MIRegion(u32 val = 0) : hex(val) {}
};

struct MIProtType : BitField2<u16>
{
  FIELD(u16, 0, 2, reg0);
  FIELD(u16, 2, 2, reg1);
  FIELD(u16, 4, 2, reg2);
  FIELD(u16, 6, 2, reg3);
  FIELD(u16, 8, 8, reserved);

  constexpr MIProtType(u16 val = 0) : BitField2(val) {}
};

struct MIIRQMask : BitField2<u16>
{
  FIELD(bool, 0, 1, reg0);
  FIELD(bool, 1, 1, reg1);
  FIELD(bool, 2, 1, reg2);
  FIELD(bool, 3, 1, reg3);
  FIELD(u16, 4, 1, all_regs);
  FIELD(u16, 5, 11, reserved);

  constexpr MIIRQMask(u16 val = 0) : BitField2(val) {}
};

struct MIIRQFlag : BitField2<u16>
{
  FIELD(bool, 0, 1, reg0);
  FIELD(bool, 1, 1, reg1);
  FIELD(bool, 2, 1, reg2);
  FIELD(bool, 3, 1, reg3);
  FIELD(u16, 4, 1, all_regs);
  FIELD(u16, 5, 11, reserved);

  constexpr MIIRQFlag(u16 val = 0) : BitField2(val) {}
};

union MIProtAddr
{
  BitField2<u32> full;
  struct
  {
    u16 lo;
    u16 hi;
  };
  FIELD_IN(full, u32, 0, 5, reserved_1);
  FIELD_IN(full, u32, 5, 25, addr);
  FIELD_IN(full, u32, 30, 2, reserved_2);

  constexpr MIProtAddr(u32 val = 0) : full(val) {}
};

union MITimer
{
  u32 hex;
  struct
  {
    u16 lo;
    u16 hi;
  };

  constexpr MITimer(u32 val = 0) : hex(val) {}
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

// STATE_TO_SAVE
static MIMemStruct g_mi_mem;

void DoState(PointerWrap& p)
{
  p.Do(g_mi_mem);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  for (u32 i = MI_REGION0_FIRST; i <= MI_REGION3_LAST; i += 4)
  {
    auto& region = g_mi_mem.regions[i / 4];
    mmio->Register(base | i, MMIO::DirectRead<u16>(&region.first_page),
                   MMIO::DirectWrite<u16>(&region.first_page));
    mmio->Register(base | (i + 2), MMIO::DirectRead<u16>(&region.last_page),
                   MMIO::DirectWrite<u16>(&region.last_page));
  }

  mmio->Register(base | MI_PROT_TYPE, MMIO::DirectRead<u16>(&g_mi_mem.prot_type.storage),
                 MMIO::DirectWrite<u16>(&g_mi_mem.prot_type.storage));

  mmio->Register(base | MI_IRQMASK, MMIO::DirectRead<u16>(&g_mi_mem.irq_mask.storage),
                 MMIO::DirectWrite<u16>(&g_mi_mem.irq_mask.storage));

  mmio->Register(base | MI_IRQFLAG, MMIO::DirectRead<u16>(&g_mi_mem.irq_flag.storage),
                 MMIO::DirectWrite<u16>(&g_mi_mem.irq_flag.storage));

  mmio->Register(base | MI_UNKNOWN1, MMIO::DirectRead<u16>(&g_mi_mem.unknown1),
                 MMIO::DirectWrite<u16>(&g_mi_mem.unknown1));

  // The naming is confusing here: the register contains the lower part of
  // the address (hence MI_..._LO but this is still the high part of the
  // overall register.
  mmio->Register(base | MI_PROT_ADDR_LO, MMIO::DirectRead<u16>(&g_mi_mem.prot_addr.hi),
                 MMIO::DirectWrite<u16>(&g_mi_mem.prot_addr.hi));
  mmio->Register(base | MI_PROT_ADDR_HI, MMIO::DirectRead<u16>(&g_mi_mem.prot_addr.lo),
                 MMIO::DirectWrite<u16>(&g_mi_mem.prot_addr.lo));

  for (u32 i = 0; i < g_mi_mem.timers.size(); ++i)
  {
    auto& timer = g_mi_mem.timers[i];
    mmio->Register(base | (MI_TIMER0_HI + 4 * i), MMIO::DirectRead<u16>(&timer.hi),
                   MMIO::DirectWrite<u16>(&timer.hi));
    mmio->Register(base | (MI_TIMER0_LO + 4 * i), MMIO::DirectRead<u16>(&timer.lo),
                   MMIO::DirectWrite<u16>(&timer.lo));
  }

  mmio->Register(base | MI_UNKNOWN2, MMIO::DirectRead<u16>(&g_mi_mem.unknown2),
                 MMIO::DirectWrite<u16>(&g_mi_mem.unknown2));

  for (u32 i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToSmaller<u32>(mmio, base | i, base | (i + 2)),
                   MMIO::WriteToSmaller<u32>(mmio, base | i, base | (i + 2)));
  }
}

}  // namespace MemoryInterface
