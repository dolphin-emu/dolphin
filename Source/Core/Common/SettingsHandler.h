// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Thanks to Treeki for writing the original class - 29/01/2012

#pragma once

#include <array>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
class SettingsHandler
{
public:
  enum
  {
    SETTINGS_SIZE = 0x100,
    // Key used to encrypt/decrypt setting.txt contents
    INITIAL_SEED = 0x73B5DBFA
  };

  using Buffer = std::array<u8, SETTINGS_SIZE>;
  SettingsHandler();
  explicit SettingsHandler(Buffer&& buffer);

  void AddSetting(std::string_view key, std::string_view value);

  const Buffer& GetBytes() const;
  void SetBytes(Buffer&& buffer);
  std::string GetValue(std::string_view key) const;

  void Decrypt();
  void Reset();
  static std::string GenerateSerialNumber();

private:
  void WriteLine(std::string_view str);
  void WriteByte(u8 b);

  std::array<u8, SETTINGS_SIZE> m_buffer;
  u32 m_position, m_key;
  std::string decoded;
};
}  // namespace Common
