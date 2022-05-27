// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <fmt/format.h>
#include <string>
#include <winerror.h>

#include "Common/CommonTypes.h"

namespace Common
{
std::string GetHResultMessage(HRESULT hr);

// Wrapper for HRESULT to be used with fmt.  Note that we can't create a fmt::formatter directly
// for HRESULT as HRESULT is simply a typedef on long and not a distinct type.
struct HRWrap
{
  constexpr explicit HRWrap(HRESULT hr) : m_hr(hr) {}
  const HRESULT m_hr;
};
}  // namespace Common

template <>
struct fmt::formatter<Common::HRWrap>
{
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const Common::HRWrap& hr, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{} ({:#010x})", Common::GetHResultMessage(hr.m_hr),
                          static_cast<u32>(hr.m_hr));
  }
};
