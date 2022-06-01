// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2022, Minty Meeo
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// BitField View
// A C++17 library to create proxy "views" of the underlying bits in an integral value.

#pragma once

#include <cstddef>      // std::size_t
#include <iterator>     // std::input_iterator_tag, std::output_iterator_tag
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::make_unsigned, std::make_signed, std::is_integral, std::is_enum

#include "Common/Assert.h"  // DEBUG_ASSERT

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class BitFieldFixedView;

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class ConstBitFieldFixedView;

template <typename host_t_, typename field_t_>
class BitFieldView;

template <typename host_t_, typename field_t_>
class ConstBitFieldView;

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class BitFieldFixedViewArray;

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class ConstBitFieldFixedViewArray;

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class FixedViewArrayIterator;

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class FixedViewArrayConstIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class BitFieldFixedView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr static std::size_t lshift = rshift - start;
  host_t& host;

public:
  BitFieldFixedView() = delete;
  constexpr BitFieldFixedView(host_t& host_) : host(host_){};

  constexpr operator field_t() const { return Get(); }
  constexpr BitFieldFixedView& operator=(const BitFieldFixedView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const
  {
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr void Set(const field_t val)
  {
    constexpr uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;
    host = (host & ~mask) | ((static_cast<host_t>(val) << start) & mask);
  }

  constexpr BitFieldFixedView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitFieldFixedView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr BitFieldFixedView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr BitFieldFixedView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr BitFieldFixedView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldFixedView& operator|=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldFixedView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldFixedView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class ConstBitFieldFixedView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr static std::size_t lshift = rshift - start;
  const host_t& host;

public:
  ConstBitFieldFixedView() = delete;
  constexpr ConstBitFieldFixedView(const host_t& host_) : host(host_){};

  constexpr operator field_t() const { return Get(); }

  constexpr field_t Get() const
  {
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_>
class BitFieldView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  host_t& host;
  const std::size_t start;
  const std::size_t width;

public:
  BitFieldView() = delete;
  constexpr BitFieldView(host_t& host_, const std::size_t start_, const std::size_t width_)
      : host(host_), start(start_), width(width_){};

  constexpr operator field_t() const { return Get(); }
  constexpr BitFieldView& operator=(const BitFieldView& rhs) { return operator=(rhs.Get()); }

  constexpr field_t Get() const
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const std::size_t lshift = rshift - start;
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr void Set(const field_t val)
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;
    host = (host & ~mask) | ((static_cast<host_t>(val) << start) & mask);
  }

  constexpr BitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr BitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr BitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr BitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr BitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr BitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_>
class ConstBitFieldView
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  const host_t& host;
  const std::size_t start;
  const std::size_t width;

public:
  ConstBitFieldView() = delete;
  constexpr ConstBitFieldView(const host_t& host_, const std::size_t start_,
                              const std::size_t width_)
      : host(host_), start(start_), width(width_){};

  constexpr operator field_t() const { return Get(); }

  constexpr const field_t Get() const
  {
    const std::size_t rshift = 8 * sizeof(host_t) - width;
    const std::size_t lshift = rshift - start;
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class BitFieldFixedViewArray
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  using Iterator = FixedViewArrayIterator<host_t, field_t, start, width>;
  using ConstIterator = FixedViewArrayConstIterator<host_t, field_t, start, width>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  host_t& host;

public:
  BitFieldFixedViewArray() = delete;
  constexpr BitFieldFixedViewArray(host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    const std::size_t lshift = rshift - (start + width * idx);
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }
  constexpr void Set(const std::size_t idx, const field_t val)
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    const uhost_t mask = (std::numeric_limits<uhost_t>::max() >> rshift) << (start + width * idx);
    host = (host & ~mask) | ((static_cast<host_t>(val) << (start + width * idx)) & mask);
  }

  constexpr BitFieldView<host_t, field_t> operator[](const std::size_t idx)
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return BitFieldView<host_t, field_t>(host, start + width * idx, width);
  }
  constexpr ConstBitFieldView<host_t, field_t> operator[](const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return ConstBitFieldView<host_t, field_t>(host, start + width * idx, width);
  }

  constexpr Iterator begin() { return Iterator(host, 0); }
  constexpr Iterator end() { return Iterator(host, length); }
  constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(host, length); }
  constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(host, length); }
  constexpr std::size_t Size() const { return length; }
  constexpr std::size_t Length() const { return length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_,
          std::size_t length_>
class ConstBitFieldFixedViewArray
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  using ConstIterator = FixedViewArrayConstIterator<host_t, field_t, start, width>;
  constexpr static std::size_t rshift = 8 * sizeof(host_t) - width;
  const host_t& host;

public:
  ConstBitFieldFixedViewArray() = delete;
  constexpr ConstBitFieldFixedViewArray(const host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    const std::size_t lshift = rshift - (start + width * idx);
    // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
    // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
    if constexpr (std::is_signed_v<field_t>)
      return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
    else
      return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
  }

  constexpr ConstBitFieldView<host_t, field_t> operator[](const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return ConstBitFieldView<host_t, field_t>(host, start + width * idx, width);
  }

  constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(host, length); }
  constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(host, length); }
  constexpr std::size_t Size() const { return length; }
  constexpr std::size_t Length() const { return length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class FixedViewArrayIterator
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = BitFieldView<host_t, field_t>;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  host_t& host;
  std::size_t idx;

public:
  // Required by std::input_or_output_iterator
  constexpr FixedViewArrayIterator() = default;
  // Required by LegacyIterator
  constexpr FixedViewArrayIterator(const FixedViewArrayIterator& other) = default;
  // Required by LegacyIterator
  FixedViewArrayIterator& operator=(const FixedViewArrayIterator& other) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr FixedViewArrayIterator(FixedViewArrayIterator&& other) = default;
  FixedViewArrayIterator& operator=(FixedViewArrayIterator&& other) = default;

  FixedViewArrayIterator(host_t& host_, const std::size_t idx_) : host(host_), idx(idx_) {}

public:
  FixedViewArrayIterator& operator++()
  {
    ++idx;
    return *this;
  }
  FixedViewArrayIterator operator++(int)
  {
    FixedViewArrayIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const { return reference(host, start + width * idx, width); }
  constexpr bool operator==(const FixedViewArrayIterator& other) const { return idx == other.idx; }
  constexpr bool operator!=(const FixedViewArrayIterator& other) const { return idx != other.idx; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename host_t_, typename field_t_, std::size_t start_, std::size_t width_>
class FixedViewArrayConstIterator
{
  // This code is only sane for integral types
  static_assert(std::is_integral_v<host_t_> || std::is_enum_v<host_t_>);
  static_assert(std::is_integral_v<field_t_> || std::is_enum_v<field_t_>);

public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = ConstBitFieldView<host_t, field_t>;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  const host_t& host;
  std::size_t idx;

public:
  // Required by std::input_or_output_iterator
  constexpr FixedViewArrayConstIterator() = default;
  // Required by LegacyIterator
  constexpr FixedViewArrayConstIterator(const FixedViewArrayConstIterator& other) = default;
  // Required by LegacyIterator
  FixedViewArrayConstIterator& operator=(const FixedViewArrayConstIterator& other) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr FixedViewArrayConstIterator(FixedViewArrayConstIterator&& other) = default;
  FixedViewArrayConstIterator& operator=(FixedViewArrayConstIterator&& other) = default;

  FixedViewArrayConstIterator(const host_t& host_, const std::size_t idx_) : host(host_), idx(idx_)
  {
  }

public:
  FixedViewArrayConstIterator& operator++()
  {
    ++idx;
    return *this;
  }
  FixedViewArrayConstIterator operator++(int)
  {
    FixedViewArrayConstIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const { return reference(host, start + width * idx, width); }
  constexpr bool operator==(const FixedViewArrayConstIterator other) const
  {
    return idx == other.idx;
  }
  constexpr bool operator!=(const FixedViewArrayConstIterator other) const
  {
    return idx != other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// Mix up the template argument order on purpose to allow implicit deduction of host_t.  Even C++17
// cannot deduce trailing template parameters for constructors.  For them, it's all or nothing.
// Functions, however, can.

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr BitFieldFixedView<host_t, field_t, start, width>  //
MakeBitFieldFixedView(host_t& host)
{
  return BitFieldFixedView<host_t, field_t, start, width>(host);
}
template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr ConstBitFieldFixedView<host_t, field_t, start, width>  //
MakeConstBitFieldFixedView(const host_t& host)
{
  return ConstBitFieldFixedView<host_t, field_t, start, width>(host);
}
template <typename field_t, typename host_t>
constexpr BitFieldView<host_t, field_t>  //
MakeBitfieldView(host_t& host, const std::size_t start, const std::size_t width)
{
  return BitFieldView<host_t, field_t>(host, start, width);
}
template <typename field_t, typename host_t>
constexpr ConstBitFieldView<host_t, field_t>  //
MakeConstBitFieldView(const host_t& host, const std::size_t start, const std::size_t width)
{
  return ConstBitFieldView<host_t, field_t>(host, start, width);
}
template <typename field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr BitFieldFixedViewArray<host_t, field_t, start, width, length>  //
MakeBitFieldFixedViewArray(host_t& host)
{
  return BitFieldFixedViewArray<host_t, field_t, start, width, length>(host);
}
template <typename field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr ConstBitFieldFixedViewArray<host_t, field_t, start, width, length>  //
MakeConstBitFieldFixedViewArray(const host_t& host)
{
  return ConstBitFieldFixedViewArray<host_t, field_t, start, width, length>(host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// BitField View (Array) in Member?  BitField View (Array) Macro?  BitField View (Array) Method?
// You can believe whatever you want to believe this abbreviation is short for.

#define BFVIEW_M(host, field_t, start, width, name)                                                \
  constexpr BitFieldFixedView<decltype(host), field_t, start, width> name()                        \
  {                                                                                                \
    return BitFieldFixedView<decltype(host), field_t, start, width>(host);                         \
  }                                                                                                \
  constexpr ConstBitFieldFixedView<decltype(host), field_t, start, width> name() const             \
  {                                                                                                \
    return ConstBitFieldFixedView<decltype(host), field_t, start, width>(host);                    \
  }

#define BFVIEWARRAY_M(host, field_t, start, width, length, name)                                   \
  constexpr BitFieldFixedViewArray<decltype(host), field_t, start, width, length> name()           \
  {                                                                                                \
    return BitFieldFixedViewArray<decltype(host), field_t, start, width, length>(host);            \
  }                                                                                                \
  constexpr ConstBitFieldFixedViewArray<decltype(host), field_t, start, width, length> name()      \
      const                                                                                        \
  {                                                                                                \
    return ConstBitFieldFixedViewArray<decltype(host), field_t, start, width, length>(host);       \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <fmt/format.h>

template <typename host_t, typename field_t, std::size_t start, std::size_t width>
struct fmt::formatter<BitFieldFixedView<host_t, field_t, start, width>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldFixedView<host_t, field_t, start, width>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t, std::size_t start, std::size_t width>
struct fmt::formatter<ConstBitFieldFixedView<host_t, field_t, start, width>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const ConstBitFieldFixedView<host_t, field_t, start, width>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t>
struct fmt::formatter<BitFieldView<host_t, field_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldView<host_t, field_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename host_t, typename field_t>
struct fmt::formatter<ConstBitFieldView<host_t, field_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const ConstBitFieldView<host_t, field_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
