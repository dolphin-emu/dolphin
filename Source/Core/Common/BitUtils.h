// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <cstddef>
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
}  // namespace Common
