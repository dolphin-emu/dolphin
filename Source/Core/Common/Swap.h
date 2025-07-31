// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstring>
#include <type_traits>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#elif defined(__linux__)
#include <byteswap.h>
#elif defined(__FreeBSD__)
#include <sys/endian.h>
#elif defined(__OpenBSD__)
#include <endian.h>
#endif

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"

namespace Common
{
inline u8 swap8(u8 data)
{
  return data;
}
inline u32 swap24(const u8* data)
{
  return (data[0] << 16) | (data[1] << 8) | data[2];
}

#if defined(ANDROID) || defined(__OpenBSD__)
#undef swap16
#undef swap32
#undef swap64
#endif

#ifdef _WIN32
inline u16 swap16(u16 data)
{
  return _byteswap_ushort(data);
}
inline u32 swap32(u32 data)
{
  return _byteswap_ulong(data);
}
inline u64 swap64(u64 data)
{
  return _byteswap_uint64(data);
}
#elif __linux__
inline u16 swap16(u16 data)
{
  return bswap_16(data);
}
inline u32 swap32(u32 data)
{
  return bswap_32(data);
}
inline u64 swap64(u64 data)
{
  return bswap_64(data);
}
#elif __APPLE__
inline __attribute__((always_inline)) u16 swap16(u16 data)
{
  return OSSwapInt16(data);
}
inline __attribute__((always_inline)) u32 swap32(u32 data)
{
  return OSSwapInt32(data);
}
inline __attribute__((always_inline)) u64 swap64(u64 data)
{
  return OSSwapInt64(data);
}
#elif __FreeBSD__
inline u16 swap16(u16 data)
{
  return bswap16(data);
}
inline u32 swap32(u32 data)
{
  return bswap32(data);
}
inline u64 swap64(u64 data)
{
  return bswap64(data);
}
#else
// Slow generic implementation.
inline u16 swap16(u16 data)
{
  return (data >> 8) | (data << 8);
}
inline u32 swap32(u32 data)
{
  return (swap16(data) << 16) | swap16(data >> 16);
}
inline u64 swap64(u64 data)
{
  return ((u64)swap32(data) << 32) | swap32(data >> 32);
}
#endif

inline u16 swap16(const u8* data)
{
  u16 value;
  std::memcpy(&value, data, sizeof(u16));

  return swap16(value);
}
inline u32 swap32(const u8* data)
{
  u32 value;
  std::memcpy(&value, data, sizeof(u32));

  return swap32(value);
}
inline u64 swap64(const u8* data)
{
  u64 value;
  std::memcpy(&value, data, sizeof(u64));

  return swap64(value);
}

template <int count>
void swap(u8*);

template <>
inline void swap<1>(u8* data)
{
}

template <>
inline void swap<2>(u8* data)
{
  const u16 value = swap16(data);

  std::memcpy(data, &value, sizeof(u16));
}

template <>
inline void swap<4>(u8* data)
{
  const u32 value = swap32(data);

  std::memcpy(data, &value, sizeof(u32));
}

template <>
inline void swap<8>(u8* data)
{
  const u64 value = swap64(data);

  std::memcpy(data, &value, sizeof(u64));
}

template <typename T>
inline T FromBigEndian(T data)
{
  static_assert(std::is_arithmetic<T>::value, "function only makes sense with arithmetic types");

  swap<sizeof(data)>(reinterpret_cast<u8*>(&data));
  return data;
}

#ifdef __AVX__
// Byte-swap patterns for PSHUFB.
template <size_t ByteSize>
inline __m128i GetSwapShuffle128()
{
  if constexpr (ByteSize == 2)
    return _mm_set_epi64x(0x0e0f0c0d0a0b0809, 0x0607040502030001);
  else if constexpr (ByteSize == 4)
    return _mm_set_epi64x(0x0c0d0e0f08090a0b, 0x0405060700010203);
  else if constexpr (ByteSize == 8)
    return _mm_set_epi64x(0x08090a0b0c0d0e0f, 0x0001020304050607);
  else
    static_assert(false);
}
#endif

#ifdef __AVX2__
// Byte-swap patterns for VPSHUFB.
template <size_t ByteSize>
inline __m256i GetSwapShuffle256()
{
  __m128i pattern = GetSwapShuffle128<ByteSize>();
  return _mm256_set_m128i(pattern, pattern);
}
#endif

// Templated functions for byteswapped copies.
template <typename T>
inline void CopySwapped(T* dst, const T* src, size_t byte_size)
{
  constexpr size_t S = sizeof(T);
  const size_t count = byte_size / S;
  size_t i = 0;

#ifdef __AVX2__
  for (; i + 32 / S <= count; i += 32 / S)
  {
    const auto vdst = reinterpret_cast<__m256i*>(dst + i);
    const auto vsrc = reinterpret_cast<const __m256i*>(src + i);
    const auto swap = GetSwapShuffle256<S>();
    _mm256_storeu_si256(vdst, _mm256_shuffle_epi8(_mm256_loadu_si256(vsrc), swap));
  }
#endif

#ifdef __AVX__
  for (; i + 16 / S <= count; i += 16 / S)
  {
    const auto vdst = reinterpret_cast<__m128i*>(dst + i);
    const auto vsrc = reinterpret_cast<const __m128i*>(src + i);
    const auto swap = GetSwapShuffle128<S>();
    _mm_storeu_si128(vdst, _mm_shuffle_epi8(_mm_loadu_si128(vsrc), swap));
  }
#endif

  for (; i < count; ++i)
    dst[i] = Common::FromBigEndian(src[i]);
}

template <typename value_type>
struct BigEndianValue
{
  static_assert(std::is_arithmetic<value_type>(), "value_type must be an arithmetic type");
  BigEndianValue() = default;
  explicit BigEndianValue(value_type val) { *this = val; }
  operator value_type() const { return FromBigEndian(raw); }
  BigEndianValue& operator=(value_type v)
  {
    raw = FromBigEndian(v);
    return *this;
  }

private:
  value_type raw;
};
}  // Namespace Common

template <typename value_type>
struct fmt::formatter<Common::BigEndianValue<value_type>>
{
  fmt::formatter<value_type> m_formatter;
  constexpr auto parse(format_parse_context& ctx) { return m_formatter.parse(ctx); }
  template <typename FormatContext>
  auto format(const Common::BigEndianValue<value_type>& value, FormatContext& ctx) const
  {
    return m_formatter.format(value.operator value_type(), ctx);
  }
};
