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

// BitFieldView.h - A C++17 library

#pragma once

#include <cstddef>      // std::size_t, std::int32_t, std::uint32_t, std::int64_t, std::uint64_t
#include <iterator>     // std::input_iterator_tag, std::output_iterator_tag
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::make_unsigned, std::make_signed, std::is_integral, std::is_enum

#include "Common/Assert.h"  // DEBUG_ASSERT

///////////////////////////////////////////////////////////////////////////////////////////////////
// Assertation helper classes

template <typename host_t>
struct AssertHostTypeIsSane
{
  AssertHostTypeIsSane() = default;
  static_assert(std::is_integral_v<host_t> || std::is_enum_v<host_t>,
                "Given host type is not sane.  You must define a partial specialization for it.");
};
template <typename field_t>
struct AssertFieldTypeIsSane
{
  AssertFieldTypeIsSane() = default;
  static_assert(std::is_integral_v<field_t> || std::is_enum_v<field_t>,
                "Given field type is not sane.  It must be an integral or enumerated type.");
};
template <typename field_t, std::size_t start, std::size_t width, typename host_t>
struct AssertBitFieldFixedIsSane
{
  AssertBitFieldFixedIsSane() = default;
  static_assert(start <= 8 * sizeof(host_t), "BitField out of range");
  static_assert(start + width <= 8 * sizeof(host_t), "BitField out of range");
  static_assert(width <= 8 * sizeof(field_t), "Field is too wide");
  static_assert(width > 0, "BitField is invalid (zero width)");
  static_assert(!std::is_same_v<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
};
template <typename field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
struct AssertBitFieldFixedArrayIsSane
{
  AssertBitFieldFixedArrayIsSane() = default;
  static_assert(start <= 8 * sizeof(host_t), "BitFieldArray out of range");
  static_assert(start + width * length <= 8 * sizeof(host_t), "BitFieldArray out of range");
  static_assert(width <= 8 * sizeof(field_t), "Field is too wide");
  static_assert(width > 0, "Field is invalid (zero width)");
  static_assert(!std::is_same_v<field_t, bool> || width == 1,
                "Boolean fields should only be 1 bit wide");
};
template <typename field_t, typename host_t>
struct AssertBitFieldIsSane
{
  AssertBitFieldIsSane() = delete;
  AssertBitFieldIsSane(const std::size_t start, const std::size_t width)
  {
    DEBUG_ASSERT(start <= 8 * sizeof(host_t));          // BitField out of range
    DEBUG_ASSERT(start + width <= 8 * sizeof(host_t));  // BitField out of range
    DEBUG_ASSERT(width <= 8 * sizeof(field_t));         // BitField is too wide
    DEBUG_ASSERT(width > 0);                            // BitField is invalid (zero width);
    if constexpr (std::is_same_v<field_t, bool>)
      DEBUG_ASSERT(width == 1);  // Boolean fields should only be 1 bit wide
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Simple, functional programming.

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr field_t GetBitFieldFixed(const host_t host)
{
  AssertHostTypeIsSane<host_t>();
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, host_t>();

  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  constexpr std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr std::size_t lshift = rshift - start;

  // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
  // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
}

template <typename field_t, typename host_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width, const host_t host)
{
  AssertHostTypeIsSane<host_t>();
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, host_t>(start, width);

  using uhost_t = std::make_unsigned_t<host_t>;
  using shost_t = std::make_signed_t<host_t>;
  const std::size_t rshift = 8 * sizeof(host_t) - width;
  const std::size_t lshift = rshift - start;

  // Choose between arithmetic (sign extend) or logical (no sign extend) right-shift.
  // Fun fact: This is implementaton defined.  It is only possible to do this implicitly.
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<shost_t>(host << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<uhost_t>(host << lshift) >> rshift);
}

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr void SetBitFieldFixed(host_t& host, const field_t val)
{
  AssertHostTypeIsSane<host_t>();
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, host_t>();

  using uhost_t = std::make_unsigned_t<host_t>;
  constexpr std::size_t rshift = 8 * sizeof(host_t) - width;
  constexpr uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;

  host = (host & ~mask) | ((static_cast<uhost_t>(val) << start) & mask);
}

template <typename field_t, typename host_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, host_t& host,
                           const field_t val)
{
  AssertHostTypeIsSane<host_t>();
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, host_t>(start, width);

  using uhost_t = std::make_unsigned_t<host_t>;
  const std::size_t rshift = 8 * sizeof(host_t) - width;
  const uhost_t mask = std::numeric_limits<uhost_t>::max() >> rshift << start;

  host = (host & ~mask) | ((static_cast<uhost_t>(val) << start) & mask);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Float host type specialization

namespace detail
{
using intflt_t = std::int32_t;
using uintflt_t = std::uint32_t;
static_assert(sizeof(float) == sizeof(intflt_t));
union UFloatBits
{
  float flt;
  intflt_t sbits;
  uintflt_t ubits;
  explicit constexpr UFloatBits(float flt_) : flt(flt_) {}
  explicit constexpr UFloatBits(intflt_t sbits_) : sbits(sbits_) {}
  explicit constexpr UFloatBits(uintflt_t ubits_) : ubits(ubits_) {}
};
}  // namespace detail

template <typename field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const float& host)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, float>();

  constexpr std::size_t rshift = 8 * sizeof(float) - width;
  constexpr std::size_t lshift = rshift - start;

  detail::UFloatBits hostbits(host);
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<detail::intflt_t>(hostbits.ubits << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<detail::uintflt_t>(hostbits.ubits << lshift) >> rshift);
}

template <typename field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width, const float host)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, float>(start, width);

  const std::size_t rshift = 8 * sizeof(float) - width;
  const std::size_t lshift = rshift - start;

  detail::UFloatBits hostbits(host);
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<detail::intflt_t>(hostbits.ubits << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<detail::uintflt_t>(hostbits.ubits << lshift) >> rshift);
}

template <typename field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(float& host, const field_t val)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, float>();

  using detail::uintflt_t;
  constexpr std::size_t rshift = 8 * sizeof(float) - width;
  constexpr uintflt_t mask = std::numeric_limits<uintflt_t>::max() >> rshift << start;

  detail::UFloatBits hostbits(host);
  hostbits.ubits = (hostbits.ubits & ~mask) | ((static_cast<uintflt_t>(val) << start) & mask);
  host = hostbits.flt;
}

template <typename field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, float& host,
                           const field_t val)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, float>(start, width);

  using detail::uintflt_t;
  const std::size_t rshift = 8 * sizeof(float) - width;
  const uintflt_t mask = std::numeric_limits<uintflt_t>::max() >> rshift << start;

  detail::UFloatBits hostbits(host);
  hostbits.ubits = (hostbits.ubits & ~mask) | ((static_cast<uintflt_t>(val) << start) & mask);
  host = hostbits.flt;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Double host type specialization

namespace detail
{
using intdbl_t = std::int64_t;
using uintdbl_t = std::uint64_t;
static_assert(sizeof(double) == sizeof(intdbl_t));
union UDoubleBits
{
  double dbl;
  intdbl_t sbits;
  uintdbl_t ubits;
  explicit constexpr UDoubleBits(double dbl_) : dbl(dbl_) {}
  explicit constexpr UDoubleBits(intdbl_t sbits_) : sbits(sbits_) {}
  explicit constexpr UDoubleBits(uintdbl_t ubits_) : ubits(ubits_) {}
};
}  // namespace detail

template <typename field_t, std::size_t start, std::size_t width>
constexpr field_t GetBitFieldFixed(const double host)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, double>();

  constexpr std::size_t rshift = 8 * sizeof(double) - width;
  constexpr std::size_t lshift = rshift - start;

  detail::UDoubleBits hostbits(host);
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<detail::intdbl_t>(hostbits.ubits << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<detail::uintdbl_t>(hostbits.ubits << lshift) >> rshift);
}

template <typename field_t>
constexpr field_t GetBitField(const std::size_t start, const std::size_t width, const double host)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, double>(start, width);

  const std::size_t rshift = 8 * sizeof(double) - width;
  const std::size_t lshift = rshift - start;

  detail::UDoubleBits hostbits(host);
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(static_cast<detail::intdbl_t>(hostbits.ubits << lshift) >> rshift);
  else
    return static_cast<field_t>(static_cast<detail::uintdbl_t>(hostbits.ubits << lshift) >> rshift);
}

template <typename field_t, std::size_t start, std::size_t width>
constexpr void SetBitFieldFixed(double& host, const field_t val)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldFixedIsSane<field_t, start, width, double>();

  using detail::uintdbl_t;
  constexpr std::size_t rshift = 8 * sizeof(double) - width;
  constexpr uintdbl_t mask = std::numeric_limits<uintdbl_t>::max() >> rshift << start;

  detail::UDoubleBits hostbits(host);
  hostbits.ubits = (hostbits.ubits & ~mask) | ((static_cast<uintdbl_t>(val) << start) & mask);
  host = hostbits.dbl;
}

template <typename field_t>
constexpr void SetBitField(const std::size_t start, const std::size_t width, double& host,
                           const field_t val)
{
  AssertFieldTypeIsSane<field_t>();
  AssertBitFieldIsSane<field_t, double>(start, width);

  using detail::uintdbl_t;
  const std::size_t rshift = 8 * sizeof(double) - width;
  const uintdbl_t mask = std::numeric_limits<uintdbl_t>::max() >> rshift << start;

  detail::UDoubleBits hostbits(host);
  hostbits.ubits = (hostbits.ubits & ~mask) | ((static_cast<uintdbl_t>(val) << start) & mask);
  host = hostbits.dbl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class BitFieldFixedView;

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class ConstBitFieldFixedView;

template <typename field_t_, typename host_t_>
class BitFieldView;

template <typename field_t_, typename host_t_>
class ConstBitFieldView;

template <typename field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class BitFieldFixedViewArray;

template <typename field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class ConstBitFieldFixedViewArray;

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayIterator;

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
class FixedViewArrayConstIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Even C++17 cannot partially deduce template parameters for constructors.  For them, it's all or
// nothing.  Functions, however, can, so we should define some pre-C++17-esque helper functions.

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr BitFieldFixedView<field_t, start, width, host_t>  //
MakeBitFieldFixedView(host_t& host)
{
  return BitFieldFixedView<field_t, start, width, host_t>(host);
}
template <typename field_t, std::size_t start, std::size_t width, typename host_t>
constexpr ConstBitFieldFixedView<field_t, start, width, host_t>  //
MakeConstBitFieldFixedView(const host_t& host)
{
  return ConstBitFieldFixedView<field_t, start, width, host_t>(host);
}
template <typename field_t, typename host_t>
constexpr BitFieldView<field_t, host_t>  //
MakeBitFieldView(const std::size_t start, const std::size_t width, host_t& host)
{
  return BitFieldView<field_t, host_t>(start, width, host);
}
template <typename field_t, typename host_t>
constexpr ConstBitFieldView<field_t, host_t>  //
MakeConstBitFieldView(const std::size_t start, const std::size_t width, const host_t& host)
{
  return ConstBitFieldView<field_t, host_t>(start, width, host);
}
template <typename field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr BitFieldFixedViewArray<field_t, start, width, length, host_t>  //
MakeBitFieldFixedViewArray(host_t& host)
{
  return BitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}
template <typename field_t, std::size_t start, std::size_t width, std::size_t length,
          typename host_t>
constexpr ConstBitFieldFixedViewArray<field_t, start, width, length, host_t>  //
MakeConstBitFieldFixedViewArray(const host_t& host)
{
  return ConstBitFieldFixedViewArray<field_t, start, width, length, host_t>(host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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
  constexpr BitFieldFixedView& operator|=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr BitFieldFixedView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr BitFieldFixedView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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

template <typename field_t_, typename host_t_>
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

template <typename field_t_, typename host_t_>
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

template <typename field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class BitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using Iterator = FixedViewArrayIterator<field_t, start, width, host_t>;
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;
  host_t& host;

public:
  BitFieldFixedViewArray() = delete;
  constexpr BitFieldFixedViewArray(host_t& host_) : host(host_)
  {
    AssertBitFieldFixedArrayIsSane<field_t, start, width, length, host_t>();
  }

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

template <typename field_t_, std::size_t start_, std::size_t width_, std::size_t length_,
          typename host_t_>
class ConstBitFieldFixedViewArray
{
public:
  using host_t = host_t_;
  using field_t = field_t_;
  constexpr static std::size_t start = start_;
  constexpr static std::size_t width = width_;
  constexpr static std::size_t length = length_;

private:
  using ConstIterator = FixedViewArrayConstIterator<field_t, start, width, host_t>;
  const host_t& host;

public:
  ConstBitFieldFixedViewArray() = delete;
  constexpr ConstBitFieldFixedViewArray(const host_t& host_) : host(host_)
  {
    AssertBitFieldFixedArrayIsSane<field_t, start, width, length, host_t>();
  }

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

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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

template <typename field_t_, std::size_t start_, std::size_t width_, typename host_t_>
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

///////////////////////////////////////////////////////////////////////////////////////////////////
// BitField View (Array) in Member?  BitField View (Array) Macro?  BitField View (Array) Method?
// You can believe whatever you want to believe this abbreviation is short for.

#define BFVIEW_M(host, field_t, start, width, name)                                                \
  constexpr BitFieldFixedView<field_t, start, width, decltype(host)> name()                        \
  {                                                                                                \
    return MakeBitFieldFixedView<field_t, start, width>(host);                                     \
  }                                                                                                \
  constexpr ConstBitFieldFixedView<field_t, start, width, decltype(host)> name() const             \
  {                                                                                                \
    return MakeConstBitFieldFixedView<field_t, start, width>(host);                                \
  }

#define BFVIEWARRAY_M(host, field_t, start, width, length, name)                                   \
  constexpr BitFieldFixedViewArray<field_t, start, width, length, decltype(host)> name()           \
  {                                                                                                \
    return MakeBitFieldFixedViewArray<field_t, start, width, length>(host);                        \
  }                                                                                                \
  constexpr ConstBitFieldFixedViewArray<field_t, start, width, length, decltype(host)> name()      \
      const                                                                                        \
  {                                                                                                \
    return MakeConstBitFieldFixedViewArray<field_t, start, width, length>(host);                   \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////

#include <fmt/format.h>

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<BitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldFixedView<field_t, start, width, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename field_t, std::size_t start, std::size_t width, typename host_t>
struct fmt::formatter<ConstBitFieldFixedView<field_t, start, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const ConstBitFieldFixedView<field_t, start, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename field_t, typename host_t>
struct fmt::formatter<BitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const BitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <typename field_t, typename host_t>
struct fmt::formatter<ConstBitFieldView<field_t, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const ConstBitFieldView<field_t, host_t>& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
