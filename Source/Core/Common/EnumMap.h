// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <type_traits>

#include "Common/TypeUtils.h"

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
class BitFieldFixedView;
template <typename field_t, std::size_t start, std::size_t width, typename host_t>
class ConstBitFieldFixedView;
template <typename field_t, typename host_t>
class BitFieldView;
template <typename field_t, typename host_t>
class ConstBitFieldView;

namespace Common
{
// A type that allows lookup of values associated with an enum as the key.
// Designed for enums whose numeric values start at 0 and increment continuously with few gaps.
template <typename V, auto last_member, typename = decltype(last_member)>
class EnumMap final
{
  // The third template argument is needed to avoid compile errors from ambiguity with multiple
  // enums with the same number of members in GCC prior to 8.  See https://godbolt.org/z/xcKaW1seW
  // and https://godbolt.org/z/hz7Yqq1P5
  using T = decltype(last_member);
  static_assert(std::is_enum_v<T>);
  static constexpr size_t s_size = static_cast<size_t>(last_member) + 1;

  using array_type = std::array<V, s_size>;
  using iterator = typename array_type::iterator;
  using const_iterator = typename array_type::const_iterator;

public:
  constexpr EnumMap() = default;
  constexpr EnumMap(const EnumMap& other) = default;
  constexpr EnumMap& operator=(const EnumMap& other) = default;
  constexpr EnumMap(EnumMap&& other) = default;
  constexpr EnumMap& operator=(EnumMap&& other) = default;

  // Constructor that accepts exactly size Vs (enforcing that all must be specified).
  template <typename... T, typename = std::enable_if_t<Common::IsNOf<V, s_size, T...>::value>>
  constexpr EnumMap(T... values) : m_array{static_cast<V>(values)...}
  {
  }

  constexpr const V& operator[](T key) const { return m_array[static_cast<std::size_t>(key)]; }
  constexpr V& operator[](T key) { return m_array[static_cast<std::size_t>(key)]; }

  // These only exist to perform the safety check; without them, BitFieldView's implicit conversion
  // would work (but since BitFieldView is used for game-generated data, we need to be careful about
  // bounds-checking)
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr const V& operator[](BitFieldFixedView<field_t, start, width, T> key) const
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr V& operator[](BitFieldFixedView<field_t, start, width, T> key)
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr const V& operator[](ConstBitFieldFixedView<field_t, start, width, T> key) const
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr V& operator[](ConstBitFieldFixedView<field_t, start, width, T> key)
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr const V& operator[](BitFieldView<field_t, T> key) const
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr V& operator[](BitFieldView<field_t, T> key)
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr const V& operator[](ConstBitFieldView<field_t, T> key) const
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }
  template <typename field_t, std::size_t start, std::size_t width>
  constexpr V& operator[](ConstBitFieldView<field_t, T> key)
  {
    static_assert(1 << width == s_size, "Unsafe indexing into EnumMap (may go out of bounds)");
    return m_array[static_cast<std::size_t>(key.Get())];
  }

  constexpr bool InBounds(T key) const { return static_cast<std::size_t>(key) < s_size; }

  constexpr size_t size() const noexcept { return s_size; }

  constexpr V* data() { return m_array.data(); }
  constexpr const V* data() const { return m_array.data(); }

  constexpr iterator begin() { return m_array.begin(); }
  constexpr iterator end() { return m_array.end(); }
  constexpr const_iterator begin() const { return m_array.begin(); }
  constexpr const_iterator end() const { return m_array.end(); }
  constexpr const_iterator cbegin() const { return m_array.cbegin(); }
  constexpr const_iterator cend() const { return m_array.cend(); }

  constexpr void fill(const V& v) { m_array.fill(v); }

private:
  array_type m_array{};
};
}  // namespace Common
