// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <string>

#include <mbedtls/aes.h>
#include <mbedtls/md5.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Random.h"
#include "Common/StringUtil.h"
#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderCrypto.h"

using namespace IOS::HLE::USB::SkylanderCrypto;

namespace IOS::HLE::USB
{
void SkylanderFigure::PopulateSectorTrailers()
{
  // Set the sector permissions
  u32 first_block = 0x690F0F0F;
  u32 other_blocks = 0x69080F7F;
  memcpy(&m_data[0x36], &first_block, sizeof(first_block));
  for (size_t index = 1; index < 0x10; index++)
  {
    memcpy(&m_data[(index * 0x40) + 0x36], &other_blocks, sizeof(other_blocks));
  }
}

void SkylanderFigure::PopulateKeys()
{
  for (u8 sector = 0; sector < 0x10; sector++)
  {
    u16 key_offset = (sector * 64) + (3 * 16);
    u64 key = CalculateKeyA(sector, std::span<u8, 4>(m_data.begin(), 4));

    for (u32 j = 0; j < 6; j++)
    {
      u16 index = key_offset + (5 - j);
      u8 byte = (key >> (j * 8)) & 0xFF;
      m_data[index] = byte;
    }
  }
}

SkylanderFigure::SkylanderFigure(const std::string& file_path)
{
  m_sky_file = File::IOFile(file_path, "w+b");
  m_data = {};
}
// Generate a AES key without the block filled in
void SkylanderFigure::GenerateIncompleteHashIn(u8* dest) const
{
  std::array<u8, 0x56> hash_in = {};

  // copy first 2 blocks into hash
  GetBlock(0, hash_in.data());
  GetBlock(1, hash_in.data() + 0x10);

  // Skip 1 byte. Is a block index that needs to be set per block.

  // Byte array of ascii string " Copyright (C) 2010 Activision. All Rights Reserved.". The space at
  // the start of the string is intentional
  static constexpr std::array<u8, 0x35> HASH_CONST = {
      0x20, 0x43, 0x6F, 0x70, 0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x43, 0x29,
      0x20, 0x32, 0x30, 0x31, 0x30, 0x20, 0x41, 0x63, 0x74, 0x69, 0x76, 0x69, 0x73, 0x69,
      0x6F, 0x6E, 0x2E, 0x20, 0x41, 0x6C, 0x6C, 0x20, 0x52, 0x69, 0x67, 0x68, 0x74, 0x73,
      0x20, 0x52, 0x65, 0x73, 0x65, 0x72, 0x76, 0x65, 0x64, 0x2E, 0x20};

  memcpy(hash_in.data() + 0x21, HASH_CONST.data(), HASH_CONST.size());

  memcpy(dest, hash_in.data(), 0x56);
}
void SkylanderFigure::Encrypt(std::span<const u8, FIGURE_SIZE> input)
{
  std::array<u8, 0x56> hash_in = {};

  GenerateIncompleteHashIn(hash_in.data());

  std::array<u8, FIGURE_SIZE> encrypted = {};

  std::array<u8, BLOCK_SIZE> current_block = {};

  // Run for every block
  for (u8 i = 0; i < 64; ++i)
  {
    memcpy(current_block.data(), input.data() + (i * BLOCK_SIZE), BLOCK_SIZE);

    // Skip sector trailer and the first 8 blocks
    if (((i + 1) % 4 == 0) || i < 8)
    {
      memcpy(encrypted.data() + (i * BLOCK_SIZE), current_block.data(), BLOCK_SIZE);
      continue;
    }

    // Block index
    hash_in[0x20] = i;

    std::array<u8, BLOCK_SIZE> hash_out = {};

    mbedtls_md5_ret(hash_in.data(), 0x56, hash_out.data());

    mbedtls_aes_context aes_context = {};

    mbedtls_aes_setkey_enc(&aes_context, hash_out.data(), 128);

    mbedtls_aes_crypt_ecb(&aes_context, MBEDTLS_AES_ENCRYPT, current_block.data(),
                          encrypted.data() + (i * BLOCK_SIZE));
  }

  memcpy(m_data.data(), encrypted.data(), FIGURE_SIZE);

  DEBUG_LOG_FMT(IOS_USB, "Encrypted skylander data: \n{}", HexDump(encrypted.data(), FIGURE_SIZE));
}
SkylanderFigure::SkylanderFigure(File::IOFile file)
{
  m_sky_file = std::move(file);
  m_sky_file.Seek(0, File::SeekOrigin::Begin);
  m_sky_file.ReadBytes(m_data.data(), m_data.size());
}
bool SkylanderFigure::Create(u16 sky_id, u16 sky_var,
                             std::optional<std::array<u8, 4>> requested_nuid)
{
  if (!m_sky_file)
  {
    return false;
  }

  memset(m_data.data(), 0, m_data.size());

  PopulateSectorTrailers();

  // Set the NUID of the figure
  if (requested_nuid)
    std::memcpy(&m_data[0], requested_nuid->data(), 4);
  else
    Common::Random::Generate(&m_data[0], 4);

  // The BCC (Block Check Character)
  m_data[4] = m_data[0] ^ m_data[1] ^ m_data[2] ^ m_data[3];

  // ATQA
  m_data[5] = 0x81;
  m_data[6] = 0x01;

  // SAK
  m_data[7] = 0x0F;

  // Set the skylander info
  memcpy(&m_data[0x10], &sky_id, sizeof(sky_id));
  memcpy(&m_data[0x1C], &sky_var, sizeof(sky_var));

  // Set checksum
  ComputeChecksumType0(m_data.data(), m_data.data() + 0x1E);

  PopulateKeys();

  Save();
  return true;
}
void SkylanderFigure::Save()
{
  m_sky_file.Seek(0, File::SeekOrigin::Begin);
  m_sky_file.WriteBytes(m_data.data(), FIGURE_SIZE);
}

void SkylanderFigure::GetBlock(u8 index, u8* dest) const
{
  memcpy(dest, m_data.data() + (index * BLOCK_SIZE), BLOCK_SIZE);
}

FigureData SkylanderFigure::GetData() const
{
  FigureData figure_data = {.figure_id = Common::BitCastPtr<u16>(m_data.data() + 0x10),
                            .variant_id = Common::BitCastPtr<u16>(m_data.data() + 0x1C)};

  auto filter = std::make_pair(figure_data.figure_id, figure_data.variant_id);
  Type type = Type::Item;
  if (IOS::HLE::USB::list_skylanders.count(filter) != 0)
  {
    auto found = IOS::HLE::USB::list_skylanders.at(filter);
    type = found.type;
  }

  figure_data.normalized_type = NormalizeSkylanderType(type);
  if (figure_data.normalized_type == Type::Skylander)
  {
    std::array<u8, FIGURE_SIZE> decrypted = {};

    DecryptFigure(&decrypted);

    // Area with highest area counter is the newest
    u16 area_offset = ((decrypted[0x89] + 1U) != decrypted[0x249]) ? 0x80 : 0x240;

    figure_data.skylander_data = {
        .money = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x3),
        .hero_level = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x5A),
        .playtime = Common::BitCastPtr<u32>(decrypted.data() + area_offset + 0x5),
        .last_reset = {.minute = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x60),
                       .hour = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x61),
                       .day = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x62),
                       .month = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x63),
                       .year = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x64)},
        .last_placed = {.minute = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x50),
                        .hour = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x51),
                        .day = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x52),
                        .month = Common::BitCastPtr<u8>(decrypted.data() + area_offset + 0x53),
                        .year = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x54)}};

    figure_data.skylander_data.toy_code =
        ComputeToyCode(Common::BitCastPtr<u64>(decrypted.data() + 0x14));

    std::array<u16, 16> nickname{};

    // First nickname half
    for (size_t i = 0; i < 8; ++i)
    {
      nickname[i] = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x20 + (i * 2));
    }

    // Second nickname half
    for (size_t i = 0; i < 8; ++i)
    {
      nickname[i + 8] = Common::BitCastPtr<u16>(decrypted.data() + area_offset + 0x40 + (i * 2));
    }

    figure_data.skylander_data.nickname = nickname;
  }
  else if (figure_data.normalized_type == Type::Trophy)
  {
    std::array<u8, FIGURE_SIZE> decrypted = {};

    DecryptFigure(&decrypted);

    // Area with highest area counter is the newest
    u16 area_offset = ((decrypted[0x89] + 1U) != decrypted[0x249]) ? 0x80 : 0x240;

    figure_data.trophy_data.unlocked_villains = *(decrypted.data() + area_offset + 0x14);
  }

  return figure_data;
}
void SkylanderFigure::SetData(FigureData* figure_data)
{
  std::array<u8, FIGURE_SIZE> decrypted = {};

  DecryptFigure(&decrypted);

  if (figure_data->normalized_type == Type::Skylander)
  {
    // Only update area with lowest counter
    u16 area_offset = (decrypted[0x89] != (decrypted[0x249] + 1U)) ? 0x80 : 0x240;
    u16 other_area_offset = (area_offset == 0x80) ? 0x240 : 0x80;

    memcpy(decrypted.data() + area_offset + 0x3, &figure_data->skylander_data.money, 2);
    memcpy(decrypted.data() + area_offset + 0x5A, &figure_data->skylander_data.hero_level, 2);
    memcpy(decrypted.data() + area_offset + 0x5, &figure_data->skylander_data.playtime, 4);

    {
      memcpy(decrypted.data() + area_offset + 0x60, &figure_data->skylander_data.last_reset.minute,
             1);
      memcpy(decrypted.data() + area_offset + 0x61, &figure_data->skylander_data.last_reset.hour,
             1);
      memcpy(decrypted.data() + area_offset + 0x62, &figure_data->skylander_data.last_reset.day, 1);
      memcpy(decrypted.data() + area_offset + 0x63, &figure_data->skylander_data.last_reset.month,
             1);
      memcpy(decrypted.data() + area_offset + 0x64, &figure_data->skylander_data.last_reset.year,
             2);
    }

    {
      memcpy(decrypted.data() + area_offset + 0x50, &figure_data->skylander_data.last_placed.minute,
             1);
      memcpy(decrypted.data() + area_offset + 0x51, &figure_data->skylander_data.last_placed.hour,
             1);
      memcpy(decrypted.data() + area_offset + 0x52, &figure_data->skylander_data.last_placed.day,
             1);
      memcpy(decrypted.data() + area_offset + 0x53, &figure_data->skylander_data.last_placed.month,
             1);
      memcpy(decrypted.data() + area_offset + 0x54, &figure_data->skylander_data.last_placed.year,
             2);
    }

    {
      for (size_t i = 0; i < 8; ++i)
      {
        memcpy(decrypted.data() + area_offset + 0x20 + (i * 2),
               &figure_data->skylander_data.nickname[i], 0x2);
      }

      for (size_t i = 0; i < 8; ++i)
      {
        memcpy(decrypted.data() + area_offset + 0x40 + (i * 2),
               &figure_data->skylander_data.nickname[8 + i], 0x2);
      }
    }

    {
      ComputeChecksumType3(decrypted.data() + area_offset + 0x50,
                           decrypted.data() + area_offset + 0xA);
      ComputeChecksumType2(decrypted.data() + area_offset + 0x10,
                           decrypted.data() + area_offset + 0xC);
      decrypted[area_offset + 9] = decrypted[other_area_offset + 9] + 1;
      ComputeChecksumType1(decrypted.data() + area_offset, decrypted.data() + area_offset + 0xE);
    }

    {
      area_offset = (decrypted[0x112] != (decrypted[0x2D2] + 1U)) ? 0x110 : 0x2D0;
      other_area_offset = (area_offset == 0x110) ? 0x2D0 : 0x110;

      decrypted[area_offset + 2] = decrypted[other_area_offset + 2] + 1;

      ComputeChecksumType6(decrypted.data() + area_offset, decrypted.data() + area_offset);
    }
  }
  else if (figure_data->normalized_type == Type::Trophy)
  {
    u16 area_offset = (decrypted[0x89] != (decrypted[0x249] + 1U)) ? 0x80 : 0x240;
    u16 other_area_offset = (area_offset == 0x80) ? 0x240 : 0x80;

    {
      memcpy(decrypted.data() + area_offset + 0x14, &figure_data->trophy_data.unlocked_villains, 1);

      {
        ComputeChecksumType3(decrypted.data() + area_offset + 0x50,
                             decrypted.data() + area_offset + 0xA);
        ComputeChecksumType2(decrypted.data() + area_offset + 0x10,
                             decrypted.data() + area_offset + 0xC);
        decrypted[area_offset + 9] = decrypted[other_area_offset + 9] + 1;
        ComputeChecksumType1(decrypted.data() + area_offset, decrypted.data() + area_offset + 0xE);
      }
    }

    {
      area_offset = (decrypted[0x112] != (decrypted[0x2D2] + 1U)) ? 0x110 : 0x2D0;
      other_area_offset = (area_offset == 0x110) ? 0x2D0 : 0x110;

      decrypted[area_offset + 2] = decrypted[other_area_offset + 2] + 1;

      ComputeChecksumType6(decrypted.data() + area_offset, decrypted.data() + area_offset);
    }
  }

  // Encrypt again
  Encrypt(decrypted);

  Save();
}
void SkylanderFigure::DecryptFigure(std::array<u8, FIGURE_SIZE>* dest) const
{
  std::array<u8, 0x56> hash_in = {};

  GenerateIncompleteHashIn(hash_in.data());

  std::array<u8, FIGURE_SIZE> decrypted = {};

  std::array<u8, BLOCK_SIZE> current_block = {};

  // Run for every block
  for (u8 i = 0; i < BLOCK_COUNT; ++i)
  {
    GetBlock(i, current_block.data());

    // Skip sector trailer and the first 8 blocks
    if (((i + 1) % 4 == 0) || i < 8)
    {
      memcpy(decrypted.data() + (i * BLOCK_SIZE), current_block.data(), BLOCK_SIZE);
      continue;
    }

    // Check if block is all 0
    u16 total = 0;
    for (size_t j = 0; j < BLOCK_SIZE; j++)
    {
      total += current_block[j];
    }
    if (total == 0)
    {
      continue;
    }

    // Block index
    hash_in[0x20] = i;

    std::array<u8, BLOCK_SIZE> hash_out = {};

    mbedtls_md5_ret(hash_in.data(), 0x56, hash_out.data());

    mbedtls_aes_context aes_context = {};

    mbedtls_aes_setkey_dec(&aes_context, hash_out.data(), 128);

    mbedtls_aes_crypt_ecb(&aes_context, MBEDTLS_AES_DECRYPT, current_block.data(),
                          decrypted.data() + (i * BLOCK_SIZE));
  }

  memcpy(dest->data(), decrypted.data(), FIGURE_SIZE);

  DEBUG_LOG_FMT(IOS_USB, "Decrypted skylander data: \n{}", HexDump(decrypted.data(), FIGURE_SIZE));
}
void SkylanderFigure::Close()
{
  m_sky_file.Close();
}
void SkylanderFigure::SetBlock(u8 block, const u8* buf)
{
  memcpy(m_data.data() + (block * BLOCK_SIZE), buf, BLOCK_SIZE);
}
bool SkylanderFigure::FileIsOpen() const
{
  return m_sky_file.IsOpen();
}
}  // namespace IOS::HLE::USB
