// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
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
 *   formatter() : EnumFormatter({"A", "B", "C"}) {}
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
 *   formatter() : EnumFormatter(names) {}
 * };
 */
template <auto last_member, typename T = decltype(last_member),
          size_t size = static_cast<size_t>(last_member) + 1,
          std::enable_if_t<std::is_enum_v<T>, bool> = true>
class EnumFormatter
{
public:
  constexpr auto parse(fmt::format_parse_context& ctx)
  {
    auto it = ctx.begin(), end = ctx.end();
    // 'u' for user display, 's' for shader generation
    if (it != end && (*it == 'u' || *it == 's'))
      formatting_for_shader = (*it++ == 's');
    return it;
  }

  template <typename FormatContext>
  auto format(const T& e, FormatContext& ctx)
  {
    const auto value = static_cast<std::underlying_type_t<T>>(e);

    if (!formatting_for_shader)
    {
      if (value >= 0 && value < size && m_names[value] != nullptr)
        return fmt::format_to(ctx.out(), "{} ({})", m_names[value], value);
      else
        return fmt::format_to(ctx.out(), "Invalid ({})", value);
    }
    else
    {
      if (value >= 0 && value < size && m_names[value] != nullptr)
        return fmt::format_to(ctx.out(), "{:#x}u /* {} */", value, m_names[value]);
      else
        return fmt::format_to(ctx.out(), "{:#x}u /* Invalid */",
                              static_cast<std::make_unsigned_t<T>>(value));
    }
  }

protected:
  // This is needed because std::array deduces incorrectly if nullptr is included in the list
  using array_type = std::array<const char*, size>;

  constexpr explicit EnumFormatter(const array_type names) : m_names(std::move(names)) {}

private:
  const array_type m_names;
  bool formatting_for_shader = false;
};
