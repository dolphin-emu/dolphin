// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// NKit v2 header format designed by Nanook.
// Implementation based on nod: https://github.com/encounter/nod/blob/d10d376/nod/src/io/nkit.rs
// See: https://github.com/dolphin-emu/dolphin/pull/14731

#include "DiscIO/NKitHeader.h"

#include <algorithm>
#include <cstring>

#include <mbedtls/md5.h>
#include <xxhash.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/Hash.h"
#include "DiscIO/Blob.h"

namespace DiscIO::NKit
{
static constexpr char NKIT_MAGIC[8] = {'N', 'K', 'I', 'T', ' ', ' ', 'v', '2'};

static bool ParseHeaderFields(const Header& header, const u8* data, size_t remaining,
                              const u8* gap_data, u64 gap_type_blocks, Info& info)
{
  const Flags flags = static_cast<Flags>(static_cast<u16>(header.flags));
  size_t pos = 0;

  if (!!(flags & Flags::Size))
  {
    if (pos + sizeof(SizeField) > remaining)
      return false;
    SizeField field;
    std::memcpy(&field, data + pos, sizeof(field));
    info.digests.disc_size = field.disc_size;
    pos += sizeof(SizeField);
  }

  if (!!(flags & Flags::Crc32))
  {
    if (pos + sizeof(Crc32Field) > remaining)
      return false;
    Crc32Field field;
    std::memcpy(&field, data + pos, sizeof(field));
    info.digests.crc32 = field.crc32;
    pos += sizeof(Crc32Field);
  }

  if (!!(flags & Flags::Md5))
  {
    if (pos + sizeof(Md5Field) > remaining)
      return false;
    Md5Field field;
    std::memcpy(&field, data + pos, sizeof(field));
    info.digests.md5 = field.digest;
    pos += sizeof(Md5Field);
  }

  if (!!(flags & Flags::Sha1))
  {
    if (pos + sizeof(Sha1Field) > remaining)
      return false;
    Sha1Field field;
    std::memcpy(&field, data + pos, sizeof(field));
    info.digests.sha1 = field.digest;
    pos += sizeof(Sha1Field);
  }

  if (!!(flags & Flags::XxHash64))
  {
    if (pos + sizeof(XxHash64Field) > remaining)
      return false;
    XxHash64Field field;
    std::memcpy(&field, data + pos, sizeof(field));
    info.digests.xxhash64 = field.hash;
    pos += sizeof(XxHash64Field);
  }

  if (!!(flags & Flags::Key))
  {
    if (pos + sizeof(KeyField) > remaining)
      return false;
    KeyField key_header;
    std::memcpy(&key_header, data + pos, sizeof(key_header));
    pos += sizeof(KeyField) + key_header.key_length;
  }

  // Encrypted, ExtraData, IndexFile are flag-only (no data)

  const size_t gap_type_bytes = static_cast<size_t>((gap_type_blocks + 7) / 8);
  info.gap_type.assign(gap_data, gap_data + gap_type_bytes);
  info.flags = flags;
  info.valid = true;
  return true;
}

Info ReadHeader(BlobReader* reader, u64 offset, u64 gap_type_blocks)
{
  Info info;

  Header header;
  if (!reader->Read(offset, sizeof(header), reinterpret_cast<u8*>(&header)))
    return info;

  if (std::memcmp(header.magic, NKIT_MAGIC, sizeof(NKIT_MAGIC)) != 0)
    return info;

  const u16 header_size = header.header_size;
  if (header_size < sizeof(Header))
    return info;

  const size_t remaining = header_size - sizeof(Header);
  const size_t gap_type_bytes = static_cast<size_t>((gap_type_blocks + 7) / 8);
  std::vector<u8> data(remaining + gap_type_bytes);
  if (!reader->Read(offset + sizeof(Header), data.size(), data.data()))
    return info;

  ParseHeaderFields(header, data.data(), remaining, data.data() + remaining, gap_type_blocks, info);
  return info;
}

Info ReadHeaderFromFile(File::DirectIOFile& file, u64 offset, u64 gap_type_blocks)
{
  Info info;

  Header header;
  file.Seek(offset, File::SeekOrigin::Begin);
  if (!file.Read(Common::AsWritableU8Span(header)))
    return info;

  if (std::memcmp(header.magic, NKIT_MAGIC, sizeof(NKIT_MAGIC)) != 0)
    return info;

  const u16 header_size = header.header_size;
  if (header_size < sizeof(Header))
    return info;

  const size_t remaining = header_size - sizeof(Header);
  const size_t gap_type_bytes = static_cast<size_t>((gap_type_blocks + 7) / 8);
  std::vector<u8> data(remaining + gap_type_bytes);
  if (!file.Read(data.data(), data.size()))
    return info;

  ParseHeaderFields(header, data.data(), remaining, data.data() + remaining, gap_type_blocks, info);
  return info;
}

bool WriteHeader(File::DirectIOFile& file, u64 offset, const Info& info)
{
  const u64 gap_type_bytes = info.gap_type.size();

  size_t data_size = 0;
  if (!!(info.flags & Flags::Size))
    data_size += sizeof(SizeField);
  if (!!(info.flags & Flags::Crc32))
    data_size += sizeof(Crc32Field);
  if (!!(info.flags & Flags::Md5))
    data_size += sizeof(Md5Field);
  if (!!(info.flags & Flags::Sha1))
    data_size += sizeof(Sha1Field);
  if (!!(info.flags & Flags::XxHash64))
    data_size += sizeof(XxHash64Field);
  const u16 total_header_size = static_cast<u16>(sizeof(Header) + data_size);

  std::vector<u8> buffer(total_header_size + gap_type_bytes, 0);

  Header header;
  std::memcpy(header.magic, NKIT_MAGIC, sizeof(NKIT_MAGIC));
  header.header_size = total_header_size;
  header.flags = static_cast<u16>(info.flags);
  std::memcpy(buffer.data(), &header, sizeof(Header));

  size_t pos = sizeof(Header);

  if (!!(info.flags & Flags::Size))
  {
    SizeField field;
    field.disc_size = info.digests.disc_size;
    std::memcpy(buffer.data() + pos, &field, sizeof(field));
    pos += sizeof(SizeField);
  }

  if (!!(info.flags & Flags::Crc32))
  {
    Crc32Field field;
    field.crc32 = info.digests.crc32;
    std::memcpy(buffer.data() + pos, &field, sizeof(field));
    pos += sizeof(Crc32Field);
  }

  if (!!(info.flags & Flags::Md5))
  {
    Md5Field field;
    field.digest = info.digests.md5;
    std::memcpy(buffer.data() + pos, &field, sizeof(field));
    pos += sizeof(Md5Field);
  }

  if (!!(info.flags & Flags::Sha1))
  {
    Sha1Field field;
    field.digest = info.digests.sha1;
    std::memcpy(buffer.data() + pos, &field, sizeof(field));
    pos += sizeof(Sha1Field);
  }

  if (!!(info.flags & Flags::XxHash64))
  {
    XxHash64Field field;
    field.hash = info.digests.xxhash64;
    std::memcpy(buffer.data() + pos, &field, sizeof(field));
    pos += sizeof(XxHash64Field);
  }

  std::memcpy(buffer.data() + pos, info.gap_type.data(), gap_type_bytes);

  file.Seek(offset, File::SeekOrigin::Begin);
  return file.Write(buffer.data(), buffer.size());
}

Digests ComputeDigests(BlobReader* reader, u64 disc_size)
{
  Digests digests;
  digests.disc_size = disc_size;

  auto sha1_ctx = Common::SHA1::CreateContext();
  u32 crc = Common::StartCRC32();
  mbedtls_md5_context md5_ctx{};
  mbedtls_md5_init(&md5_ctx);
  mbedtls_md5_starts_ret(&md5_ctx);
  XXH64_state_t* xxh_state = XXH64_createState();
  XXH64_reset(xxh_state, 0);

  constexpr size_t CHUNK_SIZE = 1 << 21;  // 2 MiB
  std::vector<u8> buffer(CHUNK_SIZE);

  for (u64 offset = 0; offset < disc_size; offset += CHUNK_SIZE)
  {
    const size_t sz = static_cast<size_t>(std::min<u64>(CHUNK_SIZE, disc_size - offset));
    if (!reader->Read(offset, sz, buffer.data()))
      break;

    sha1_ctx->Update(buffer.data(), sz);
    crc = Common::UpdateCRC32(crc, buffer.data(), sz);
    mbedtls_md5_update_ret(&md5_ctx, buffer.data(), sz);
    XXH64_update(xxh_state, buffer.data(), sz);
  }

  digests.sha1 = sha1_ctx->Finish();
  digests.crc32 = crc;
  mbedtls_md5_finish_ret(&md5_ctx, digests.md5.data());
  mbedtls_md5_free(&md5_ctx);
  digests.xxhash64 = XXH64_digest(xxh_state);
  XXH64_freeState(xxh_state);

  return digests;
}

VerifyResult Verify(const Info& stored, const Digests& computed)
{
  VerifyResult result;

  result.size_match =
      !(stored.flags & Flags::Size) || stored.digests.disc_size == computed.disc_size;
  result.crc32_match = !(stored.flags & Flags::Crc32) || stored.digests.crc32 == computed.crc32;
  result.md5_match = !(stored.flags & Flags::Md5) || stored.digests.md5 == computed.md5;
  result.sha1_match = !(stored.flags & Flags::Sha1) || stored.digests.sha1 == computed.sha1;
  result.xxhash64_match =
      !(stored.flags & Flags::XxHash64) || stored.digests.xxhash64 == computed.xxhash64;

  return result;
}

bool IsGapJunk(const std::vector<u8>& gap_type, u64 block_index)
{
  const size_t byte_index = static_cast<size_t>(block_index / 8);
  if (byte_index >= gap_type.size())
    return false;
  return (gap_type[byte_index] & (1 << (7 - (block_index & 7)))) != 0;
}

}  // namespace DiscIO::NKit
