// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Hash.h"

#include <algorithm>
#include <bit>
#include <cstring>

#include <zlib.h>

#include "Common/BitUtils.h"
#include "Common/CPUDetect.h"
#include "Common/CommonFuncs.h"
#include "Common/Intrinsics.h"

#ifdef _M_ARM_64
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <arm_acle.h>
#endif
#endif

namespace Common
{
u32 HashAdler32(const u8* data, size_t len)
{
  // Use fast implementation from zlib-ng
  return adler32_z(1, data, len);
}

// Stupid hash - but can't go back now :)
// Don't use for new things. At least it's reasonably fast.
u32 HashEctor(const u8* data, size_t len)
{
  u32 crc = 0;

  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    crc = (crc << 3) | (crc >> 29);
  }

  return crc;
}

#ifdef _ARCH_64

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

static u64 getblock(const u64* p, int i)
{
  return p[i];
}

//----------
// Block mix - combine the key bits with the hash bits and scramble everything

static void bmix64(u64& h1, u64& h2, u64& k1, u64& k2, u64& c1, u64& c2)
{
  k1 *= c1;
  k1 = std::rotl(k1, 23);
  k1 *= c2;
  h1 ^= k1;
  h1 += h2;

  h2 = std::rotl(h2, 41);

  k2 *= c2;
  k2 = std::rotl(k2, 23);
  k2 *= c1;
  h2 ^= k2;
  h2 += h1;

  h1 = h1 * 3 + 0x52dce729;
  h2 = h2 * 3 + 0x38495ab5;

  c1 = c1 * 5 + 0x7b7d159c;
  c2 = c2 * 5 + 0x6bce6396;
}

//----------
// Finalization mix - avalanches all bits to within 0.05% bias

static u64 fmix64(u64 k)
{
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccd;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53;
  k ^= k >> 33;

  return k;
}

static u64 GetMurmurHash3(const u8* src, u32 len, u32 samples)
{
  const u8* data = (const u8*)src;
  const int nblocks = len / 16;
  u32 Step = (len / 8);
  if (samples == 0)
    samples = std::max(Step, 1u);
  Step = Step / samples;
  if (Step < 1)
    Step = 1;

  u64 h1 = 0x9368e53c2f6af274;
  u64 h2 = 0x586dcd208f7cd3fd;

  u64 c1 = 0x87c37b91114253d5;
  u64 c2 = 0x4cf5ad432745937f;

  //----------
  // body

  const u64* blocks = (const u64*)(data);

  for (int i = 0; i < nblocks; i += Step)
  {
    u64 k1 = getblock(blocks, i * 2 + 0);
    u64 k2 = getblock(blocks, i * 2 + 1);

    bmix64(h1, h2, k1, k2, c1, c2);
  }

  //----------
  // tail

  const u8* tail = (const u8*)(data + nblocks * 16);

  u64 k1 = 0;
  u64 k2 = 0;

  switch (len & 15)
  {
  case 15:
    k2 ^= u64(tail[14]) << 48;
  case 14:
    k2 ^= u64(tail[13]) << 40;
  case 13:
    k2 ^= u64(tail[12]) << 32;
  case 12:
    k2 ^= u64(tail[11]) << 24;
  case 11:
    k2 ^= u64(tail[10]) << 16;
  case 10:
    k2 ^= u64(tail[9]) << 8;
  case 9:
    k2 ^= u64(tail[8]) << 0;

  case 8:
    k1 ^= u64(tail[7]) << 56;
  case 7:
    k1 ^= u64(tail[6]) << 48;
  case 6:
    k1 ^= u64(tail[5]) << 40;
  case 5:
    k1 ^= u64(tail[4]) << 32;
  case 4:
    k1 ^= u64(tail[3]) << 24;
  case 3:
    k1 ^= u64(tail[2]) << 16;
  case 2:
    k1 ^= u64(tail[1]) << 8;
  case 1:
    k1 ^= u64(tail[0]) << 0;
    bmix64(h1, h2, k1, k2, c1, c2);
  };

  //----------
  // finalization

  h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix64(h1);
  h2 = fmix64(h2);

  h1 += h2;

  return h1;
}

#else

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

static u32 getblock(const u32* p, int i)
{
  return p[i];
}

//----------
// Finalization mix - force all bits of a hash block to avalanche

// avalanches all bits to within 0.25% bias

static u32 fmix32(u32 h)
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

static void bmix32(u32& h1, u32& h2, u32& k1, u32& k2, u32& c1, u32& c2)
{
  k1 *= c1;
  k1 = std::rotl(k1, 11);
  k1 *= c2;
  h1 ^= k1;
  h1 += h2;

  h2 = std::rotl(h2, 17);

  k2 *= c2;
  k2 = std::rotl(k2, 11);
  k2 *= c1;
  h2 ^= k2;
  h2 += h1;

  h1 = h1 * 3 + 0x52dce729;
  h2 = h2 * 3 + 0x38495ab5;

  c1 = c1 * 5 + 0x7b7d159c;
  c2 = c2 * 5 + 0x6bce6396;
}

//----------

static u64 GetMurmurHash3(const u8* src, u32 len, u32 samples)
{
  const u8* data = (const u8*)src;
  u32 out[2];
  const int nblocks = len / 8;
  u32 Step = (len / 4);
  if (samples == 0)
    samples = std::max(Step, 1u);
  Step = Step / samples;
  if (Step < 1)
    Step = 1;

  u32 h1 = 0x8de1c3ac;
  u32 h2 = 0xbab98226;

  u32 c1 = 0x95543787;
  u32 c2 = 0x2ad7eb25;

  //----------
  // body

  const u32* blocks = (const u32*)(data + nblocks * 8);

  for (int i = -nblocks; i < 0; i += Step)
  {
    u32 k1 = getblock(blocks, i * 2 + 0);
    u32 k2 = getblock(blocks, i * 2 + 1);

    bmix32(h1, h2, k1, k2, c1, c2);
  }

  //----------
  // tail

  const u8* tail = (const u8*)(data + nblocks * 8);

  u32 k1 = 0;
  u32 k2 = 0;

  switch (len & 7)
  {
  case 7:
    k2 ^= tail[6] << 16;
  case 6:
    k2 ^= tail[5] << 8;
  case 5:
    k2 ^= tail[4] << 0;
  case 4:
    k1 ^= tail[3] << 24;
  case 3:
    k1 ^= tail[2] << 16;
  case 2:
    k1 ^= tail[1] << 8;
  case 1:
    k1 ^= tail[0] << 0;
    bmix32(h1, h2, k1, k2, c1, c2);
  };

  //----------
  // finalization

  h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix32(h1);
  h2 = fmix32(h2);

  h1 += h2;
  h2 += h1;

  out[0] = h1;
  out[1] = h2;

  return *((u64*)&out);
}

#endif

#if defined(_M_X86_64)

FUNCTION_TARGET_SSE42
static u64 GetHash64_SSE42_CRC32(const u8* src, u32 len, u32 samples)
{
  u64 h[4] = {len, 0, 0, 0};
  u32 Step = (len / 8);
  const u64* data = (const u64*)src;
  const u64* end = data + Step;
  if (samples == 0)
    samples = std::max(Step, 1u);
  Step = Step / samples;
  if (Step < 1)
    Step = 1;

  while (data < end - Step * 3)
  {
    h[0] = _mm_crc32_u64(h[0], data[Step * 0]);
    h[1] = _mm_crc32_u64(h[1], data[Step * 1]);
    h[2] = _mm_crc32_u64(h[2], data[Step * 2]);
    h[3] = _mm_crc32_u64(h[3], data[Step * 3]);
    data += Step * 4;
  }
  if (data < end - Step * 0)
    h[0] = _mm_crc32_u64(h[0], data[Step * 0]);
  if (data < end - Step * 1)
    h[1] = _mm_crc32_u64(h[1], data[Step * 1]);
  if (data < end - Step * 2)
    h[2] = _mm_crc32_u64(h[2], data[Step * 2]);

  if (len & 7)
  {
    u64 temp = 0;
    memcpy(&temp, end, len & 7);
    h[0] = _mm_crc32_u64(h[0], temp);
  }

  // FIXME: is there a better way to combine these partial hashes?
  return h[0] + (h[1] << 10) + (h[2] << 21) + (h[3] << 32);
}

#elif defined(_M_ARM_64)

static u64 GetHash64_ARMv8_CRC32(const u8* src, u32 len, u32 samples)
{
  u64 h[4] = {len, 0, 0, 0};
  u32 Step = (len / 8);
  const u64* data = (const u64*)src;
  const u64* end = data + Step;
  if (samples == 0)
    samples = std::max(Step, 1u);
  Step = Step / samples;
  if (Step < 1)
    Step = 1;

  while (data < end - Step * 3)
  {
    h[0] = __crc32d(h[0], data[Step * 0]);
    h[1] = __crc32d(h[1], data[Step * 1]);
    h[2] = __crc32d(h[2], data[Step * 2]);
    h[3] = __crc32d(h[3], data[Step * 3]);
    data += Step * 4;
  }
  if (data < end - Step * 0)
    h[0] = __crc32d(h[0], data[Step * 0]);
  if (data < end - Step * 1)
    h[1] = __crc32d(h[1], data[Step * 1]);
  if (data < end - Step * 2)
    h[2] = __crc32d(h[2], data[Step * 2]);

  if (len & 7)
  {
    u64 temp = 0;
    memcpy(&temp, end, len & 7);
    h[0] = __crc32d(h[0], temp);
  }

  // FIXME: is there a better way to combine these partial hashes?
  return h[0] + (h[1] << 10) + (h[2] << 21) + (h[3] << 32);
}

#endif

using TextureHashFunction = u64 (*)(const u8* src, u32 len, u32 samples);
static u64 SetHash64Function(const u8* src, u32 len, u32 samples);
static TextureHashFunction s_texture_hash_func = SetHash64Function;

static u64 SetHash64Function(const u8* src, u32 len, u32 samples)
{
  if (cpu_info.bCRC32)
  {
#if defined(_M_X86_64)
    s_texture_hash_func = &GetHash64_SSE42_CRC32;
#elif defined(_M_ARM_64)
    s_texture_hash_func = &GetHash64_ARMv8_CRC32;
#endif
  }
  else
  {
    s_texture_hash_func = &GetMurmurHash3;
  }
  return s_texture_hash_func(src, len, samples);
}

u64 GetHash64(const u8* src, u32 len, u32 samples)
{
  return s_texture_hash_func(src, len, samples);
}

u32 StartCRC32()
{
  return crc32_z(0L, Z_NULL, 0);
}

u32 UpdateCRC32(u32 crc, const u8* data, size_t len)
{
  return crc32_z(crc, data, len);
}

u32 ComputeCRC32(const u8* data, size_t len)
{
  return UpdateCRC32(StartCRC32(), data, len);
}

u32 ComputeCRC32(std::string_view data)
{
  return ComputeCRC32(reinterpret_cast<const u8*>(data.data()), data.size());
}

u8 HashCrc7(const u8* ptr, size_t length)
{
  // Used for SD cards
  constexpr u8 CRC_POLYNOMIAL = 0x09;

  u8 result = 0;
  for (size_t i = 0; i < length; i++)
  {
    // TODO: Cache in a table
    result ^= ptr[i];
    for (auto bit = 0; bit < 8; bit++)
    {
      if (result & 0x80)
        result = (result << 1) ^ (CRC_POLYNOMIAL << 1);
      else
        result = result << 1;
    }
  }

  return result >> 1;
}

u16 HashCrc16(const u8* ptr, size_t length)
{
  // Specifically CRC-16-CCITT, used for SD cards
  constexpr u16 CRC_POLYNOMIAL = 0x1021;

  u16 result = 0;
  for (size_t i = 0; i < length; i++)
  {
    // TODO: Cache in a table
    result ^= (ptr[i] << 8);
    for (auto bit = 0; bit < 8; bit++)
    {
      if (result & 0x8000)
        result = (result << 1) ^ CRC_POLYNOMIAL;
      else
        result = result << 1;
    }
  }

  return result;
}
}  // namespace Common
