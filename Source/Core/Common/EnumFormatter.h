// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/EnumMap.h"

#include <fmt/format.h>
#include <type_traits>

/*
 * Helper for using enums with fmt.
 *
 * Usage example:
 *
 * enum class Foo
 * {
 *   A = 0,
 *   B = 1,
 *   C = 2,
 * };
 *
 * template <>
 * struct fmt::formatter<Foo> : EnumFormatter<Foo::C>
 * {
 *   constexpr formatter() : EnumFormatter({"A", "B", "C"}) {}
 * };
 *
 * enum class Bar
 * {
 *   D = 0,
 *   E = 1,
 *   F = 3,
 * };
 *
 * template <>
 * struct fmt::formatter<Bar> : EnumFormatter<Bar::F>
 * {
 *   // using std::array here fails due to nullptr not being const char*, at least in MSVC
 *   // (but only when a field is used; directly in the constructor is OK)
 *   static constexpr array_type names = {"D", "E", nullptr, "F"};
 *   constexpr formatter() : EnumFormatter(names) {}
 * };
 */
template <auto last_member>
class EnumFormatter
{
  using T = decltype(last_member);
  static_assert(std::is_enum_v<T>);

public:
  constexpr auto parse(fmt::format_parse_context& ctx)
  {
    auto it = ctx.begin(), end = ctx.end();
    // 'u' for user display, 's' for shader generation, 'n' for name only
    if (it != end && (*it == 'u' || *it == 's' || *it == 'n'))
      format_type = *it++;
    return it;
  }

  template <typename FormatContext>
  auto format(const T& e, FormatContext& ctx) const
  {
    const auto value_s = static_cast<std::underlying_type_t<T>>(e);      // Possibly signed
    const auto value_u = static_cast<std::make_unsigned_t<T>>(value_s);  // Always unsigned
    const bool has_name = m_names.InBounds(e) && m_names[e] != nullptr;

    switch (format_type)
    {
    default:
    case 'u':
      if (has_name)
        return fmt::format_to(ctx.out(), "{} ({})", m_names[e], value_s);
      else
        return fmt::format_to(ctx.out(), "Invalid ({})", value_s);
    case 's':
      if (has_name)
        return fmt::format_to(ctx.out(), "{:#x}u /* {} */", value_u, m_names[e]);
      else
        return fmt::format_to(ctx.out(), "{:#x}u /* Invalid */", value_u);
    case 'n':
      if (has_name)
        return fmt::format_to(ctx.out(), "{}", m_names[e]);
      else
        return fmt::format_to(ctx.out(), "Invalid ({})", value_s);
    }
  }

protected:
  // This is needed because std::array deduces incorrectly if nullptr is included in the list
  using array_type = Common::EnumMap<const char*, last_member>;

  constexpr explicit EnumFormatter(const array_type names) : m_names(std::move(names)) {}

  const array_type m_names;
  char format_type = 'u';
};
