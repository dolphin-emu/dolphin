// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>

#include "Common/Assert.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace Common
{
///
/// Retrieves the size of a type in bits.
///
/// @tparam T Type to get the size of.
///
/// @return the size of the type in bits.
///
template <typename T>
constexpr size_t BitSize() noexcept
{
  return sizeof(T) * CHAR_BIT;
}

///
/// Calculates a least-significant bit mask in four operations.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @param  width  The width of the bit mask.  Must not equal zero to avoid UB.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T>
constexpr T CalcBitMaskFast(const size_t width) noexcept
{
  DEBUG_ASSERT(width <= BitSize<T>());  // Bit width larger than BitSize<T>
  DEBUG_ASSERT(width > 0);              // width == 0 causes undefined behavior
  constexpr auto max = std::numeric_limits<std::make_unsigned_t<T>>::max();
  return static_cast<T>(max >> (BitSize<T>() - width));
}
///
/// Calculates a least-significant bit mask in four operations.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @tparam width  The width of the bit mask.  Must not equal zero to avoid UB.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T, size_t width>
constexpr T CalcBitMaskFast() noexcept
{
  static_assert(width <= BitSize<T>(), "Bit width larger than BitSize<T>");
  static_assert(width > 0, "width == 0 causes undefined behavior");
  return CalcBitMaskFast<T>(width);
}
///
/// Calculates a least-significant bit mask in four operations + a special case for width == 0.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @tparam width  The width of the bit mask.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T>
constexpr T CalcBitMaskSafe(const size_t width) noexcept
{
  DEBUG_ASSERT(width <= BitSize<T>());  // Bit width larger than BitSize<T>
  if (width)
    return CalcBitMaskFast<T>(width);
  else [[unlikely]]
    return 0;
}
///
/// Calculates a least-significant bit mask in four operations + a special case for width == 0.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @param width   The width of the bit mask.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T, size_t width>
constexpr T CalcBitMaskSafe() noexcept
{
  static_assert(width <= BitSize<T>(), "Bit width larger than BitSize<T>");
  return CalcBitMaskSafe<T>(width);
}

///
/// Extracts a bit from a value.
///
/// @param  start  The bit to extract. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bit from. Must satisfy std::integral.
///
/// @return The extracted bit.
///
template <std::integral T>
constexpr bool ExtractBit(const size_t start, const T host) noexcept
{
  DEBUG_ASSERT(start < BitSize<T>());  // Bit start out of range
  return static_cast<bool>((host >> start) & T{1});
}

///
/// Extracts a bit from a value.
///
/// @tparam start  The bit to extract. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bit from. Must satisfy std::integral.
///
/// @return The extracted bit.
///
template <size_t start, std::integral T>
constexpr bool ExtractBit(const T host) noexcept
{
  static_assert(start < BitSize<T>(), "Bit start out of range");
  return ExtractBit(start, host);
}

// TODO C++23: The x86 CPU extension "Bit Manipulation Instruction Set 1" provides unsigned bit
// field extraction instructions which could theoretically be faster than the idiomatic way of doing
// this. However, the only way to create specializations using BEXTR intrinsics while remaining
// constexpr requires consteval if statements. Even then, BMI1 is from 2012-ish (SSE2 is Dolphin
// Emulator's only hard-required x86 CPU extension atm), so these specializations would only really
// be available to end-users using GCC/Clang who enable -march=native when compiling.
// Bit Manipulation Instruction Set 2's PEXT/PDEP instructions also are worth looking at.

///
/// Extracts a zero-extended range of bits from a value.
///
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  width  The width of the bit range in bits.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <std::integral T>
constexpr std::make_unsigned_t<T> ExtractBitsU(const size_t start, const size_t width,
                                               const T host) noexcept
{
  // TODO: would pre-calculating the mask would be faster than calculating it at run-time?
  DEBUG_ASSERT(start + width <= BitSize<T>());  // Bitfield out of range
  DEBUG_ASSERT(width > 0);                      // Bitfield is invalid (zero width)
  const T mask = CalcBitMaskFast<T>(width);
  return static_cast<std::make_unsigned_t<T>>((host >> start) & mask);
}

///
/// Extracts a zero-extended range of bits from a value.
///
/// @tparam start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @tparam width  The width of the bit range in bits.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t start, size_t width, std::integral T>
constexpr std::make_unsigned_t<T> ExtractBitsU(const T host) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  return ExtractBitsU(start, width, host);
}

///
/// Extracts a sign-extended range of bits from a value.
///
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  width  The width of the bit range in bits.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <std::integral T>
constexpr std::make_signed_t<T> ExtractBitsS(const size_t start, const size_t width,
                                             const T host) noexcept
{
  // This idiom was both undefined and implementation-defined behavior up until C++20.
  // https://en.cppreference.com/w/cpp/language/operator_arithmetic#Bitwise_shift_operators
  DEBUG_ASSERT(start + width <= BitSize<T>());  // Bitfield out of range
  DEBUG_ASSERT(width > 0);                      // Bitfield is invalid (zero width)
  const size_t rshift = BitSize<T>() - width;
  const size_t lshift = rshift - start;
  return static_cast<std::make_signed_t<T>>(host << lshift) >> rshift;
}

///
/// Extracts a sign-extended range of bits from a value.
///
/// @tparam start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @tparam width  The width of the bit range in bits.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t start, size_t width, std::integral T>
constexpr std::make_signed_t<T> ExtractBitsS(const T host) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  return ExtractBitsS(start, width, host);
}

///
/// Inserts a bits into a value.
///
/// @param  start  The index of the bit. Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the bits into. Must satisfy std::integral.
/// @param  val    The boolean value which is inserted into the host.
///
template <std::integral T>
void InsertBit(size_t start, T& host, bool val) noexcept
{
  DEBUG_ASSERT(start < BitSize<T>());  // Bit out of range
  const T mask = (T{1} << start);
  host = (host & ~mask) | (static_cast<T>(val) << start);
}

///
/// Inserts a bits into a value.
///
/// @tparam start  The index of the bit. Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the bits into. Must satisfy std::integral.
/// @param  val    The boolean value which is inserted into the host.
///
template <size_t start, std::integral T>
void InsertBit(T& host, bool val) noexcept
{
  static_assert(start < BitSize<T>(), "Bit out of range");
  InsertBit(start, host, val);
}

///
/// Inserts a range of bits into a value.
///
/// @tparam start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @tparam width  The width of the bit range in bits.
/// @param  host   The host value to insert the field bits into. Must satisfy std::integral.
/// @param  val    The value which is inserted into the host. This auto type parameter must be able
/// to static_cast to the same type as the host value.
///
template <std::integral T>
constexpr void InsertBits(const size_t start, const size_t width, T& host, const auto val) noexcept
{
  // TODO: would pre-calculating the mask would be faster than calculating it at run-time?
  DEBUG_ASSERT(start + width <= BitSize<T>());  // BitField out of range
  DEBUG_ASSERT(width > 0);                      // BitField is invalid (zero width)
  const T mask = CalcBitMaskFast<T>(width) << start;
  host = (host & ~mask) | ((static_cast<T>(val) << start) & mask);
}

///
/// Inserts a range of bits into a value.
///
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  width  The width of the bit range in bits.
/// @param  host   The host value to insert the field bits into. Must satisfy std::integral.
/// @param  val    The value which is inserted into the host. This auto type parameter must be able
/// to static_cast to the same type as the host value.
///
template <size_t start, size_t width, std::integral T>
constexpr void InsertBits(T& host, const auto val) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  InsertBits(start, width, host, val);
}

///
/// Rotates a value left (ROL).
///
/// @param  value  The value to rotate.
/// @param  amount The number of bits to rotate the value.
/// @tparam T      An unsigned type.
///
/// @return The rotated value.
///
template <typename T>
constexpr T RotateLeft(const T value, size_t amount) noexcept
{
  static_assert(std::is_unsigned<T>(), "Can only rotate unsigned types left.");

  amount %= BitSize<T>();

  if (amount == 0)
    return value;

  return static_cast<T>((value << amount) | (value >> (BitSize<T>() - amount)));
}

///
/// Rotates a value right (ROR).
///
/// @param  value  The value to rotate.
/// @param  amount The number of bits to rotate the value.
/// @tparam T      An unsigned type.
///
/// @return The rotated value.
///
template <typename T>
constexpr T RotateRight(const T value, size_t amount) noexcept
{
  static_assert(std::is_unsigned<T>(), "Can only rotate unsigned types right.");

  amount %= BitSize<T>();

  if (amount == 0)
    return value;

  return static_cast<T>((value >> amount) | (value << (BitSize<T>() - amount)));
}

///
/// Verifies whether the supplied value is a valid bit mask of the form 0b00...0011...11.
/// Both edge cases of all zeros and all ones are considered valid masks, too.
///
/// @param  mask The mask value to test for validity.
///
/// @tparam T    The type of the value.
///
/// @return A bool indicating whether the mask is valid.
///
template <typename T>
constexpr bool IsValidLowMask(const T mask) noexcept
{
  static_assert(std::is_integral<T>::value, "Mask must be an integral type.");
  static_assert(std::is_unsigned<T>::value, "Signed masks can introduce hard to find bugs.");

  // Can be efficiently determined without looping or bit counting. It's the counterpart
  // to https://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
  // and doesn't require special casing either edge case.
  return (mask & (mask + 1)) == 0;
}

///
/// Reinterpret objects of one type as another by bit-casting between object representations.
///
/// @remark This is the example implementation of std::bit_cast which is to be included
///         in C++2a. See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0476r2.html
///         for more details. The only difference is this variant is not constexpr,
///         as the mechanism for bit_cast requires a compiler built-in to have that quality.
///
/// @param source The source object to convert to another representation.
///
/// @tparam To   The type to reinterpret source as.
/// @tparam From The initial type representation of source.
///
/// @return The representation of type From as type To.
///
/// @pre Both To and From types must be the same size
/// @pre Both To and From types must satisfy the TriviallyCopyable concept.
///
template <typename To, typename From>
inline To BitCast(const From& source) noexcept
{
  static_assert(sizeof(From) == sizeof(To),
                "BitCast source and destination types must be equal in size.");
  static_assert(std::is_trivially_copyable<From>(),
                "BitCast source type must be trivially copyable.");
  static_assert(std::is_trivially_copyable<To>(),
                "BitCast destination type must be trivially copyable.");

  alignas(To) std::byte storage[sizeof(To)];
  std::memcpy(&storage, &source, sizeof(storage));
  return reinterpret_cast<To&>(storage);
}

template <typename T, typename PtrType>
class BitCastPtrType
{
public:
  static_assert(std::is_trivially_copyable<PtrType>(),
                "BitCastPtr source type must be trivially copyable.");
  static_assert(std::is_trivially_copyable<T>(),
                "BitCastPtr destination type must be trivially copyable.");

  explicit BitCastPtrType(PtrType* ptr) : m_ptr(ptr) {}

  // Enable operator= only for pointers to non-const data
  template <typename S>
  inline typename std::enable_if<std::is_same<S, T>() && !std::is_const<PtrType>()>::type
  operator=(const S& source)
  {
    std::memcpy(m_ptr, &source, sizeof(source));
  }

  inline operator T() const
  {
    T result;
    std::memcpy(&result, m_ptr, sizeof(result));
    return result;
  }

private:
  PtrType* m_ptr;
};

// Provides an aliasing-safe alternative to reinterpret_cast'ing pointers to structs
// Conversion constructor and operator= provided for a convenient syntax.
// Usage: MyStruct s = BitCastPtr<MyStruct>(some_ptr);
// BitCastPtr<MyStruct>(some_ptr) = s;
template <typename T, typename PtrType>
inline auto BitCastPtr(PtrType* ptr) noexcept -> BitCastPtrType<T, PtrType>
{
  return BitCastPtrType<T, PtrType>{ptr};
}

// Similar to BitCastPtr, but specifically for aliasing structs to arrays.
template <typename ArrayType, typename T,
          typename Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline auto BitCastToArray(const T& obj) noexcept -> Container
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of source type.");
  static_assert(std::is_trivially_copyable<T>(),
                "BitCastToArray source type must be trivially copyable.");
  static_assert(std::is_trivially_copyable<Container>(),
                "BitCastToArray array type must be trivially copyable.");

  Container result;
  std::memcpy(result.data(), &obj, sizeof(T));
  return result;
}

template <typename ArrayType, typename T,
          typename Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline void BitCastFromArray(const Container& array, T& obj) noexcept
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of destination type.");
  static_assert(std::is_trivially_copyable<Container>(),
                "BitCastFromArray array type must be trivially copyable.");
  static_assert(std::is_trivially_copyable<T>(),
                "BitCastFromArray destination type must be trivially copyable.");

  std::memcpy(&obj, array.data(), sizeof(T));
}

template <typename ArrayType, typename T,
          typename Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline auto BitCastFromArray(const Container& array) noexcept -> T
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of destination type.");
  static_assert(std::is_trivially_copyable<Container>(),
                "BitCastFromArray array type must be trivially copyable.");
  static_assert(std::is_trivially_copyable<T>(),
                "BitCastFromArray destination type must be trivially copyable.");

  T obj;
  std::memcpy(&obj, array.data(), sizeof(T));
  return obj;
}

template <typename T>
class FlagBit
{
public:
  FlagBit(std::underlying_type_t<T>& bits, T bit) : m_bits(bits), m_bit(bit) {}
  explicit operator bool() const
  {
    return (m_bits & static_cast<std::underlying_type_t<T>>(m_bit)) != 0;
  }
  FlagBit& operator=(const bool rhs)
  {
    if (rhs)
      m_bits |= static_cast<std::underlying_type_t<T>>(m_bit);
    else
      m_bits &= ~static_cast<std::underlying_type_t<T>>(m_bit);
    return *this;
  }

private:
  std::underlying_type_t<T>& m_bits;
  T m_bit;
};

template <typename T>
class Flags
{
public:
  constexpr Flags() = default;
  constexpr Flags(std::initializer_list<T> bits)
  {
    for (auto bit : bits)
    {
      m_hex |= static_cast<std::underlying_type_t<T>>(bit);
    }
  }
  FlagBit<T> operator[](T bit) { return FlagBit(m_hex, bit); }

  std::underlying_type_t<T> m_hex = 0;
};

// Left-shift a value and set new LSBs to that of the supplied LSB.
// Converts a value from a N-bit range to an (N+X)-bit range. e.g. 0x101 -> 0x10111
template <typename T>
T ExpandValue(T value, size_t left_shift_amount)
{
  static_assert(std::is_unsigned<T>(), "ExpandValue is only sane on unsigned types.");

  return (value << left_shift_amount) |
         (T(-static_cast<T>(ExtractBit<0>(value))) >> (BitSize<T>() - left_shift_amount));
}

template <typename T>
constexpr int CountLeadingZerosConst(T value)
{
  int result = sizeof(T) * 8;
  while (value)
  {
    result--;
    value >>= 1;
  }
  return result;
}

constexpr int CountLeadingZeros(uint64_t value)
{
#if defined(__GNUC__)
  return value ? __builtin_clzll(value) : 64;
#elif defined(_MSC_VER)
  if (std::is_constant_evaluated())
  {
    return CountLeadingZerosConst(value);
  }
  else
  {
    unsigned long index = 0;
    return _BitScanReverse64(&index, value) ? 63 - index : 64;
  }
#else
  return CountLeadingZerosConst(value);
#endif
}

constexpr int CountLeadingZeros(uint32_t value)
{
#if defined(__GNUC__)
  return value ? __builtin_clz(value) : 32;
#elif defined(_MSC_VER)
  if (std::is_constant_evaluated())
  {
    return CountLeadingZerosConst(value);
  }
  else
  {
    unsigned long index = 0;
    return _BitScanReverse(&index, value) ? 31 - index : 32;
  }
#else
  return CountLeadingZerosConst(value);
#endif
}

template <typename T>
constexpr int CountTrailingZerosConst(T value)
{
  int result = sizeof(T) * 8;
  while (value)
  {
    result--;
    value <<= 1;
  }
  return result;
}

constexpr int CountTrailingZeros(uint64_t value)
{
#if defined(__GNUC__)
  return value ? __builtin_ctzll(value) : 64;
#elif defined(_MSC_VER)
  if (std::is_constant_evaluated())
  {
    return CountTrailingZerosConst(value);
  }
  else
  {
    unsigned long index = 0;
    return _BitScanForward64(&index, value) ? index : 64;
  }
#else
  return CountTrailingZerosConst(value);
#endif
}

constexpr int CountTrailingZeros(uint32_t value)
{
#if defined(__GNUC__)
  return value ? __builtin_ctz(value) : 32;
#elif defined(_MSC_VER)
  if (std::is_constant_evaluated())
  {
    return CountTrailingZerosConst(value);
  }
  else
  {
    unsigned long index = 0;
    return _BitScanForward(&index, value) ? index : 32;
  }
#else
  return CountTrailingZerosConst(value);
#endif
}

}  // namespace Common
