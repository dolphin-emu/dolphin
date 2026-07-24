// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/DirectIOFile.h"
#include "Common/Swap.h"

namespace DiscIO
{
class BlobReader;
}

namespace DiscIO::NKit
{
constexpr u64 HEADER_OFFSET = 0x10000;

enum class Flags : u16
{
  None = 0,
  Size = 1 << 0,
  Crc32 = 1 << 1,
  Md5 = 1 << 2,
  Sha1 = 1 << 3,
  XxHash64 = 1 << 4,
  Key = 1 << 5,
  Encrypted = 1 << 6,
  ExtraData = 1 << 7,
  IndexFile = 1 << 8,
};

constexpr Flags operator|(Flags a, Flags b)
{
  return static_cast<Flags>(static_cast<u16>(a) | static_cast<u16>(b));
}

constexpr Flags operator&(Flags a, Flags b)
{
  return static_cast<Flags>(static_cast<u16>(a) & static_cast<u16>(b));
}

constexpr bool operator!(Flags a)
{
  return static_cast<u16>(a) == 0;
}

#pragma pack(1)
struct Header
{
  char magic[8];
  Common::BigEndianValue<u16> header_size;
  Common::BigEndianValue<u16> flags;
};
static_assert(sizeof(Header) == 12);

struct SizeField
{
  Common::BigEndianValue<u64> disc_size;
};
static_assert(sizeof(SizeField) == 8);

struct Crc32Field
{
  Common::BigEndianValue<u32> crc32;
};
static_assert(sizeof(Crc32Field) == 4);

struct Md5Field
{
  std::array<u8, 16> digest;
};
static_assert(sizeof(Md5Field) == 16);

struct Sha1Field
{
  std::array<u8, 20> digest;
};
static_assert(sizeof(Sha1Field) == 20);

struct XxHash64Field
{
  Common::BigEndianValue<u64> hash;
};
static_assert(sizeof(XxHash64Field) == 8);

struct KeyField
{
  u8 key_length;
  u8 key_type;
};
static_assert(sizeof(KeyField) == 2);
#pragma pack()

struct Digests
{
  u64 disc_size = 0;
  u32 crc32 = 0;
  std::array<u8, 16> md5{};
  std::array<u8, 20> sha1{};
  u64 xxhash64 = 0;
};

struct Info
{
  Digests digests;
  Flags flags = Flags::None;
  std::vector<u8> gap_type;
  bool valid = false;
};

struct VerifyResult
{
  bool size_match = false;
  bool crc32_match = false;
  bool md5_match = false;
  bool sha1_match = false;
  bool xxhash64_match = false;

  bool AllMatch() const
  {
    return size_match && crc32_match && md5_match && sha1_match && xxhash64_match;
  }
};

Info ReadHeader(BlobReader* reader, u64 offset, u64 gap_type_blocks);
Info ReadHeaderFromFile(File::DirectIOFile& file, u64 offset, u64 gap_type_blocks);
bool WriteHeader(File::DirectIOFile& file, u64 offset, const Info& info);
Digests ComputeDigests(BlobReader* reader, u64 disc_size);
VerifyResult Verify(const Info& stored, const Digests& computed);
bool IsGapJunk(const std::vector<u8>& gap_type, u64 block_index);

}  // namespace DiscIO::NKit
