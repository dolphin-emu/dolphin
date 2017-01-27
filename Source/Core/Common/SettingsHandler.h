// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Thanks to Treeki for writing the original class - 29/01/2012

#pragma once

#include <array>
#include <string>

#include "Common/CommonTypes.h"

class SettingsHandler
{
public:
  enum
  {
    SETTINGS_SIZE = 0x100,
    // Key used to encrypt/decrypt setting.txt contents
    INITIAL_SEED = 0x73B5DBFA
  };

  SettingsHandler();

  bool Open(const std::string& settings_file_path);
  bool Save(const std::string& destination_file_path) const;

  void AddSetting(const std::string& key, const std::string& value);

  const u8* GetData() const;
  const std::string GetValue(const std::string& key);

  void Decrypt();
  void Reset();
  static std::string GenerateSerialNumber();

private:
  void WriteByte(u8 b);

  std::array<u8, SETTINGS_SIZE> m_buffer;
  u32 m_position, m_key;
  std::string decoded;
};
