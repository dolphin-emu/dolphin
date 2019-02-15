// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/Sram.h"

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/ConfigManager.h"

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

#if 0
// german
const SRAM sram_dump_german = {{
	0x1F, 0x66,
	0xE0, 0x96,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x04, 0xEA, 0x19, 0x40,
	0x00,
	0x00,
	0x01,
	0x3C,
	0x12, 0xD5, 0xEA, 0xD3, 0x00, 0xFA, 0x2D, 0x33, 0x13, 0x41, 0x26, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00,
	0x00,
	0x00,
	0x84, 0xFF,
	0x00, 0x00,
	0x00, 0x00
}};
#endif

void InitSRAM()
{
  File::IOFile file(SConfig::GetInstance().m_strSRAM, "rb");
  if (file)
  {
    if (!file.ReadArray(&g_SRAM, 1))
    {
      ERROR_LOG(EXPANSIONINTERFACE, "EXI IPL-DEV: Could not read all of SRAM");
      g_SRAM = sram_dump;
    }
  }
  else
  {
    g_SRAM = sram_dump;
  }
}

void SetCardFlashID(const u8* buffer, u8 card_index)
{
  u64 rand = Common::swap64(&buffer[12]);
  u8 csum = 0;
  for (int i = 0; i < 12; i++)
  {
    rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
    csum += g_SRAM.settings_ex.flash_id[card_index][i] = buffer[i] - ((u8)rand & 0xff);
    rand = (((rand * (u64)0x0000000041c64e6dULL) + (u64)0x0000000000003039ULL) >> 16);
    rand &= (u64)0x0000000000007fffULL;
  }
  g_SRAM.settings_ex.flash_id_checksum[card_index] = csum ^ 0xFF;
}

void FixSRAMChecksums()
{
  // 16bit big-endian additive checksum
  u16 checksum = 0;
  u16 checksum_inv = 0;
  for (auto p = reinterpret_cast<u16*>(&g_SRAM.settings.rtc_bias);
       p != reinterpret_cast<u16*>(&g_SRAM.settings_ex); p++)
  {
    u16 value = Common::FromBigEndian(*p);
    checksum += value;
    checksum_inv += ~value;
  }
  g_SRAM.settings.checksum = checksum;
  g_SRAM.settings.checksum_inv = checksum_inv;
}
