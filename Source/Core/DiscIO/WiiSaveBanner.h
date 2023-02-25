// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class WiiSaveBanner
{
public:
  explicit WiiSaveBanner(u64 title_id);
  explicit WiiSaveBanner(const std::string& path);

  bool IsValid() const { return m_valid; }
  const std::string& GetPath() const { return m_path; }
  std::string GetName() const;
  std::string GetDescription() const;

  std::vector<u32> GetBanner(u32* width, u32* height) const;

private:
  struct Header
  {
    char magic[4];  // "WIBN"
    u32 flags;
    u16 animation_speed;
    u8 unused[22];
    char16_t name[32];
    char16_t description[32];
  } m_header;

  bool m_valid = true;
  std::string m_path;
};
}  // namespace DiscIO
