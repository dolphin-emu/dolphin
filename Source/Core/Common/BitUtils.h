// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <type_traits>

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
/// Extracts a bit from a value.
///
/// @param  src The value to extract a bit from.
/// @param  bit The bit to extract.
///
/// @tparam T   The type of the value.
///
/// @return The extracted bit.
///
template <typename T>
constexpr T ExtractBit(const T src, const size_t bit) noexcept
{
  return (src >> bit) & static_cast<T>(1);
}

///
/// Extracts a bit from a value.
///
/// @param  src The value to extract a bit from.
///
/// @tparam bit The bit to extract.
/// @tparam T   The type of the value.
///
/// @return The extracted bit.
///
template <size_t bit, typename T>
constexpr T ExtractBit(const T src) noexcept
{
  static_assert(bit < BitSize<T>(), "Specified bit must be within T's bit width.");

  return ExtractBit(src, bit);
}

///
/// Extracts a range of bits from a value.
///
/// @param  src    The value to extract the bits from.
/// @param  begin  The beginning of the bit range. This is inclusive.
/// @param  end    The ending of the bit range. This is inclusive.
///
/// @tparam T      The type of the value.
/// @tparam Result The returned result type. This is the unsigned analog
///                of a signed type if a signed type is passed as T.
///
/// @return The extracted bits.
///
template <typename T, typename Result = std::make_unsigned_t<T>>
constexpr Result ExtractBits(const T src, const size_t begin, const size_t end) noexcept
{
  return static_cast<Result>(((static_cast<Result>(src) << ((BitSize<T>() - 1) - end)) >>
                              (BitSize<T>() - end + begin - 1)));
}

///
/// Extracts a range of bits from a value.
///
/// @param  src    The value to extract the bits from.
///
/// @tparam begin  The beginning of the bit range. This is inclusive.
/// @tparam end    The ending of the bit range. This is inclusive.
/// @tparam T      The type of the value.
/// @tparam Result The returned result type. This is the unsigned analog
///                of a signed type if a signed type is passed as T.
///
/// @return The extracted bits.
///
template <size_t begin, size_t end, typename T, typename Result = std::make_unsigned_t<T>>
constexpr Result ExtractBits(const T src) noexcept
{
  static_assert(begin < end, "Beginning bit must be less than the ending bit.");
  static_assert(begin < BitSize<T>(), "Beginning bit is larger than T's bit width.");
  static_assert(end < BitSize<T>(), "Ending bit is larger than T's bit width.");

  return ExtractBits<T, Result>(src, begin, end);
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
void SetBit(T& value, size_t bit_number, bool bit_value)
{
  static_assert(std::is_unsigned<T>(), "SetBit is only sane on unsigned types.");

  if (bit_value)
    value |= (T{1} << bit_number);
  else
    value &= ~(T{1} << bit_number);
}

template <size_t bit_number, typename T>
void SetBit(T& value, bool bit_value)
{
  SetBit(value, bit_number, bit_value);
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
         (T(-ExtractBit<0>(value)) >> (BitSize<T>() - left_shift_amount));
}
}  // namespace Common
