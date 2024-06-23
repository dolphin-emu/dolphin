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
using SettingsBuffer = std::array<u8, 0x100>;

class SettingsWriter
{
public:
  SettingsWriter();

  void AddSetting(std::string_view key, std::string_view value);

  const SettingsBuffer& GetBytes() const;

  static std::string GenerateSerialNumber();

private:
  void WriteLine(std::string_view str);
  void WriteByte(u8 b);

  SettingsBuffer m_buffer;
  u32 m_position, m_key;
};

class SettingsReader
{
public:
  explicit SettingsReader(const SettingsBuffer& buffer);

  std::string GetValue(std::string_view key) const;

private:
  std::string m_decoded;
};
}  // namespace Common
