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
#include "Common/Concepts.h"      // Common::SameAsOrUnderlyingSameAs
#include "Common/ConstantPack.h"  // Common::IndexPack
#include "Common/TypeUtils.h"     // Common::ScopedEnumUnderlyingElseVoid

#include "Common/Future/CppLibConcepts.h"      // std::integral
#include "Common/Future/CppLibToUnderlying.h"  // std::to_underlying

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
// Forward declarations.  Con is short for const.  Mut is short for mutable.

template <class field_t, std::size_t width, std::size_t start, class host_t>
class MutFixedBitFieldView;
template <class field_t, std::size_t width, std::size_t start, class host_t>
class ConFixedBitFieldView;
template <class field_t, std::size_t width, class host_t>
class MutLooseBitFieldView;
template <class field_t, std::size_t width, class host_t>
class ConLooseBitFieldView;

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class MutFixedBitFieldArrayView;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t_>
class ConFixedBitFieldArrayView;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class MutLooseBitFieldArrayView;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t_>
class ConLooseBitFieldArrayView;

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class MutFixedBitFieldArrayViewIterator;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class ConFixedBitFieldArrayViewIterator;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class MutLooseBitFieldArrayViewIterator;
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class ConLooseBitFieldArrayViewIterator;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BitFieldView concepts

// This is the best solution I could come up with given the circumstances.  There exist tricks for
// detecting if a type is an instantiation of a given template *without* abusing requires clauses,
// but the ones I found do not work when value parameters are thrown into the mix.  In short,
// template instances are just types, lacking a direct link back to the template it originated from.
namespace detail
{
template <class field_t, std::size_t width, std::size_t start, class host_t>
void PassMutFixedBitFieldView(MutFixedBitFieldView<field_t, width, start, host_t>);
template <class field_t, std::size_t width, std::size_t start, class host_t>
void PassConFixedBitFieldView(ConFixedBitFieldView<field_t, width, start, host_t>);

template <class T>
concept MF_BFView = requires(T t)
{
  PassMutFixedBitFieldView(t);
};
template <class T>
concept CF_BFView = requires(T t)
{
  PassConFixedBitFieldView(t);
};

template <class field_t, std::size_t width, class host_t>
void PassMutLooseBitFieldView(MutLooseBitFieldView<field_t, width, host_t>);
template <class field_t, std::size_t width, class host_t>
void PassConLooseBitFieldView(ConLooseBitFieldView<field_t, width, host_t>);

template <class T>
concept ML_BFView = requires(T t)
{
  PassMutLooseBitFieldView(t);
};
template <class T>
concept CL_BFView = requires(T t)
{
  PassConLooseBitFieldView(t);
};

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
void PassMutFixedBitFieldArrayView(MutFixedBitFieldArrayView<field_t, Ns, width, start, host_t>);
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
void PassConFixedBitFieldArrayView(ConFixedBitFieldArrayView<field_t, Ns, width, start, host_t>);

template <class T>
concept MF_BFArrayView = requires(T t)
{
  PassMutFixedBitFieldArrayView(t);
};
template <class T>
concept CF_BFArrayView = requires(T t)
{
  PassConFixedBitFieldArrayView(t);
};

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
void PassMutLooseBitFieldArrayView(MutLooseBitFieldArrayView<field_t, Ns, width, host_t>);
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
void PassConLooseBitFieldArrayView(ConLooseBitFieldArrayView<field_t, Ns, width, host_t>);

template <class T>
concept ML_BFArrayView = requires(T t)
{
  PassMutLooseBitFieldArrayView(t);
};
template <class T>
concept CL_BFArrayView = requires(T t)
{
  PassConLooseBitFieldArrayView(t);
};

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
void PassMutFixedBitFieldArrayViewIterator(
    MutFixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>);
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
void PassConFixedBitFieldArrayViewIterator(
    ConFixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>);

template <class T>
concept MF_BFArrayViewIter = requires(T t)
{
  PassMutFixedBitFieldArrayViewIterator(t);
};
template <class T>
concept CF_BFArrayViewIter = requires(T t)
{
  PassConFixedBitFieldArrayViewIterator(t);
};

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
void PassMutLooseBitFieldArrayViewIterator(
    MutLooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>);
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
void PassConLooseBitFieldArrayViewIterator(
    ConLooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>);

template <class T>
concept ML_BFArrayViewIter = requires(T t)
{
  PassMutLooseBitFieldArrayViewIterator(t);
};
template <class T>
concept CL_BFArrayViewIter = requires(T t)
{
  PassConLooseBitFieldArrayViewIterator(t);
};
};  // namespace detail

template <class T>
concept FixedBitFieldView = detail::MF_BFView<T> || detail::CF_BFView<T>;
template <class T>
concept LooseBitFieldView = detail::ML_BFView<T> || detail::CL_BFView<T>;
template <class T>
concept BitFieldView = FixedBitFieldView<T> || LooseBitFieldView<T>;

template <class T>
concept FixedBitFieldArrayView = detail::MF_BFArrayView<T> || detail::CF_BFArrayView<T>;
template <class T>
concept LooseBitFieldArrayView = detail::ML_BFArrayView<T> || detail::CL_BFArrayView<T>;
template <class T>
concept BitFieldArrayView = FixedBitFieldArrayView<T> || LooseBitFieldArrayView<T>;

template <class T>
concept FixedBitFieldArrayViewIterator =
    detail::MF_BFArrayViewIter<T> || detail::CF_BFArrayViewIter<T>;
template <class T>
concept LooseBitFieldArrayViewIterator =
    detail::ML_BFArrayViewIter<T> || detail::CL_BFArrayViewIter<T>;
template <class T>
concept BitFieldArrayViewIterator =
    FixedBitFieldArrayViewIterator<T> || LooseBitFieldArrayViewIterator<T>;

///////////////////////////////////////////////////////////////////////////////////////////////////
// BitFieldView type traits

template <class T>
struct IsFixedBitFieldView : std::bool_constant<FixedBitFieldView<T>>
{
};
template <class T>
struct IsLooseBitFieldView : std::bool_constant<LooseBitFieldView<T>>
{
};
template <class T>
struct IsBitFieldView : std::bool_constant<BitFieldView<T>>
{
};

template <class T>
struct IsFixedBitFieldArrayView : std::bool_constant<FixedBitFieldArrayView<T>>
{
};
template <class T>
struct IsLooseBitFieldArrayView : std::bool_constant<LooseBitFieldArrayView<T>>
{
};
template <class T>
struct IsBitFieldArrayView : std::bool_constant<BitFieldArrayView<T>>
{
};

template <class T>
struct IsFixedBitFieldArrayViewIterator : std::bool_constant<FixedBitFieldArrayViewIterator<T>>
{
};
template <class T>
struct IsLooseBitFieldArrayViewIterator : std::bool_constant<LooseBitFieldArrayViewIterator<T>>
{
};
template <class T>
struct IsBitFieldArrayViewIterator : std::bool_constant<BitFieldArrayViewIterator<T>>
{
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Define some pre-C++17-esque helper functions.  We do this because, even as of C++17, one cannot
// partially deduce template parameters for constructors.  For them, it's all or nothing.

template <class field_t, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldView(host_t& host)
{
  return MutFixedBitFieldView<field_t, width, start, host_t>(host);
}
template <class field_t, std::size_t width, std::size_t start, class host_t>
constexpr auto MakeBitFieldView(const host_t& host)
{
  return ConFixedBitFieldView<field_t, width, start, host_t>(host);
}
template <class field_t, std::size_t width, class host_t>
constexpr auto MakeBitFieldView(const std::size_t start, host_t& host)
{
  return MutLooseBitFieldView<field_t, width, host_t>(start, host);
}
template <class field_t, std::size_t width, class host_t>
constexpr auto MakeBitFieldView(const std::size_t start, const host_t& host)
{
  return ConLooseBitFieldView<field_t, width, host_t>(start, host);
}

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
constexpr auto MakeBitFieldArrayView(host_t& host)
{
  return MutFixedBitFieldArrayView<field_t, Ns, width, start, host_t>(host);
}
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
constexpr auto MakeBitFieldArrayView(const host_t& host)
{
  return ConFixedBitFieldArrayView<field_t, Ns, width, start, host_t>(host);
}
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
constexpr auto MakeBitFieldArrayView(const std::size_t start, host_t& host)
{
  return MutLooseBitFieldArrayView<field_t, Ns, width, host_t>(start, host);
}
template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
constexpr auto MakeBitFieldArrayView(const std::size_t start, const host_t& host)
{
  return ConLooseBitFieldArrayView<field_t, Ns, width, host_t>(start, host);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, std::size_t start, class host_t>
class MutFixedBitFieldView
{
protected:
  host_t& host;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  constexpr static std::size_t BitStart() { return start; }

  MutFixedBitFieldView() = delete;
  constexpr MutFixedBitFieldView(host_t& host_) : host(host_){};
  constexpr MutFixedBitFieldView& operator=(const MutFixedBitFieldView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const { return GetFixedBitField<field_t, width, start>(host); }
  constexpr void Set(const field_t val) { SetFixedBitField<field_t, width, start>(host, val); }

  constexpr MutFixedBitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr MutFixedBitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr MutFixedBitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr MutFixedBitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr MutFixedBitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr MutFixedBitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr MutFixedBitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr MutFixedBitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  constexpr explicit operator ScopedEnumUnderlyingElseVoid<field_t>() const
  {
    return std::to_underlying(Get());
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, std::size_t start, class host_t>
class ConFixedBitFieldView
{
protected:
  const host_t& host;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  constexpr static std::size_t BitStart() { return start; }

  ConFixedBitFieldView() = delete;
  constexpr ConFixedBitFieldView(const host_t& host_) : host(host_){};

  constexpr field_t Get() const { return GetFixedBitField<field_t, width, start>(host); }

  constexpr operator field_t() const { return Get(); }
  constexpr explicit operator ScopedEnumUnderlyingElseVoid<field_t>() const
  {
    return std::to_underlying(Get());
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, class host_t>
class MutLooseBitFieldView
{
protected:
  host_t& host;
  const std::size_t start;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  std::size_t BitStart() { return start; }

  MutLooseBitFieldView() = delete;
  constexpr MutLooseBitFieldView(const std::size_t start_, host_t& host_)
      : host(host_), start(start_){};
  constexpr MutLooseBitFieldView& operator=(const MutLooseBitFieldView& rhs)
  {
    return operator=(rhs.Get());
  }

  constexpr field_t Get() const { return GetLooseBitField<field_t, width>(start, host); }
  constexpr void Set(const field_t val)
  {
    return SetLooseBitField<field_t, width>(start, host, val);
  }

  constexpr MutLooseBitFieldView& operator=(const field_t rhs)
  {
    Set(rhs);
    return *this;
  }
  constexpr MutLooseBitFieldView& operator+=(const field_t rhs) { return operator=(Get() + rhs); }
  constexpr MutLooseBitFieldView& operator-=(const field_t rhs) { return operator=(Get() - rhs); }
  constexpr MutLooseBitFieldView& operator*=(const field_t rhs) { return operator=(Get() * rhs); }
  constexpr MutLooseBitFieldView& operator/=(const field_t rhs) { return operator=(Get() / rhs); }
  constexpr MutLooseBitFieldView& operator|=(const field_t rhs) { return operator=(Get() | rhs); }
  constexpr MutLooseBitFieldView& operator&=(const field_t rhs) { return operator=(Get() & rhs); }
  constexpr MutLooseBitFieldView& operator^=(const field_t rhs) { return operator=(Get() ^ rhs); }

  constexpr operator field_t() const { return Get(); }
  constexpr explicit operator ScopedEnumUnderlyingElseVoid<field_t>() const
  {
    return std::to_underlying(Get());
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, std::size_t width, class host_t>
class ConLooseBitFieldView
{
protected:
  const host_t& host;
  const std::size_t start;

public:
  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return width; }
  std::size_t BitStart() { return start; }

  ConLooseBitFieldView() = delete;
  constexpr ConLooseBitFieldView(const std::size_t start_, const host_t& host_)
      : host(host_), start(start_){};

  constexpr field_t Get() const { return GetLooseBitField<field_t, width>(start, host); }

  constexpr operator field_t() const { return Get(); }
  constexpr explicit operator ScopedEnumUnderlyingElseVoid<field_t>() const
  {
    return std::to_underlying(Get());
  }
  constexpr bool operator!() const { return !static_cast<bool>(Get()); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class MutFixedBitFieldArrayView
{
protected:
  host_t& host;

public:
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         MutLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         MutLooseBitFieldView<field_t, width, host_t>>;
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;
  using iterator = MutFixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>;
  using const_iterator = ConFixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth() * length(); }
  constexpr static std::size_t BitStart() { return start; }

  MutFixedBitFieldArrayView() = delete;
  constexpr MutFixedBitFieldArrayView(host_t& host_) : host(host_)
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

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class ConFixedBitFieldArrayView
{
protected:
  const host_t& host;

public:
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;
  using const_iterator = ConFixedBitFieldArrayViewIterator<field_t, Ns, width, start, host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return const_reference::BitWidth() * Ns::first; }
  constexpr static std::size_t BitStart() { return start; }

  ConFixedBitFieldArrayView() = delete;
  constexpr ConFixedBitFieldArrayView(const host_t& host_) : host(host_)
  {
    // This static assertion is normally the job of the GetFixedBitField/SetFixedBitField functions.
    // BitFieldArrayViews never run into those functions, though, which could lead to nasty bugs.
    static_assert(!std::is_arithmetic_v<host_t> || start + BitWidth() <= BitSize<host_t>(),
                  "BitFieldArrayView out of range");
  }

  constexpr const_reference at(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return const_reference(start + const_reference::BitWidth() * idx, host);
  }

  constexpr const_reference operator[](const std::size_t idx) const { return at(idx); }

  constexpr const_iterator begin() const { return const_iterator(host, 0); }
  constexpr const_iterator end() const { return const_iterator(host, length()); }
  constexpr const_iterator cbegin() const { return const_iterator(host, 0); }
  constexpr const_iterator cend() const { return const_iterator(host, length()); }
  constexpr static std::size_t size() { return Ns::first; }
  constexpr static std::size_t length() { return Ns::first; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class MutLooseBitFieldArrayView
{
protected:
  host_t& host;
  const std::size_t start;

public:
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         MutLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         MutLooseBitFieldView<field_t, width, host_t>>;
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;
  using iterator = MutLooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>;
  using const_iterator = ConLooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth() * Ns::first; }
  std::size_t BitStart() { return start; }

  MutLooseBitFieldArrayView() = delete;
  constexpr MutLooseBitFieldArrayView(const std::size_t start_, host_t& host_)
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

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class ConLooseBitFieldArrayView
{
protected:
  const host_t& host;
  const std::size_t start;

public:
  using const_reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;
  using const_iterator = ConLooseBitFieldArrayViewIterator<field_t, Ns, width, host_t>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return const_reference::BitWidth() * Ns::first; }
  std::size_t BitStart() { return start; }

  ConLooseBitFieldArrayView() = delete;
  constexpr ConLooseBitFieldArrayView(const std::size_t start_, const host_t& host_)
      : host(host_), start(start_)
  {
  }

  constexpr const_reference at(const std::size_t idx) const
  {
    DEBUG_ASSERT(idx < Ns::first);  // Index out of range
    return const_reference(start + const_reference::BitWidth() * idx, host);
  }
  constexpr const_reference operator[](const std::size_t idx) const { return at(idx); }

  constexpr const_iterator begin() const { return const_iterator(start, host, 0); }
  constexpr const_iterator end() const { return const_iterator(start, host, Ns::first); }
  constexpr const_iterator cbegin() const { return const_iterator(start, host, 0); }
  constexpr const_iterator cend() const { return const_iterator(start, host, Ns::first); }
  constexpr static std::size_t size() { return Ns::first; }
  constexpr static std::size_t length() { return Ns::first; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class MutFixedBitFieldArrayViewIterator
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
                         MutLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         MutLooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  constexpr static std::size_t BitStart() { return start; }

  MutFixedBitFieldArrayViewIterator(host_t& host_, const std::size_t idx_) : host(host_), idx(idx_)
  {
  }

  // Required by std::input_or_output_iterator
  constexpr MutFixedBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr MutFixedBitFieldArrayViewIterator(const MutFixedBitFieldArrayViewIterator&) = default;
  MutFixedBitFieldArrayViewIterator& operator=(const MutFixedBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr MutFixedBitFieldArrayViewIterator(MutFixedBitFieldArrayViewIterator&&) = default;
  MutFixedBitFieldArrayViewIterator& operator=(MutFixedBitFieldArrayViewIterator&&) = default;

  MutFixedBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  MutFixedBitFieldArrayViewIterator operator++(int)
  {
    MutFixedBitFieldArrayViewIterator other{*this};
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const MutFixedBitFieldArrayViewIterator& other) const
  {
    return idx == other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, std::size_t start,
          class host_t>
class ConFixedBitFieldArrayViewIterator
{
protected:
  const host_t& host;
  std::size_t idx;

public:
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  constexpr static std::size_t BitStart() { return start; }

  ConFixedBitFieldArrayViewIterator(const host_t& host_, const std::size_t idx_)
      : host(host_), idx(idx_)
  {
  }

  // Required by std::input_or_output_iterator
  constexpr ConFixedBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr ConFixedBitFieldArrayViewIterator(const ConFixedBitFieldArrayViewIterator&) = default;
  ConFixedBitFieldArrayViewIterator& operator=(const ConFixedBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr ConFixedBitFieldArrayViewIterator(ConFixedBitFieldArrayViewIterator&&) = default;
  ConFixedBitFieldArrayViewIterator& operator=(ConFixedBitFieldArrayViewIterator&&) = default;

  ConFixedBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  ConFixedBitFieldArrayViewIterator operator++(int)
  {
    ConFixedBitFieldArrayViewIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const ConFixedBitFieldArrayViewIterator other) const
  {
    return idx == other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class MutLooseBitFieldArrayViewIterator
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
                         MutLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         MutLooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  std::size_t BitStart() { return start; }

  MutLooseBitFieldArrayViewIterator(const std::size_t start_, host_t& host_, const std::size_t idx_)
      : host(host_), idx(idx_), start(start_)
  {
  }

  // Required by std::input_or_output_iterator
  constexpr MutLooseBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr MutLooseBitFieldArrayViewIterator(const MutLooseBitFieldArrayViewIterator&) = default;
  MutLooseBitFieldArrayViewIterator& operator=(const MutLooseBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr MutLooseBitFieldArrayViewIterator(MutLooseBitFieldArrayViewIterator&&) = default;
  MutLooseBitFieldArrayViewIterator& operator=(MutLooseBitFieldArrayViewIterator&&) = default;

  MutLooseBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  MutLooseBitFieldArrayViewIterator operator++(int)
  {
    MutLooseBitFieldArrayViewIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const MutLooseBitFieldArrayViewIterator& other) const
  {
    return idx == other.idx;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

template <class field_t, AnyConstantPack<std::size_t> Ns, std::size_t width, class host_t>
class ConLooseBitFieldArrayViewIterator
{
protected:
  const host_t& host;
  std::size_t idx;
  const std::size_t start;

public:
  using iterator_category = std::input_iterator_tag;
  using value_type = field_t;
  using difference_type = ptrdiff_t;
  using pointer = void;
  using reference =
      std::conditional_t<(Ns::size() > 1),
                         ConLooseBitFieldArrayView<field_t, typename Ns::peel, width, host_t>,
                         ConLooseBitFieldView<field_t, width, host_t>>;

  using HostType = host_t;
  using FieldType = field_t;
  constexpr static std::size_t BitWidth() { return reference::BitWidth(); }
  std::size_t BitStart() { return start; }

  ConLooseBitFieldArrayViewIterator(const std::size_t start_, const host_t& host_,
                                    const std::size_t idx_)
      : host(host_), idx(idx_), start(start_)
  {
  }

  // Required by std::input_or_output_iterator
  constexpr ConLooseBitFieldArrayViewIterator() = default;
  // Copy constructor and assignment operators, required by LegacyIterator
  constexpr ConLooseBitFieldArrayViewIterator(const ConLooseBitFieldArrayViewIterator&) = default;
  ConLooseBitFieldArrayViewIterator& operator=(const ConLooseBitFieldArrayViewIterator&) = default;
  // Move constructor and assignment operators, explicitly defined for completeness
  constexpr ConLooseBitFieldArrayViewIterator(ConLooseBitFieldArrayViewIterator&&) = default;
  ConLooseBitFieldArrayViewIterator& operator=(ConLooseBitFieldArrayViewIterator&&) = default;

  ConLooseBitFieldArrayViewIterator& operator++()
  {
    ++idx;
    return *this;
  }
  ConLooseBitFieldArrayViewIterator operator++(int)
  {
    ConLooseBitFieldArrayViewIterator other(*this);
    ++*this;
    return other;
  }
  constexpr reference operator*() const
  {
    return reference(start + reference::BitWidth() * idx, host);
  }
  constexpr bool operator==(const ConLooseBitFieldArrayViewIterator other) const
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

template <Common::BitFieldView T>
struct fmt::formatter<T>
{
  fmt::formatter<typename T::FieldType> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const T& ref, FormatContext& ctx) const
  {
    return m_formatter.format(ref.Get(), ctx);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
