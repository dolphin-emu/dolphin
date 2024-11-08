// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/Sram.h"

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "Core/HW/EXI/EXI.h"

// English
// This is just a template. Most/all fields are updated with sane(r) values at runtime.
// clang-format off
const Sram sram_dump = {Common::BigEndianValue<u32>{0},
                        {Common::BigEndianValue<u16>{0x2c}, Common::BigEndianValue<u16>{0xffd0}, 0,
                         0, 0, 0, 0, 0, {0x20 | SramFlags::kOobeDone | SramFlags::kStereo}},
                        {{
                             {'D', 'O', 'L', 'P', 'H', 'I', 'N', 'S', 'L', 'O', 'T', 'A'},
                             {'D', 'O', 'L', 'P', 'H', 'I', 'N', 'S', 'L', 'O', 'T', 'B'},
                         },
                         0,
                         {},
                         0,
                         0,
                         {0x6E, 0x6D},
                         0,
                         {}}};
// clang-format on

void InitSRAM(Sram* sram, const std::string& filename)
{
  File::IOFile file(filename, "rb");
  if (file)
  {
    if (!file.ReadArray(sram, 1))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "EXI IPL-DEV: Could not read all of SRAM");
      *sram = sram_dump;
    }
  }
  else
  {
    *sram = sram_dump;
  }
}

void SetCardFlashID(Sram* sram, const u8* buffer, ExpansionInterface::Slot card_slot)
{
  u8 card_index;
  switch (card_slot)
  {
  case ExpansionInterface::Slot::A:
    card_index = 0;
    break;
  case ExpansionInterface::Slot::B:
    card_index = 1;
    break;
  default:
    PanicAlertFmt("Invalid memcard slot {}", card_slot);
    return;
  }

  u64 rand = Common::swap64(&buffer[12]);
  u8 csum = 0;
  for (int i = 0; i < 12; i++)
  {
    rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
    csum += sram->settings_ex.flash_id[card_index][i] = buffer[i] - ((u8)rand & 0xff);
    rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
    rand &= (u64)0x0000000000007fffULL;
  }
  sram->settings_ex.flash_id_checksum[card_index] = csum ^ 0xFF;
}

void FixSRAMChecksums(Sram* sram)
{
  // 16bit big-endian additive checksum
  u16 checksum = 0;
  u16 checksum_inv = 0;
  for (auto p = reinterpret_cast<u16*>(&sram->settings.rtc_bias);
       p != reinterpret_cast<u16*>(&sram->settings_ex); p++)
  {
    u16 value = Common::FromBigEndian(*p);
    checksum += value;
    checksum_inv += ~value;
  }
  sram->settings.checksum = checksum;
  sram->settings.checksum_inv = checksum_inv;
}
