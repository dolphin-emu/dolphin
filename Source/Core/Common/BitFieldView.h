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

// BitFieldView.h - A C++20 library

#pragma once

#include <bit>          // std::bit_cast
#include <concepts>     // std::integral
#include <cstddef>      // std::size_t
#include <cstdint>      // std::uint32_t, std::uint64_t
#include <iterator>     // std::input_iterator_tag, std::output_iterator_tag
#include <type_traits>  // std::is_integral, std::is_enum

#include "Common/Assert.h"    // DEBUG_ASSERT
#include "Common/BitUtils.h"  // Common::ExtractBit, Common::ExtractBitsU, Common::ExtractBitsS,
                              // Common::InsertBit, Common::InsertBits, Common::BitSize

namespace Common
{
template <typename T>
concept SaneBitFieldType = requires
{
  std::is_integral_v<T> || std::is_enum_v<T>;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Intermediary step between BitFieldView classes and BitUtil functions.  This step requires the
// host_t to satisfy std::integral.  If it does not, you must define a partial specialization for
// that host type (see: float and double specializations).  It is recommended that any type which
// must exist in memory (like a wrapper type) always be passed by reference.

template <SaneBitFieldType field_t, std::size_t start, std::size_t width, std::integral host_t>
constexpr field_t GetBitFieldFixed(const host_t host) noexcept
{
  if constexpr (std::is_same_v<field_t, bool>)
  {
    static_assert(width == 1, "Boolean fields should only be 1 bit wide");
    return static_cast<field_t>(ExtractBit<start>(host));
  }
  else if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS<start, width>(host));
  else
    return static_cast<field_t>(ExtractBitsU<start, width>(host));
}

template <SaneBitFieldType field_t, std::integral host_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const host_t host) noexcept
{
  if constexpr (std::is_same_v<field_t, bool>)
  {
    DEBUG_ASSERT(width == 1);  // Boolean fields should only be 1 bit wide
    return static_cast<field_t>(ExtractBit(start, host));
  }
  else if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS(start, width, host));
  else
    return static_cast<field_t>(ExtractBitsU(start, width, host));
}

template <SaneBitFieldType field_t, std::size_t start, std::size_t width, std::integral host_t>
constexpr void SetBitFieldFixed(host_t& host, const field_t val) noexcept
{
  if constexpr (std::is_same_v<field_t, bool>)
  {
    static_assert(width == 1, "Boolean fields should only be 1 bit wide");
    InsertBit<start>(host, val);
  }
  else
    InsertBits<start, width>(host, val);
}

template <SaneBitFieldType field_t, std::integral host_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, host_t& host,
                           const field_t val) noexcept
{
  if constexpr (std::is_same_v<field_t, bool>)
  {
    DEBUG_ASSERT(width == 1);  // Boolean fields should only be 1 bit wide
    InsertBit(start, host, val);
  }
  else
    InsertBits(start, width, host, val);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Float host type specialization

static_assert(sizeof(float) == sizeof(std::uint32_t));

template <SaneBitFieldType field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const float& host) noexcept
{
  const std::uint32_t hostbits = std::bit_cast<std::uint32_t>(host);
  return GetBitFieldFixed<field_t, start, width>(hostbits);
}

template <SaneBitFieldType field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const float& host) noexcept
{
  const std::uint32_t hostbits = std::bit_cast<std::uint32_t>(host);
  return GetBitField<field_t>(start, width, hostbits);
}

template <SaneBitFieldType field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(float& host, const field_t val) noexcept
{
  std::uint32_t hostbits = std::bit_cast<std::uint32_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = std::bit_cast<float>(hostbits);
}

template <SaneBitFieldType field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, float& host,
                           const field_t val) noexcept
{
  std::uint32_t hostbits = std::bit_cast<std::uint32_t>(host);
  SetBitField<field_t>(start, width, hostbits);
  host = std::bit_cast<float>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Double host type specialization

static_assert(sizeof(double) == sizeof(std::uint64_t));

template <SaneBitFieldType field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const double& host) noexcept
{
  const std::uint64_t hostbits = std::bit_cast<std::uint64_t>(host);
  return GetBitFieldFixed<field_t, start, width>(hostbits);
}

template <SaneBitFieldType field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const double& host) noexcept
{
  const std::uint64_t hostbits = std::bit_cast<std::uint64_t>(host);
  return GetBitField<field_t>(start, width, hostbits);
}

template <SaneBitFieldType field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(double& host, const field_t val) noexcept
{
  std::uint64_t hostbits = std::bit_cast<std::uint64_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = std::bit_cast<double>(hostbits);
}

template <SaneBitFieldType field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, double& host,
                           const field_t val) noexcept
{
  std::uint64_t hostbits = std::bit_cast<std::uint64_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = std::bit_cast<double>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class BitFieldFixedView;

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class ConstBitFieldFixedView;

template <SaneBitFieldType field_t_, typename host_t_>
class BitFieldView;

template <SaneBitFieldType field_t_, typename host_t_>
class ConstBitFieldView;

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class BitFieldFixedViewArray;

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class ConstBitFieldFixedViewArray;

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayIterator;

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayConstIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Even C++17 cannot partially deduce template parameters for constructors.  For them, it's all or
// nothing.  Functions, however, can, so we should define some pre-C++17-esque helper functions.

template <SaneBitFieldType field_t, std::size_t start, std::size_t width, typename host_t>
constexpr BitFieldFixedView<field_t, start, width, host_t>  //
MakeBitFieldFixedView(host_t& host)
{
  return BitFieldFixedView<field_t, start, width, host_t>(host);
}
template <SaneBitFieldType field_t, std::size_t start, std::size_t width, typename host_t>
constexpr ConstBitFieldFixedView<field_t, start, width, host_t>  //
MakeConstBitFieldFixedView(const host_t& host)
{
  return ConstBitFieldFixedView<field_t, start, width, host_t>(host);
}
template <SaneBitFieldType field_t, typename host_t>
constexpr BitFieldView<field_t, host_t>  //
MakeBitFieldView(const std::size_t start, const std::size_t width, host_t& host)
{
  return BitFieldView<field_t, host_t>(start, width, host);
}
template <SaneBitFieldType field_t, typename host_t>
constexpr ConstBitFieldView<field_t, host_t>  //
MakeConstBitFieldView(const std::size_t start, const std::size_t width, const host_t& host)
{
  return ConstBitFieldView<field_t, host_t>(start, width, host);
}
template <SaneBitFieldType field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr BitFieldFixedViewArray<field_t, start, width, length, host_t>  //
MakeBitFieldFixedViewArray(host_t& host)
{
  return BitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}
template <SaneBitFieldType field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr ConstBitFieldFixedViewArray<field_t, start, width, length, host_t>  //
MakeConstBitFieldFixedViewArray(const host_t& host)
{
  return ConstBitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class BitFieldFixedView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  host_t& host;

public:
  BitFieldFixedView() = delete;
  constexpr BitFieldFixedView(host_t& host_) : host(host_){};

  constexpr field_t Get() const { return GetBitFieldFixed<field_t, start, width>(host); }
  constexpr void Set(const field_t val) { SetBitFieldFixed<field_t, start, width>(host, val); }

  constexpr BitFieldFixedView& operator=(const BitFieldFixedView& rhs)
  {
    return operator=(rhs.Get());
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
  constexpr BitFieldFixedView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr BitFieldFixedView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldFixedView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class ConstBitFieldFixedView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

private:
  const host_t& host;

public:
  ConstBitFieldFixedView() = delete;
  constexpr ConstBitFieldFixedView(const host_t& host_) : host(host_){};

  constexpr field_t Get() const { return GetBitFieldFixed<field_t, start, width>(host); }

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, typename host_t_>
class BitFieldView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  const std::size_t start;
  const std::size_t width;
  host_t& host;

public:
  BitFieldView() = delete;
  constexpr BitFieldView(const std::size_t start_, const std::size_t width_, host_t& host_)
      : start(start_), width(width_), host(host_){};

  constexpr field_t Get() const { return GetBitField<field_t>(start, width, host); }
  constexpr void Set(const field_t val) { return SetBitField<field_t>(start, width, host, val); }

  constexpr BitFieldView& operator=(const BitFieldView& rhs) { return operator=(rhs.Get()); }
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

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, typename host_t_>
class ConstBitFieldView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;

private:
  const std::size_t start;
  const std::size_t width;
  const host_t& host;

public:
  ConstBitFieldView() = delete;
  constexpr ConstBitFieldView(const std::size_t start_, const std::size_t width_,
                              const host_t& host_)
      : start(start_), width(width_), host(host_){};

  constexpr field_t Get() const { return GetBitField<field_t>(start, width, host); }

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class BitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

  static_assert(width > 0, "BitFieldViewArray is invalid (zero width)");
  static_assert(length > 0, "BitFieldViewArray is invalid (zero length)");
  // These static assertions are normally the job of fixed variant Get/SetBitField functions.
  // BitFieldViewArrays never run into those functions, though, which could lead to nasty bugs.
  static_assert(!std::is_same_v<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
  static_assert(!std::is_arithmetic_v<host_t> || start + width * length <= BitSize<host_t>(),
                "BitFieldViewArray out of range");

private:
  using Iterator = FixedViewArrayIterator<field_t, start, width, host_t>;
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;
  host_t& host;

public:
  BitFieldFixedViewArray() = delete;
  constexpr BitFieldFixedViewArray(host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return GetBitField<field_t>(start + width * idx, width, host);
  }
  constexpr void Set(const std::size_t idx, const field_t val)
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    SetBitField<field_t>(start + width * idx, width, host, val);
  }

  constexpr BitFieldView<field_t, host_t> operator[](const std::size_t idx)
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return MakeBitFieldView<field_t>(start + width * idx, width, host);
  }
  constexpr ConstBitFieldView<field_t, host_t> operator[](const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return MakeConstBitFieldView<field_t>(start + width * idx, width, host);
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

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class ConstBitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

  static_assert(width > 0, "BitFieldViewArray is invalid (zero width)");
  static_assert(length > 0, "BitFieldViewArray is invalid (zero length)");
  // These static assertions are normally the job of fixed variant Get/SetBitField functions.
  // BitFieldViewArrays never run into those functions, though, which could lead to nasty bugs.
  static_assert(!std::is_same_v<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
  static_assert(!std::is_arithmetic_v<host_t> || start + width * length <= BitSize<host_t>(),
                "BitFieldViewArray out of range");

private:
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;
  const host_t& host;

public:
  ConstBitFieldFixedViewArray() = delete;
  constexpr ConstBitFieldFixedViewArray(const host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return GetBitField<field_t>(start + width * idx, width, host);
  }

  constexpr ConstBitFieldView<field_t, host_t> operator[](const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return MakeConstBitFieldView<field_t>(start + width * idx, width, host);
  }

  constexpr ConstIterator begin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(host, length); }
  constexpr ConstIterator cbegin() const { return ConstIterator(host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(host, length); }
  constexpr std::size_t Size() const { return length; }
  constexpr std::size_t Length() const { return length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayIterator
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = BitFieldView<field_t, host_t>;
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
  constexpr reference operator*() const { return reference(start + width * idx, width, host); }
  constexpr bool operator==(const FixedViewArrayIterator& other) const { return idx == other.idx; }
  constexpr bool operator!=(const FixedViewArrayIterator& other) const { return idx != other.idx; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <SaneBitFieldType field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayConstIterator
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = ConstBitFieldView<field_t, host_t>;
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
  constexpr reference operator*() const { return reference(start + width * idx, width, host); }
  constexpr bool operator==(const FixedViewArrayConstIterator other) const
  {
    return idx == other.idx;
  }
  constexpr bool operator!=(const FixedViewArrayConstIterator other) const
  {
    return idx != other.idx;
  }
};
};  // namespace Common

///////////////////////////////////////////////////////////////////////////////////////////////////
// Easily create BitFieldView(Array) methods in a class which target a given host member.
//
// class Example
// {
//   u32 hex_a;
//   u32 hex_b;
//
//   BFVIEW_IN     (hex_a, u32, 3, 3,    arbitrary_field);
//   BFVIEWARRAY_IN(hex_b, u32, 9, 2, 5, arbitrary_field_array);
// };

#define BFVIEW_IN(host, field_t, start, width, name)                                               \
  constexpr auto name() /*                                                                      */ \
  {                                                                                                \
    return ::Common::MakeBitFieldFixedView<field_t, start, width>(host);                           \
  }                                                                                                \
  constexpr auto name() const /*                                                                */ \
  {                                                                                                \
    return ::Common::MakeConstBitFieldFixedView<field_t, start, width>(host);                      \
  }

#define BFVIEWARRAY_IN(host, field_t, start, width, length, name)                                  \
  constexpr auto name() /*                                                                      */ \
  {                                                                                                \
    return ::Common::MakeBitFieldFixedViewArray<field_t, start, width, length>(host);              \
  }                                                                                                \
  constexpr auto name() const /*                                                                */ \
  {                                                                                                \
    return ::Common::MakeConstBitFieldFixedViewArray<field_t, start, width, length>(host);         \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Automagically ascertain the storage of a single-member class via a structured binding.
//
// class Example
// {
//   u32 hex;
//
//   BFVIEW     (u32, 3, 3,    arbitrary_field);
//   BFVIEWARRAY(u32, 9, 2, 5, arbitrary_field_array);
// };

#define BFVIEW(field_t, start, width, name)                                                        \
  constexpr auto name() /*                                                                      */ \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeBitFieldFixedView<field_t, start, width>(host);                           \
  }                                                                                                \
  constexpr auto name() const /*                                                                */ \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeConstBitFieldFixedView<field_t, start, width>(host);                      \
  }

#define BFVIEWARRAY(field_t, start, width, length, name)                                           \
  constexpr auto name() /*                                                                      */ \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeBitFieldFixedViewArray<field_t, start, width, length>(host);              \
  }                                                                                                \
  constexpr auto name() const /*                                                                */ \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeConstBitFieldFixedViewArray<field_t, start, width, length>(host);         \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <fmt/format.h>

template <Common::SaneBitFieldType field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<Common::BitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::BitFieldFixedView<field_t, start, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::SaneBitFieldType field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<Common::ConstBitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::ConstBitFieldFixedView<field_t, start, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::SaneBitFieldType field_t, typename host_t>
struct fmt::formatter<Common::BitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::BitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::SaneBitFieldType field_t, typename host_t>
struct fmt::formatter<Common::ConstBitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::ConstBitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
