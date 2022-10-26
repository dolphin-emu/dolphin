// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/Concepts.h"

#include "Common/Future/CppLibConcepts.h"

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
/// Calculates a least-significant bit mask in four operations, assuming the width is never zero.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @param  width  The width of the bit mask.  Must not equal zero to avoid UB.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T>
constexpr T CalcLowMaskFast(const size_t width) noexcept
{
  DEBUG_ASSERT(width > 0);              // width == 0 causes undefined behavior
  DEBUG_ASSERT(width <= BitSize<T>());  // Bit width larger than BitSize<T>
  constexpr std::make_unsigned_t<T> ones = ~std::make_unsigned_t<T>{0};
  return static_cast<T>(ones >> (BitSize<T>() - width));
}

///
/// Calculates a least-significant bit mask in six or seven operations.
///
/// @tparam T      The type of the bit mask.  Must satisfy std::integral.
/// @param  width  The width of the bit mask.
///
/// @return A bit mask of the given width starting from the least-significant bit.
///
template <std::integral T>
constexpr T CalcLowMaskSafe(const size_t width) noexcept
{
  if (width != 0)
    return CalcLowMaskFast<T>(width);
  else [[unlikely]]
    return 0;
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
/// @param  width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <std::integral T>
constexpr std::make_unsigned_t<T> ExtractBitsU(const size_t width, const size_t start,
                                               const T host) noexcept
{
  DEBUG_ASSERT(start + width <= BitSize<T>());  // Bitfield out of range
  DEBUG_ASSERT(width > 0);                      // Bitfield is invalid (zero width)
  const T mask = CalcLowMaskFast<T>(width);
  return static_cast<std::make_unsigned_t<T>>((host >> start) & mask);
}

///
/// Extracts a zero-extended range of bits from a value.
///
/// @tparam width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t width, std::integral T>
constexpr std::make_unsigned_t<T> ExtractBitsU(const size_t start, const T host) noexcept
{
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  return ExtractBitsU(width, start, host);
}

///
/// Extracts a zero-extended range of bits from a value.
///
/// @tparam width  The width of the bit range in bits.
/// @tparam start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t width, size_t start, std::integral T>
constexpr std::make_unsigned_t<T> ExtractBitsU(const T host) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  return ExtractBitsU<width>(start, host);
}

///
/// Extracts a sign-extended range of bits from a value.
///
/// @param  width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <std::integral T>
constexpr std::make_signed_t<T> ExtractBitsS(const size_t width, const size_t start,
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
/// @tparam width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t width, std::integral T>
constexpr std::make_signed_t<T> ExtractBitsS(const size_t start, const T host) noexcept
{
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  return ExtractBitsS(width, start, host);
}

///
/// Extracts a sign-extended range of bits from a value.
///
/// @tparam width  The width of the bit range in bits.
/// @tparam start  The beginning of the bit range. Bit 0 is the least-significant bit.
/// @param  host   The host value to extract the bits from. Must satisfy std::integral.
///
/// @return The extracted bits.
///
template <size_t width, size_t start, std::integral T>
constexpr std::make_signed_t<T> ExtractBitsS(const T host) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  return ExtractBitsS<width>(start, host);
}

///
/// Inserts a bit into a value.
///
/// @param  start  The index of the bit. Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the bits into. Must satisfy std::integral.
/// @param  val    The boolean value which is inserted into the host.
///
template <std::integral T>
void InsertBit(size_t start, T& host, bool val) noexcept
{
  // Take advantage of the inherent bit-size of a C++ boolean to skip a mask.  This is a variant of
  // https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
  DEBUG_ASSERT(start < BitSize<T>());  // Bit out of range
  const T mask = (T{1} << start);
  host = (host & ~mask) | (static_cast<T>(val) << start);
}

///
/// Inserts a bit into a value.
///
/// @tparam start  The index of the bit. Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the bits into.  Must satisfy std::integral.
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
/// @param  width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range.  Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the field bits into.  Must satisfy std::integral.
/// @param  val    The value which is inserted into the host.
///
template <std::integral T>
constexpr void InsertBits(const size_t width, const size_t start, T& host, const auto val) noexcept
{
  // https://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching
  DEBUG_ASSERT(start + width <= BitSize<T>());  // BitField out of range
  DEBUG_ASSERT(width > 0);                      // BitField is invalid (zero width)
  const T mask = CalcLowMaskFast<T>(width) << start;
  host = (host & ~mask) | ((static_cast<T>(val) << start) & mask);
}

///
/// Inserts a range of bits into a value.
///
/// @tparam width  The width of the bit range in bits.
/// @param  start  The beginning of the bit range.  Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the field bits into.  Must satisfy std::integral.
/// @param  val    The value which is inserted into the host.
///
template <size_t width, std::integral T>
constexpr void InsertBits(const size_t start, T& host, const auto val) noexcept
{
  static_assert(width > 0, "Bitfield is invalid (zero width)");
  InsertBits(width, start, host, val);
}

///
/// Inserts a range of bits into a value.
///
/// @tparam width  The width of the bit range in bits.
/// @tparam start  The beginning of the bit range.  Bit 0 is the least-significant bit.
/// @param  host   The host value to insert the field bits into.  Must satisfy std::integral.
/// @param  val    The value which is inserted into the host.
///
template <size_t width, size_t start, std::integral T>
constexpr void InsertBits(T& host, const auto val) noexcept
{
  static_assert(start + width <= BitSize<T>(), "Bitfield out of range");
  InsertBits<width>(start, host, val);
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
template <std::unsigned_integral T>
constexpr bool IsValidLowMask(const T mask) noexcept
{
  // Signed masks can introduce hard to find bugs.

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
template <TriviallyCopyable To, TriviallyCopyable From>
inline To BitCast(const From& source) noexcept
{
  static_assert(sizeof(From) == sizeof(To),
                "BitCast source and destination types must be equal in size.");

  alignas(To) std::byte storage[sizeof(To)];
  std::memcpy(&storage, &source, sizeof(storage));
  return reinterpret_cast<To&>(storage);
}

template <TriviallyCopyable T, TriviallyCopyable PtrType>
class BitCastPtrType
{
public:
  explicit BitCastPtrType(PtrType* ptr) : m_ptr(ptr) {}

  // Enable operator= only for pointers to non-const data
  inline BitCastPtrType& operator=(const T& source) requires NotConst<PtrType>
  {
    std::memcpy(m_ptr, &source, sizeof(source));
    return *this;
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
template <typename ArrayType, TriviallyCopyable T,
          TriviallyCopyable Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline auto BitCastToArray(const T& obj) noexcept -> Container
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of source type.");

  Container result;
  std::memcpy(result.data(), &obj, sizeof(T));
  return result;
}

template <typename ArrayType, TriviallyCopyable T,
          TriviallyCopyable Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline void BitCastFromArray(const Container& array, T& obj) noexcept
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of destination type.");

  std::memcpy(&obj, array.data(), sizeof(T));
}

template <typename ArrayType, TriviallyCopyable T,
          TriviallyCopyable Container = std::array<ArrayType, sizeof(T) / sizeof(ArrayType)>>
inline auto BitCastFromArray(const Container& array) noexcept -> T
{
  static_assert(sizeof(T) % sizeof(ArrayType) == 0,
                "Size of array type must be a factor of size of destination type.");

  T obj;
  std::memcpy(&obj, array.data(), sizeof(T));
  return obj;
}

template <std::integral T>
constexpr void SetBit(const size_t start, T& host)
{
  DEBUG_ASSERT(start < BitSize<T>());  // Bit out of range
  host |= (T{1} << start);
}

template <size_t start, std::integral T>
constexpr void SetBit(T& host)
{
  static_assert(start < BitSize<T>(), "Bit out of range");
  SetBit(start, host);
}

template <std::integral T>
constexpr void ClearBit(const size_t start, T& host)
{
  DEBUG_ASSERT(start < BitSize<T>());  // Bit out of range
  host &= ~(T{1} << start);
}

template <size_t start, std::integral T>
constexpr void ClearBit(T& host)
{
  static_assert(start < BitSize<T>(), "Bit out of range");
  ClearBit(start, host);
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
template <std::unsigned_integral T>
T ExpandValue(T value, size_t left_shift_amount)
{
  return (value << left_shift_amount) |
         (T(-static_cast<T>(ExtractBit<0>(value))) >> (BitSize<T>() - left_shift_amount));
}
}  // namespace Common
