// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "Common/CommonTypes.h"

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
