// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <fmt/format.h>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"

struct PEVersion
{
  auto operator<=>(PEVersion const& rhs) const = default;

  static std::optional<PEVersion> from_u32(u32 major, u32 minor, u32 build, u32 qfe = 0);
  static std::optional<PEVersion> from_string(const std::string& str);
  static std::optional<PEVersion> from_file(const std::filesystem::path& path);

  u16 major;
  u16 minor;
  u16 build;
  u16 qfe;
};

template <>
struct fmt::formatter<PEVersion>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const PEVersion& version, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{}.{}.{}.{}", version.major, version.minor, version.build,
                          version.qfe);
  }
};
