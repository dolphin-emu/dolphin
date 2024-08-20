// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylanders/SkylanderCrypto.h"

#include <array>
#include <span>
#include <string>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Swap.h"

namespace IOS::HLE::USB::SkylanderCrypto
{
u16 ComputeCRC16(std::span<const u8> data)
{
  u16 crc = 0xFFFF;

  for (size_t i = 0; i < data.size(); ++i)
  {
    crc ^= data[i] << 8;

    for (size_t j = 0; j < 8; j++)
    {
      if (Common::ExtractBit(crc, 15))
      {
        const u16 polynomial = 0x1021;
        crc = (crc << 1) ^ polynomial;
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}
// CRC-64 algorithm that is limited to 48 bits every iteration
u64 ComputeCRC48(std::span<const u8> data)
{
  const u64 initial_register_value = 2ULL * 2ULL * 3ULL * 1103ULL * 12868356821ULL;

  u64 crc = initial_register_value;
  for (size_t i = 0; i < data.size(); ++i)
  {
    crc ^= (static_cast<u64>(data[i]) << 40);
    for (size_t j = 0; j < 8; ++j)
    {
      if (Common::ExtractBit(crc, 47))
      {
        const u64 polynomial = 0x42f0e1eba9ea3693;
        crc = (crc << 1) ^ polynomial;
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc & 0x0000FFFFFFFFFFFF;
}
u64 CalculateKeyA(u8 sector, std::span<const u8, 0x4> nuid)
{
  if (sector == 0)
  {
    return 73ULL * 2017ULL * 560381651ULL;
  }

  std::array<u8, 5> data = {nuid[0], nuid[1], nuid[2], nuid[3], sector};

  u64 big_endian_crc = ComputeCRC48(data);
  u64 little_endian_crc = Common::swap64(big_endian_crc) >> 16;

  return little_endian_crc;
}
void ComputeChecksumType0(const u8* data_start, u8* output)
{
  std::array<u8, 0x1E> input = {};
  memcpy(input.data(), data_start, 0x1E);
  u16 crc = ComputeCRC16(input);
  memcpy(output, &crc, 2);
}
void ComputeChecksumType1(const u8* data_start, u8* output)
{
  std::array<u8, 0x10> input = {};
  memcpy(input.data(), data_start, 0x10);
  input[0xE] = 0x05;
  input[0xF] = 0x00;
  u16 crc = ComputeCRC16(input);
  memcpy(output, &crc, 2);
}
void ComputeChecksumType2(const u8* data_start, u8* output)
{
  std::array<u8, 0x30> input = {};
  memcpy(input.data(), data_start, 0x20);
  memcpy(input.data() + 0x20, data_start + 0x30, 0x10);
  u16 crc = ComputeCRC16(input);
  memcpy(output, &crc, 2);
}
void ComputeChecksumType3(const u8* data_start, u8* output)
{
  std::array<u8, 0x110> input = {};
  memcpy(input.data(), data_start, 0x20);
  memcpy(input.data() + 0x20, data_start + 0x30, 0x10);
  u16 crc = ComputeCRC16(input);
  memcpy(output, &crc, 2);
}

void ComputeChecksumType6(const u8* data_start, u8* output)
{
  std::array<u8, 0x40> input = {};
  memcpy(input.data(), data_start, 0x20);
  memcpy(input.data() + 0x20, data_start + 0x30, 0x20);

  input[0x0] = 0x06;
  input[0x1] = 0x01;

  u16 crc = ComputeCRC16(input);
  memcpy(output, &crc, 2);
}
std::array<u8, 11> ComputeToyCode(u64 code)
{
  if (code == 0)
  {
    static constexpr std::array<u8, 11> invalid_code_result{
        static_cast<u8>('N'), static_cast<u8>('/'), static_cast<u8>('A')};
    return invalid_code_result;
  }

  std::array<u8, 10> code_bytes;
  for (size_t i = 0; i < code_bytes.size(); ++i)
  {
    code_bytes[i] = static_cast<u8>(code % 29);
    code /= 29;
  }

  static constexpr char lookup_table[] = "23456789BCDFGHJKLMNPQRSTVWXYZ";
  std::array<u8, 10> code_chars;
  for (size_t i = 0; i < code_bytes.size(); ++i)
  {
    code_chars[i] = static_cast<u8>(lookup_table[code_bytes[9 - i]]);
  }

  std::array<u8, 11> result;
  std::memcpy(&result[0], &code_chars[0], 5);
  result[5] = static_cast<u8>('-');
  std::memcpy(&result[6], &code_chars[5], 5);

  return result;
}
}  // namespace IOS::HLE::USB::SkylanderCrypto
