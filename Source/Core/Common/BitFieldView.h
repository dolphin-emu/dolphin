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

#include <cstddef>      // std::size_t
#include <cstdint>      // std::uint32_t, std::uint64_t
#include <iterator>     // std::input_iterator_tag, std::output_iterator_tag
#include <type_traits>  // std::is_signed

#include "Common/Assert.h"    // DEBUG_ASSERT
#include "Common/BitUtils.h"  // Common::ExtractBit, Common::ExtractBitsU, Common::ExtractBitsS,
                              // Common::InsertBit, Common::InsertBits, Common::BitSize
                              // Common::BitCast
#include "Common/Concepts.h"  // Common::IntegralOrEnum, Common::SameAsOrUnderlying,
                              // Common::Integral, Common::Arithmetic

namespace Common
{
///////////////////////////////////////////////////////////////////////////////////////////////////
// Intermediary step between BitFieldView classes and BitUtil functions.  This step requires the
// host_t to satisfy std::integral.  If it does not, you must define a partial specialization for
// that host type (see: float and double specializations).  It is recommended that any type which
// must exist in memory (like a wrapper type) always be passed by reference.

// TODO: Decide what RequiresClausePosition and IndentRequiresClause should be once Clang-Format 15
// is more widespread.
// clang-format off

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, Integral host_t>
requires(!SameAsOrUnderlying<field_t, bool>)
constexpr field_t GetBitFieldFixed(const host_t host) noexcept
{
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS<start, width>(host));
  else
    return static_cast<field_t>(ExtractBitsU<start, width>(host));
}

template <IntegralOrEnum field_t, Integral host_t>
requires(!SameAsOrUnderlying<field_t, bool>)
constexpr field_t GetBitField(const std::size_t start, const std::size_t width, const host_t host) noexcept
{
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS(start, width, host));
  else
    return static_cast<field_t>(ExtractBitsU(start, width, host));
}

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, Integral host_t>
requires(!SameAsOrUnderlying<field_t, bool>)
constexpr void SetBitFieldFixed(host_t& host, const field_t val) noexcept
{
  InsertBits<start, width>(host, val);
}

template <IntegralOrEnum field_t, Integral host_t>
requires(!SameAsOrUnderlying<field_t, bool>)
constexpr void SetBitField(const std::size_t start, const std::size_t width, host_t& host,
                           const field_t val) noexcept
{
  InsertBits(start, width, host, val);
}

// clang-format on

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bool or enumeration of underlying type bool field type overload (for optimization)

template <SameAsOrUnderlying<bool> field_t, std::size_t start, std::size_t width, Integral host_t>
constexpr field_t GetBitFieldFixed(const host_t& host) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  return static_cast<field_t>(ExtractBit<start>(host));
}

template <SameAsOrUnderlying<bool> field_t, Integral host_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const host_t& host) noexcept
{
  DEBUG_ASSERT(width == 1);  // Boolean fields should only be 1 bit wide
  return static_cast<field_t>(ExtractBit(start, host));
}

template <SameAsOrUnderlying<bool> field_t, std::size_t start, std::size_t width, Integral host_t>
constexpr void SetBitFieldFixed(host_t& host, const field_t val) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  InsertBit<start>(host, static_cast<bool>(val));
}

template <SameAsOrUnderlying<bool> field_t, Integral host_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, host_t& host,
                           const field_t val) noexcept
{
  DEBUG_ASSERT(width == 1);  // Boolean fields should only be 1 bit wide
  InsertBit(start, host, static_cast<bool>(val));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Float host type specialization

static_assert(sizeof(float) == sizeof(std::uint32_t));

template <IntegralOrEnum field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const float& host) noexcept
{
  const std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  return GetBitFieldFixed<field_t, start, width>(hostbits);
}

template <IntegralOrEnum field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const float& host) noexcept
{
  const std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  return GetBitField<field_t>(start, width, hostbits);
}

template <IntegralOrEnum field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(float& host, const field_t val) noexcept
{
  std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = BitCast<float>(hostbits);
}

template <IntegralOrEnum field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, float& host,
                           const field_t val) noexcept
{
  std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  SetBitField<field_t>(start, width, hostbits);
  host = BitCast<float>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Double host type specialization

static_assert(sizeof(double) == sizeof(std::uint64_t));

template <IntegralOrEnum field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const double& host) noexcept
{
  const std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  return GetBitFieldFixed<field_t, start, width>(hostbits);
}

template <IntegralOrEnum field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width,
                              const double& host) noexcept
{
  const std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  return GetBitField<field_t>(start, width, hostbits);
}

template <IntegralOrEnum field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(double& host, const field_t val) noexcept
{
  std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = BitCast<double>(hostbits);
}

template <IntegralOrEnum field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, double& host,
                           const field_t val) noexcept
{
  std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  SetBitFieldFixed<field_t, start, width>(hostbits);
  host = BitCast<double>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
class BitFieldFixedView;
template <Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
class EnumBitFieldFixedView;

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
class ConstBitFieldFixedView;
template <Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
class EnumConstBitFieldFixedView;

template <IntegralOrEnum field_t, typename host_t>
class BitFieldView;
template <Enumerated field_t, typename host_t>
class EnumBitFieldView;

template <IntegralOrEnum field_t, typename host_t>
class ConstBitFieldView;
template <Enumerated field_t, typename host_t>
class EnumConstBitFieldView;

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
class BitFieldFixedViewArray;
template <Enumerated field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
class EnumBitFieldFixedViewArray;

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t_>
class ConstBitFieldFixedViewArray;
template <Enumerated field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t_>
class EnumConstBitFieldFixedViewArray;

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
class FixedViewArrayIterator;
template <Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
class EnumFixedViewArrayIterator;

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
class FixedViewArrayConstIterator;
template <Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
class EnumFixedViewArrayConstIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Define some pre-C++17-esque helper functions.  We do this because:
//  - Even as of C++17, one cannot partially deduce template parameters for constructors.  For them,
//    it's all or nothing.
//  - This abstraction seamlessly guides the end-user to the syntax-saving enum variants when
//    appropriate.  The non-enum variants cannot prevent the end-user from using an enumerated type,
//    as they are the base class of their own enum variant.

template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
constexpr auto MakeBitFieldFixedView(host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumBitFieldFixedView<field_t, start, width, host_t>(host);
  else
    return BitFieldFixedView<field_t, start, width, host_t>(host);
}
template <IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
constexpr auto MakeConstBitFieldFixedView(const host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumConstBitFieldFixedView<field_t, start, width, host_t>(host);
  else
    return ConstBitFieldFixedView<field_t, start, width, host_t>(host);
}
template <IntegralOrEnum field_t, typename host_t>
constexpr auto MakeBitFieldView(const std::size_t start, const std::size_t width, host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumBitFieldView<field_t, host_t>(start, width, host);
  else
    return BitFieldView<field_t, host_t>(start, width, host);
}
template <IntegralOrEnum field_t, typename host_t>
constexpr auto MakeConstBitFieldView(const std::size_t start, const std::size_t width,
                                     const host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumConstBitFieldView<field_t, host_t>(start, width, host);
  else
    return ConstBitFieldView<field_t, host_t>(start, width, host);
}
template <IntegralOrEnum field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr auto MakeBitFieldFixedViewArray(host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumBitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
  else
    return BitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}
template <IntegralOrEnum field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr auto MakeConstBitFieldFixedViewArray(const host_t& host)
{
  if constexpr (std::is_enum_v<field_t>)
    return EnumConstBitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
  else
    return ConstBitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class BitFieldFixedView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

protected:
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

template <Enumerated field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class EnumBitFieldFixedView : public BitFieldFixedView<field_t_, start_, width_, host_t_>
{
public:
  EnumBitFieldFixedView() = delete;
  constexpr EnumBitFieldFixedView(host_t_& host_)
      : BitFieldFixedView<field_t_, start_, width_, host_t_>(host_){};

  constexpr EnumBitFieldFixedView& operator=(const EnumBitFieldFixedView& rhs)
  {
    return operator=(rhs.Get());
  }
  constexpr EnumBitFieldFixedView& operator=(const field_t_ rhs)
  {
    this->Set(rhs);
    return *this;
  }
  using underlying_field_t = std::underlying_type_t<field_t_>;
  constexpr explicit operator underlying_field_t() const
  {
    return static_cast<underlying_field_t>(this->Get());
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class ConstBitFieldFixedView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;

protected:
  const host_t& host;

public:
  ConstBitFieldFixedView() = delete;
  constexpr ConstBitFieldFixedView(const host_t& host_) : host(host_){};

  constexpr field_t Get() const { return GetBitFieldFixed<field_t, start, width>(host); }

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

template <Enumerated field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class EnumConstBitFieldFixedView : public ConstBitFieldFixedView<field_t_, start_, width_, host_t_>
{
public:
  EnumConstBitFieldFixedView() = delete;
  constexpr EnumConstBitFieldFixedView(const host_t_& host_)
      : ConstBitFieldFixedView<field_t_, start_, width_, host_t_>(host_){};

  using underlying_field_t = std::underlying_type_t<field_t_>;
  constexpr explicit operator underlying_field_t() const
  {
    return static_cast<underlying_field_t>(this->Get());
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, typename host_t_>
class BitFieldView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  const std::size_t start;
  const std::size_t width;

protected:
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

template <Enumerated field_t_, typename host_t_>
class EnumBitFieldView : public BitFieldView<field_t_, host_t_>
{
public:
  EnumBitFieldView() = delete;
  constexpr EnumBitFieldView(const std::size_t start_, const std::size_t width_, host_t_& host_)
      : BitFieldView<field_t_, host_t_>(start_, width_, host_){};

  constexpr EnumBitFieldView& operator=(const EnumBitFieldView& rhs)
  {
    return operator=(rhs.Get());
  }
  constexpr EnumBitFieldView& operator=(const field_t_ rhs)
  {
    this->Set(rhs);
    return *this;
  }
  using underlying_field_t = std::underlying_type_t<field_t_>;
  constexpr explicit operator underlying_field_t() const
  {
    return static_cast<underlying_field_t>(this->Get());
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, typename host_t_>
class ConstBitFieldView
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  const std::size_t start;
  const std::size_t width;

protected:
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

template <Enumerated field_t_, typename host_t_>
class EnumConstBitFieldView : public ConstBitFieldView<field_t_, host_t_>
{
public:
  EnumConstBitFieldView() = delete;
  constexpr EnumConstBitFieldView(const std::size_t start_, const std::size_t width_,
                                  const host_t_& host_)
      : ConstBitFieldView<field_t_, host_t_>(start_, width_, host_){};

  using underlying_field_t = std::underlying_type_t<field_t_>;
  constexpr explicit operator underlying_field_t() const
  {
    return static_cast<underlying_field_t>(this->Get());
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class BitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

protected:
  host_t& host;

private:
  using Iterator = FixedViewArrayIterator<field_t, start, width, host_t>;
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;

  static_assert(width > 0, "BitFieldViewArray is invalid (zero width)");
  static_assert(length > 0, "BitFieldViewArray is invalid (zero length)");
  // These static assertions are normally the job of fixed variant Get/SetBitField functions.
  // BitFieldViewArrays never run into those functions, though, which could lead to nasty bugs.
  static_assert(!SameAsOrUnderlying<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
  static_assert(!Arithmetic<host_t> || start + width * length <= BitSize<host_t>(),
                "BitFieldViewArray out of range");

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

  constexpr auto operator[](const std::size_t idx)
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return MakeBitFieldView<field_t>(start + width * idx, width, host);
  }
  constexpr auto operator[](const std::size_t idx) const
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

template <Enumerated field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class EnumBitFieldFixedViewArray
    : public BitFieldFixedViewArray<field_t_, start_, width_, length_, host_t_>
{
private:
  using Iterator = EnumFixedViewArrayIterator<field_t_, start_, width_, host_t_>;
  using ConstIterator = EnumFixedViewArrayConstIterator<field_t_, start_, width_, host_t_>;

public:
  EnumBitFieldFixedViewArray() = delete;
  constexpr EnumBitFieldFixedViewArray(host_t_& host_)
      : BitFieldFixedViewArray<field_t_, start_, width_, length_, host_t_>(host_)
  {
  }

  constexpr Iterator begin() { return Iterator(this->host, 0); }
  constexpr Iterator end() { return Iterator(this->host, length_); }
  constexpr ConstIterator begin() const { return ConstIterator(this->host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(this->host, length_); }
  constexpr ConstIterator cbegin() const { return ConstIterator(this->host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(this->host, length_); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class ConstBitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

protected:
  const host_t& host;

private:
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;

  static_assert(width > 0, "BitFieldViewArray is invalid (zero width)");
  static_assert(length > 0, "BitFieldViewArray is invalid (zero length)");
  // These static assertions are normally the job of fixed variant Get/SetBitField functions.
  // BitFieldViewArrays never run into those functions, though, which could lead to nasty bugs.
  static_assert(!SameAsOrUnderlying<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
  static_assert(!Arithmetic<host_t> || start + width * length <= BitSize<host_t>(),
                "BitFieldViewArray out of range");

public:
  ConstBitFieldFixedViewArray() = delete;
  constexpr ConstBitFieldFixedViewArray(const host_t& host_) : host(host_) {}

  constexpr field_t Get(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < length);  // Index out of range
    return GetBitField<field_t>(start + width * idx, width, host);
  }

  constexpr auto operator[](const std::size_t idx) const
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

template <Enumerated field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class EnumConstBitFieldFixedViewArray
    : public ConstBitFieldFixedViewArray<field_t_, start_, width_, length_, host_t_>
{
private:
  using ConstIterator = EnumFixedViewArrayConstIterator<field_t_, start_, width_, host_t_>;

public:
  EnumConstBitFieldFixedViewArray() = delete;
  constexpr EnumConstBitFieldFixedViewArray(const host_t_& host_)
      : ConstBitFieldFixedViewArray<field_t_, start_, width_, length_, host_t_>(host_)
  {
  }

  constexpr ConstIterator begin() const { return ConstIterator(this->host, 0); }
  constexpr ConstIterator end() const { return ConstIterator(this->host, length_); }
  constexpr ConstIterator cbegin() const { return ConstIterator(this->host, 0); }
  constexpr ConstIterator cend() const { return ConstIterator(this->host, length_); }
  constexpr std::size_t Size() const { return this->length; }
  constexpr std::size_t Length() const { return this->length; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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

protected:
  host_t& host;
  std::size_t idx;

public:
  // Required by std::input_or_output_iterator
  constexpr FixedViewArrayIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr FixedViewArrayIterator(const FixedViewArrayIterator&) = default;
  FixedViewArrayIterator& operator=(const FixedViewArrayIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr FixedViewArrayIterator(FixedViewArrayIterator&&) = default;
  FixedViewArrayIterator& operator=(FixedViewArrayIterator&&) = default;

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

template <Enumerated field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class EnumFixedViewArrayIterator : public FixedViewArrayIterator<field_t_, start_, width_, host_t_>
{
public:
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t_;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = EnumBitFieldView<field_t_, host_t_>;

  // Required by std::input_or_output_iterator
  constexpr EnumFixedViewArrayIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr EnumFixedViewArrayIterator(const EnumFixedViewArrayIterator&) = default;
  EnumFixedViewArrayIterator& operator=(const EnumFixedViewArrayIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr EnumFixedViewArrayIterator(EnumFixedViewArrayIterator&&) = default;
  EnumFixedViewArrayIterator& operator=(EnumFixedViewArrayIterator&&) = default;

  EnumFixedViewArrayIterator(host_t_& host_, const std::size_t idx_)
      : FixedViewArrayIterator<field_t_, start_, width_, host_t_>(host_, idx_)
  {
  }

  EnumFixedViewArrayIterator& operator++()
  {
    ++this->idx;
    return *this;
  }
  EnumFixedViewArrayIterator operator++(int)
  {
    EnumFixedViewArrayIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start_ + width_ * this->idx, width_, this->host);
  }
  constexpr bool operator==(const EnumFixedViewArrayIterator other) const
  {
    return this->idx == other.idx;
  }
  constexpr bool operator!=(const EnumFixedViewArrayIterator other) const
  {
    return this->idx != other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <IntegralOrEnum field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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

protected:
  const host_t& host;
  std::size_t idx;

public:
  // Required by std::input_or_output_iterator
  constexpr FixedViewArrayConstIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr FixedViewArrayConstIterator(const FixedViewArrayConstIterator&) = default;
  FixedViewArrayConstIterator& operator=(const FixedViewArrayConstIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr FixedViewArrayConstIterator(FixedViewArrayConstIterator&&) = default;
  FixedViewArrayConstIterator& operator=(FixedViewArrayConstIterator&&) = default;

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

template <Enumerated field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class EnumFixedViewArrayConstIterator
    : public FixedViewArrayConstIterator<field_t_, start_, width_, host_t_>
{
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t_;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference = EnumConstBitFieldView<field_t_, host_t_>;

  // Required by std::input_or_output_iterator
  constexpr EnumFixedViewArrayConstIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr EnumFixedViewArrayConstIterator(const EnumFixedViewArrayConstIterator&) = default;
  EnumFixedViewArrayConstIterator& operator=(const EnumFixedViewArrayConstIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr EnumFixedViewArrayConstIterator(EnumFixedViewArrayConstIterator&&) = default;
  EnumFixedViewArrayConstIterator& operator=(EnumFixedViewArrayConstIterator&&) = default;

  EnumFixedViewArrayConstIterator(const host_t_& host_, const std::size_t idx_)
      : FixedViewArrayConstIterator<field_t_, start_, width_, host_t_>(host_, idx_)
  {
  }

  EnumFixedViewArrayConstIterator& operator++()
  {
    ++this->idx;
    return *this;
  }
  EnumFixedViewArrayConstIterator operator++(int)
  {
    EnumFixedViewArrayConstIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start_ + width_ * this->idx, width_, this->host);
  }
  constexpr bool operator==(const EnumFixedViewArrayConstIterator other) const
  {
    return this->idx == other.idx;
  }
  constexpr bool operator!=(const EnumFixedViewArrayConstIterator other) const
  {
    return this->idx != other.idx;
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
//   BFVIEW_IN(hex_a, u32, 3, 3, arbitrary_field)
//   BFVIEWARRAY_IN(hex_b, u32, 9, 2, 5, arbitrary_field_array)
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
//   BFVIEW(u32, 3, 3, arbitrary_field)
//   BFVIEWARRAY(u32, 9, 2, 5, arbitrary_field_array)
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

template <Common::IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
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
template <Common::Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<Common::EnumBitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::EnumBitFieldFixedView<field_t, start, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::IntegralOrEnum field_t, std::size_t start, std::size_t width, typename host_t>
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
template <Common::Enumerated field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<Common::EnumConstBitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::EnumConstBitFieldFixedView<field_t, start, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::IntegralOrEnum field_t, typename host_t>
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
template <Common::Enumerated field_t, typename host_t>
struct fmt::formatter<Common::EnumBitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::EnumBitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::IntegralOrEnum field_t, typename host_t>
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
template <Common::Enumerated field_t, typename host_t>
struct fmt::formatter<Common::EnumConstBitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::EnumConstBitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
