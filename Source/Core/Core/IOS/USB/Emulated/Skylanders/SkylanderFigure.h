// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace IOS::HLE::USB
{
enum class Type : u8;

struct SkylanderDateTime final
{
  u8 minute;
  u8 hour;
  u8 day;
  u8 month;
  u16 year;
};

struct SkylanderData final
{
  std::array<u8, 11> toy_code;
  u16 money;
  u16 hero_level;
  u32 playtime;
  // Null-terminated UTF-16 string
  std::array<u16, 0x10> nickname;
  SkylanderDateTime last_reset;
  SkylanderDateTime last_placed;
};

struct Trophydata final
{
  u8 unlocked_villains;
};

struct FigureData final
{
  Type normalized_type;
  u16 figure_id;
  u16 variant_id;
  union
  {
    SkylanderData skylander_data;
    Trophydata trophy_data;
  };
};

constexpr u32 BLOCK_COUNT = 0x40;
constexpr u32 BLOCK_SIZE = 0x10;
constexpr u32 FIGURE_SIZE = BLOCK_COUNT * BLOCK_SIZE;

class SkylanderFigure
{
public:
  SkylanderFigure(const std::string& file_path);
  SkylanderFigure(File::IOFile file);
  bool Create(u16 sky_id, u16 sky_var,
              std::optional<std::array<u8, 4>> requested_nuid = std::nullopt);
  void Save();
  void Close();
  bool FileIsOpen() const;
  void GetBlock(u8 index, u8* dest) const;
  void SetBlock(u8 block, const u8* buf);
  void DecryptFigure(std::array<u8, FIGURE_SIZE>* dest) const;
  FigureData GetData() const;
  void SetData(FigureData* data);

private:
  void PopulateSectorTrailers();
  void PopulateKeys();
  void GenerateIncompleteHashIn(u8* dest) const;
  void Encrypt(std::span<const u8, FIGURE_SIZE>);

  File::IOFile m_sky_file;
  std::array<u8, FIGURE_SIZE> m_data;
};
}  // namespace IOS::HLE::USB
