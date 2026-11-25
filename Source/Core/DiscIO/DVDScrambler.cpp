// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Heavily based on DVD Scrambler / Descrambler research by Deterous
// https://github.com/Deterous/DVD-Scrambler

#include "DiscIO/DVDScrambler.h"

#include <array>
#include <optional>
#include <span>

#include "Common/CommonTypes.h"

namespace DiscIO
{
// The ECMA-267 XOR stream repeats every 16 physical sectors (32 KiB)
constexpr u32 SCRAMBLER_TABLE_SIZE = 2048 * 16;

static const std::array<u8, SCRAMBLER_TABLE_SIZE>& GetScramblerTable()
{
  static const std::array<u8, SCRAMBLER_TABLE_SIZE> s_table = [] {
    std::array<u8, SCRAMBLER_TABLE_SIZE> table{};

    // 0x0001 is the initial seed value specified by the DVD-ROM standard
    u16 shift_register = 0x0001;
    for (u8& value : table)
    {
      // The lowest 8 bits of the register populate the current table byte
      value = static_cast<u8>(shift_register);

      // Shift the LFSR 8 times to construct the next byte
      for (int bit = 0; bit < 8; ++bit)
      {
        // Feedback polynomial G(x) = x^15 + x^11 + 1, taps at bit 14 and bit 10
        const u16 lsb = ((shift_register >> 14) & 1) ^ ((shift_register >> 10) & 1);

        // Advance the register, mask it to 15 bits and merge the feedback bit
        shift_register = static_cast<u16>(((shift_register << 1) & 0x7FFF) | lsb);
      }
    }
    return table;
  }();
  return s_table;
}

void DescrambleDVDSector(std::span<u8, 2048> main_data, u32 psn, std::optional<u8> disc_key)
{
  const std::array<u8, SCRAMBLER_TABLE_SIZE>& table = GetScramblerTable();

  // Bits 4 to 7 of the PSN select one of the 16 sectors of the XOR stream
  const u32 row = (psn >> 4) & 0xF;
  u32 offset = row * 2048;

  // GameCube and Wii discs use a non-standard scrambling offset: the sequence is shifted by 7.5
  // physical sectors (15,360 bytes) and the row index is XORed with the disc key
  if (disc_key)
    offset = (*disc_key ^ row) * 2048 + 7 * 2048 + 1024;

  for (u32 i = 0; i < 2048; ++i)
  {
    u32 index = offset + i;
    // Wrap within the table. The subtrahend is SCRAMBLER_TABLE_SIZE - 1, not SCRAMBLER_TABLE_SIZE
    if (index >= SCRAMBLER_TABLE_SIZE)
      index -= SCRAMBLER_TABLE_SIZE - 1;
    main_data[i] ^= table[index];
  }
}

u32 CalculateDVDEDC(std::span<const u8> data)
{
  // CRC-32 with the 0x80000011 polynomial
  u32 crc = 0;
  for (const u8 byte : data)
  {
    crc ^= static_cast<u32>(byte) << 24;
    for (int bit = 0; bit < 8; ++bit)
    {
      if (crc & 0x80000000)
        crc = (crc << 1) ^ 0x80000011;
      else
        crc <<= 1;
    }
  }
  return crc;
}
}  // namespace DiscIO
