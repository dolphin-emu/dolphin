// Copyright 2022, Bradley G. (Minty Meeo)
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>      // std::size_t
#include <cstdint>      // std::uint32_t, std::uint64_t
#include <iterator>     // std::input_iterator_tag, std::output_iterator_tag
#include <type_traits>  // std::is_signed, std::is_arithmetic

#include "Common/Assert.h"        // DEBUG_ASSERT
#include "Common/BitUtils.h"      // Common::ExtractBitsU, Common::ExtractBitsS,
                                  // Common::InsertBits, Common::ExtractBit, Common::InsertBit,
                                  // Common::BitSize
#include "Common/Concepts.h"      // Common::SameAsOrUnderlyingSameAs, Common::UnscopedEnum
#include "Common/ConstantPack.h"  // Common::IndexPack
#include "Common/TypeUtils.h"     // Common::ScopedEnumUnderlyingElseVoid

#include "Common/Future/CppLibConcepts.h"  // std::integral

namespace Common
{
template <class T>
concept FastBitFieldType = SameAsOrUnderlyingSameAs<T, bool>;
template <class T>
concept SaneBitFieldType = IntegralOrEnum<T> && !FastBitFieldType<T>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Intermediary step between BitFieldView classes and BitUtil functions.  This step requires
// field_t and host_t to satisfy constraints.  If host_t does not satisfy std::integral, you must
// define a partial specialization for that host type (see: float and double specializations).  If
// field_t does not satisfy IntegralOrEnum (which can be divided into SaneBitFieldType and
// FastBitFieldType), you are using these functions wrong.  It is recommended that any host which
// must exist in memory (like a wrapper host_t) always be passed by reference.

template <SaneBitFieldType field_t, std::size_t width, std::size_t start, std::integral host_t>
constexpr field_t GetFixedBitField(const host_t host) noexcept
{
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS<width, start>(host));
  else
    return static_cast<field_t>(ExtractBitsU<width, start>(host));
}

template <SaneBitFieldType field_t, std::size_t width, std::integral host_t>
constexpr field_t GetLooseBitField(const std::size_t start, const host_t host) noexcept
{
  if constexpr (std::is_signed_v<field_t>)
    return static_cast<field_t>(ExtractBitsS<width>(start, host));
  else
    return static_cast<field_t>(ExtractBitsU<width>(start, host));
}

template <SaneBitFieldType field_t, std::size_t width, std::size_t start, std::integral host_t>
constexpr void SetFixedBitField(host_t& host, const field_t val) noexcept
{
  InsertBits<width, start>(host, val);
}

template <SaneBitFieldType field_t, std::size_t width, std::integral host_t>
constexpr void SetLooseBitField(const std::size_t start, host_t& host, const field_t val) noexcept
{
  InsertBits<width>(start, host, val);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Bool or enumeration of underlying type bool field type overload (for optimization)

template <FastBitFieldType field_t, std::size_t width, std::size_t start, std::integral host_t>
constexpr field_t GetFixedBitField(const host_t& host) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  return static_cast<field_t>(ExtractBit<start>(host));
}

template <FastBitFieldType field_t, std::size_t width, std::integral host_t>
constexpr field_t GetLooseBitField(const std::size_t start, const host_t& host) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  return static_cast<field_t>(ExtractBit(start, host));
}

template <FastBitFieldType field_t, std::size_t width, std::size_t start, std::integral host_t>
constexpr void SetFixedBitField(host_t& host, const field_t val) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  InsertBit<start>(host, static_cast<bool>(val));
}

template <FastBitFieldType field_t, std::size_t width, std::integral host_t>
constexpr void SetLooseBitField(const std::size_t start, host_t& host, const field_t val) noexcept
{
  static_assert(width == 1, "Boolean fields should only be 1 bit wide");
  InsertBit(start, host, static_cast<bool>(val));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Float host type specialization

static_assert(sizeof(float) == sizeof(std::uint32_t));

template <IntegralOrEnum field_t, std::size_t width, std::size_t start>
constexpr field_t GetFixedBitField(const float& host) noexcept
{
  const std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  return GetFixedBitField<field_t, width, start>(hostbits);
}

template <IntegralOrEnum field_t, std::size_t width>
constexpr field_t GetLooseBitField(const std::size_t start, const float& host) noexcept
{
  const std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  return GetLooseBitField<field_t, width>(start, hostbits);
}

template <IntegralOrEnum field_t, std::size_t width, std::size_t start>
constexpr void SetFixedBitField(float& host, const field_t val) noexcept
{
  std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  SetFixedBitField<field_t, width, start>(hostbits, val);
  host = BitCast<float>(hostbits);
}

template <IntegralOrEnum field_t, std::size_t width>
constexpr void SetLooseBitField(const std::size_t start, float& host, const field_t val) noexcept
{
  std::uint32_t hostbits = BitCast<std::uint32_t>(host);
  SetLooseBitField<field_t, width>(start, hostbits, val);
  host = BitCast<float>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Double host type specialization

static_assert(sizeof(double) == sizeof(std::uint64_t));

template <IntegralOrEnum field_t, std::size_t width, std::size_t start>
constexpr field_t GetFixedBitField(const double& host) noexcept
{
  const std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  return GetFixedBitField<field_t, width, start>(hostbits);
}

template <IntegralOrEnum field_t, std::size_t width>
constexpr field_t GetLooseBitField(const std::size_t start, const double& host) noexcept
{
  const std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  return GetLooseBitField<field_t, width>(start, hostbits);
}

template <IntegralOrEnum field_t, std::size_t width, std::size_t start>
constexpr void SetFixedBitField(double& host, const field_t val) noexcept
{
  std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  SetFixedBitField<field_t, width, start>(hostbits, val);
  host = BitCast<double>(hostbits);
}

template <IntegralOrEnum field_t, std::size_t width>
constexpr void SetLooseBitField(const std::size_t start, double& host, const field_t val) noexcept
{
  std::uint64_t hostbits = BitCast<std::uint64_t>(host);
  SetLooseBitField<field_t, width>(start, hostbits, val);
  host = BitCast<double>(hostbits);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations.

template <class field_t, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldView;
template <class field_t, std::size_t width, class host_t>
class LooseBitFieldView;

template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldArrayView;
template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
class LooseBitFieldArrayView;

template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldArrayViewIterator;
template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
class LooseBitFieldArrayViewIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Define some pre-C++17-esque helper functions.  We do this because, even as of C++17, one cannot
// partially deduce template parameters for constructors.  For them, it's all or nothing.

template <class field_t, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldView(host_t& host)
{
  return FixedBitFieldView<field_t, width, start, host_t>(host);
}
template <class field_t, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldView(const host_t& host)
{
  return FixedBitFieldView<field_t, width, start, const host_t>(host);
}
template <class field_t, std::size_t width, class host_t>
constexpr auto MakeBitFieldView(const std::size_t start, host_t& host)
{
  return LooseBitFieldView<field_t, width, host_t>(start, host);
}
template <class field_t, std::size_t width, class host_t>
constexpr auto MakeBitFieldView(const std::size_t start, const host_t& host)
{
  return LooseBitFieldView<field_t, width, const host_t>(start, host);
}

template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldArrayView(host_t& host)
{
  return FixedBitFieldArrayView<field_t, Ns, width, start, host_t>(host);
}
template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldArrayView(const host_t& host)
{
  return FixedBitFieldArrayView<field_t, Ns, width, start, const host_t>(host);
}
template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
constexpr auto MakeBitFieldArrayView(const std::size_t start, host_t& host)
{
  return LooseBitFieldArrayView<field_t, Ns, width, host_t>(start, host);
}
template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
constexpr auto MakeBitFieldArrayView(const std::size_t start, const host_t& host)
{
  return LooseBitFieldArrayView<field_t, Ns, width, const host_t>(start, host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldView final
{
protected:
  host_t& host;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  constexpr static std::size_t BitStart() { return start; }

  FixedBitFieldView() = delete;
  constexpr FixedBitFieldView(host_t& host_) : host(host_){};
  constexpr FixedBitFieldView& operator=(const FixedBitFieldView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const { return GetFixedBitField<field_t, width, start>(host); }
  constexpr void Set(const field_t val) { SetFixedBitField<field_t, width, start>(host, val); }

  constexpr FixedBitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr FixedBitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr FixedBitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr FixedBitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr FixedBitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr FixedBitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr FixedBitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr FixedBitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  /// This code enables implicit casts to unscoped enums, but also somehow clashes with Qt, which
  /// globally deletes many non-typesafe operations for QFlags with Q_DECLARE_OPERATORS_FOR_FLAGS.
  // template <class T> requires(std::is_enum_v<T> && !std::is_scoped_enum_v<T>)
  // constexpr operator T() const
  // {
  //   return static_cast<T>(Get());  // Surprisingly required static_cast
  // }
  template <class T>
  constexpr explicit operator T() const
  {
    return static_cast<T>(Get());  // Test your luck.
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, class host_t>
class LooseBitFieldView final
{
protected:
  host_t& host;
  const std::size_t start;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  std::size_t BitStart() { return start; }

  LooseBitFieldView() = delete;
  constexpr LooseBitFieldView(const std::size_t start_, host_t& host_)
      : host(host_), start(start_){};
  constexpr LooseBitFieldView& operator=(const LooseBitFieldView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const { return GetLooseBitField<field_t, width>(start, host); }
  constexpr void Set(const field_t val)
  {
    return SetLooseBitField<field_t, width>(start, host, val);
  }

  constexpr LooseBitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr LooseBitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr LooseBitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr LooseBitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr LooseBitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr LooseBitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr LooseBitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr LooseBitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  /// This code enables implicit casts to unscoped enums, but also somehow clashes with Qt, which
  /// globally deletes many non-typesafe operations for QFlags with Q_DECLARE_OPERATORS_FOR_FLAGS.
  // template <class T> requires(std::is_enum_v<T> && !std::is_scoped_enum_v<T>)
  // constexpr operator T() const
  // {
  //   return static_cast<T>(Get());  // Surprisingly required static_cast
  // }
  template <class T>
  constexpr explicit operator T() const
  {
    return static_cast<T>(Get());  // Test your luck.
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldArrayView final
{
protected:
  host_t& host;

public:
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         LooseBitFieldView<field_t, width, host_t>>;
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, const host_t>,
                         LooseBitFieldView<field_t, width, const host_t>>;
  using iterator = FixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>;
  using const_iterator = FixedBitFieldArrayViewIterator<field_t, Ns, width, start, const host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth() * length(); }
  constexpr static std::size_t BitStart() { return start; }

  FixedBitFieldArrayView() = delete;
  constexpr FixedBitFieldArrayView(host_t& host_) : host(host_)
  {
    // This static assertion is normally the job of the GetFixedBitField/SetFixedBitField functions.
    // BitFieldArrayViews never run into those functions, though, which could lead to nasty bugs.
    static_assert(!std::is_arithmetic_v<host_t> || start + BitWidth() <= BitSize<host_t>(),
                  "BitFieldArrayView out of range");
  }

  constexpr reference at(const std::size_t idx)
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr const_reference at(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return const_reference(start + const_reference::BitWidth() * idx, host);
  }
  constexpr reference operator[](const std::size_t idx) { return at(idx); }
  constexpr const_reference operator[](const std::size_t idx) const { return at(idx); }

  constexpr iterator begin() { return iterator(host, 0); }
  constexpr iterator end() { return iterator(host, length()); }
  constexpr const_iterator begin() const { return const_iterator(host, 0); }
  constexpr const_iterator end() const { return const_iterator(host, length()); }
  constexpr const_iterator cbegin() const { return const_iterator(host, 0); }
  constexpr const_iterator cend() const { return const_iterator(host, length()); }
  constexpr static std::size_t size() { return Ns::first; }
  constexpr static std::size_t length() { return Ns::first; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
class LooseBitFieldArrayView final
{
protected:
  host_t& host;
  const std::size_t start;

public:
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         LooseBitFieldView<field_t, width, host_t>>;
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, const host_t>,
                         LooseBitFieldView<field_t, width, const host_t>>;
  using iterator = LooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>;
  using const_iterator = LooseBitFieldArrayViewIterator<field_t, Ns, width, const host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth() * Ns::first; }
  std::size_t BitStart() { return start; }

  LooseBitFieldArrayView() = delete;
  constexpr LooseBitFieldArrayView(const std::size_t start_, host_t& host_)
      : host(host_), start(start_)
  {
  }

  constexpr reference at(const std::size_t idx)
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr const_reference at(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return const_reference(start + const_reference::BitWidth() * idx, host);
  }
  constexpr reference operator[](const std::size_t idx) { return at(idx); }
  constexpr const_reference operator[](const std::size_t idx) const { return at(idx); }

  constexpr iterator begin() { return iterator(start, host, 0); }
  constexpr iterator end() { return iterator(start, host, Ns::first); }
  constexpr const_iterator begin() const { return const_iterator(start, host, 0); }
  constexpr const_iterator end() const { return const_iterator(start, host, Ns::first); }
  constexpr const_iterator cbegin() const { return const_iterator(start, host, 0); }
  constexpr const_iterator cend() const { return const_iterator(start, host, Ns::first); }
  constexpr static std::size_t size() { return Ns::first; }
  constexpr static std::size_t length() { return Ns::first; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyIndexPack Ns, std::size_t width, std::size_t start, class host_t>
class FixedBitFieldArrayViewIterator final
{
protected:
  host_t& host;
  std::size_t idx;

public:
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         LooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  constexpr static std::size_t BitStart() { return start; }

  FixedBitFieldArrayViewIterator(host_t& host_, const std::size_t idx_) : host(host_), idx(idx_) {}

  // Required by std::input_or_output_iterator
  constexpr FixedBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr FixedBitFieldArrayViewIterator(const FixedBitFieldArrayViewIterator&) = default;
  FixedBitFieldArrayViewIterator& operator=(const FixedBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr FixedBitFieldArrayViewIterator(FixedBitFieldArrayViewIterator&&) = default;
  FixedBitFieldArrayViewIterator& operator=(FixedBitFieldArrayViewIterator&&) = default;

  FixedBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  FixedBitFieldArrayViewIterator operator++(int)
  {
    FixedBitFieldArrayViewIterator other{*this};
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const FixedBitFieldArrayViewIterator& other) const
  {
    return idx == other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyIndexPack Ns, std::size_t width, class host_t>
class LooseBitFieldArrayViewIterator final
{
protected:
  host_t& host;
  std::size_t idx;
  const std::size_t start;

public:
  using iterator_category = std::output_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         LooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         LooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  std::size_t BitStart() { return start; }

  LooseBitFieldArrayViewIterator(const std::size_t start_, host_t& host_, const std::size_t idx_)
      : host(host_), idx(idx_), start(start_)
  {
  }

  // Required by std::input_or_output_iterator
  constexpr LooseBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr LooseBitFieldArrayViewIterator(const LooseBitFieldArrayViewIterator&) = default;
  LooseBitFieldArrayViewIterator& operator=(const LooseBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr LooseBitFieldArrayViewIterator(LooseBitFieldArrayViewIterator&&) = default;
  LooseBitFieldArrayViewIterator& operator=(LooseBitFieldArrayViewIterator&&) = default;

  LooseBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  LooseBitFieldArrayViewIterator operator++(int)
  {
    LooseBitFieldArrayViewIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const LooseBitFieldArrayViewIterator& other) const
  {
    return idx == other.idx;
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
//   u32 hex_c;
//
//   BFVIEW_IN(hex_a, u32, 3, 3, arbitrary_field)
//   BFVIEW_IN(hex_b, u32, 2, 5, arbitrary_field_array, 9)      // 1-dimensional array
//   BFVIEW_IN(hex_c, u32, 2, 5, arbitrary_field_matrix, 3, 3)  // 2-dimensional array
// };

#define BFVIEW_IN(host, field_t, width, start, name, ...)                                          \
  constexpr auto name()                                                                            \
  {                                                                                                \
    return ::Common::MakeBitField##__VA_OPT__(                                                     \
        Array)##View<field_t, __VA_OPT__(::Common::IndexPack<__VA_ARGS__>, ) width, start>(host);  \
  }                                                                                                \
  constexpr auto name() const                                                                      \
  {                                                                                                \
    return ::Common::MakeBitField##__VA_OPT__(                                                     \
        Array)##View<field_t, __VA_OPT__(::Common::IndexPack<__VA_ARGS__>, ) width, start>(host);  \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Automagically ascertain the storage of a single-member class via a structured binding.
//
// class Example
// {
//   u32 hex;
//
//   BFVIEW(u32, 3, 3, arbitrary_field)
//   BFVIEW(u32, 2, 5, arbitrary_field_array, 9)      // 1-dimensional array
//   BFVIEW(u32, 2, 5, arbitrary_field_matrix, 3, 3)  // 2-dimensional array
// };

#define BFVIEW(field_t, width, start, name, ...)                                                   \
  constexpr auto name()                                                                            \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeBitField##__VA_OPT__(                                                     \
        Array)##View<field_t, __VA_OPT__(::Common::IndexPack<__VA_ARGS__>, ) width, start>(host);  \
  }                                                                                                \
  constexpr auto name() const                                                                      \
  {                                                                                                \
    auto& [host] = *this;                                                                          \
    return ::Common::MakeBitField##__VA_OPT__(                                                     \
        Array)##View<field_t, __VA_OPT__(::Common::IndexPack<__VA_ARGS__>, ) width, start>(host);  \
  }

///////////////////////////////////////////////////////////////////////////////////////////////////
// Custom fmt::formatter specializations

#include <fmt/format.h>

template <Common::IntegralOrEnum field_t, std::size_t width, std::size_t start, class host_t>
struct fmt::formatter<Common::FixedBitFieldView<field_t, width, start, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::FixedBitFieldView<field_t, width, start, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

template <Common::IntegralOrEnum field_t, std::size_t width, class host_t>
struct fmt::formatter<Common::LooseBitFieldView<field_t, width, host_t>>
{
  fmt::formatter<field_t> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::LooseBitFieldView<field_t, width, host_t>& ref,
              FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
