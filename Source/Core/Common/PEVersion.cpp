// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <Windows.h>

#include <limits>

#include "Common/PEVersion.h"
#include "Common/StringUtil.h"

#pragma comment(lib, "version.lib")

std::optional<PEVersion> PEVersion::from_u32(u32 major, u32 minor, u32 build, u32 qfe)
{
  const auto max = std::numeric_limits<u16>::max();
  if (major > max || minor > max || build > max || qfe > max)
    return {};
  return PEVersion{.major = static_cast<u16>(major),
                   .minor = static_cast<u16>(minor),
                   .build = static_cast<u16>(build),
                   .qfe = static_cast<u16>(qfe)};
}

std::optional<PEVersion> PEVersion::from_string(const std::string& str)
{
  auto components = SplitString(str, '.');
  // Allow variable number of components, but not empty.
  if (components.size() == 0)
    return {};
  PEVersion version;
  if (!TryParse(components[0], &version.major, 10))
    return {};
  if (components.size() > 1 && !TryParse(components[1], &version.minor, 10))
    return {};
  if (components.size() > 2 && !TryParse(components[2], &version.build, 10))
    return {};
  if (components.size() > 3 && !TryParse(components[3], &version.qfe, 10))
    return {};
  return version;
}

std::optional<PEVersion> PEVersion::from_file(const std::filesystem::path& path)
{
  DWORD handle;
  DWORD data_len = GetFileVersionInfoSizeW(path.c_str(), &handle);
  if (!data_len)
    return {};
  std::vector<u8> block(data_len);
  if (!GetFileVersionInfoW(path.c_str(), 0, data_len, block.data()))
    return {};
  void* buf;
  UINT buf_len;
  if (!VerQueryValueW(block.data(), LR"(\)", &buf, &buf_len))
    return {};
  auto info = static_cast<VS_FIXEDFILEINFO*>(buf);
  return PEVersion{.major = static_cast<u16>(info->dwFileVersionMS >> 16),
                   .minor = static_cast<u16>(info->dwFileVersionMS),
                   .build = static_cast<u16>(info->dwFileVersionLS >> 16),
                   .qfe = static_cast<u16>(info->dwFileVersionLS)};
}
