// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/WIABlob.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <bzlib.h>
#include <lzma.h>
#include <mbedtls/sha1.h>
#include <zstd.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/MultithreadedCompressor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
std::pair<int, int> GetAllowedCompressionLevels(WIACompressionType compression_type)
{
  switch (compression_type)
  {
  case WIACompressionType::Bzip2:
  case WIACompressionType::LZMA:
  case WIACompressionType::LZMA2:
    return {1, 9};
  case WIACompressionType::Zstd:
    // The actual minimum level can be gotten by calling ZSTD_minCLevel(). However, returning that
    // would make the UI rather weird, because it is a negative number with very large magnitude.
    // Note: Level 0 is a special number which means "default level" (level 3 as of this writing).
    return {1, ZSTD_maxCLevel()};
  default:
    return {0, -1};
  }
}

WIAFileReader::WIAFileReader(File::IOFile file, const std::string& path)
    : m_file(std::move(file)), m_encryption_cache(this)
{
  m_valid = Initialize(path);
}

WIAFileReader::~WIAFileReader() = default;

bool WIAFileReader::Initialize(const std::string& path)
{
  if (!m_file.Seek(0, SEEK_SET) || !m_file.ReadArray(&m_header_1, 1))
    return false;

  if (m_header_1.magic != WIA_MAGIC && m_header_1.magic != RVZ_MAGIC)
    return false;

  m_rvz = m_header_1.magic == RVZ_MAGIC;

  const u32 version = m_rvz ? RVZ_VERSION : WIA_VERSION;
  const u32 version_read_compatible =
      m_rvz ? RVZ_VERSION_READ_COMPATIBLE : WIA_VERSION_READ_COMPATIBLE;

  const u32 file_version = Common::swap32(m_header_1.version);
  const u32 file_version_compatible = Common::swap32(m_header_1.version_compatible);

  if (version < file_version_compatible || version_read_compatible > file_version)
  {
    ERROR_LOG(DISCIO, "Unsupported version %s in %s", VersionToString(file_version).c_str(),
              path.c_str());
    return false;
  }

  SHA1 header_1_actual_hash;
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(&m_header_1), sizeof(m_header_1) - sizeof(SHA1),
                   header_1_actual_hash.data());
  if (m_header_1.header_1_hash != header_1_actual_hash)
    return false;

  if (Common::swap64(m_header_1.wia_file_size) != m_file.GetSize())
  {
    ERROR_LOG(DISCIO, "File size is incorrect for %s", path.c_str());
    return false;
  }

  const u32 header_2_size = Common::swap32(m_header_1.header_2_size);
  const u32 header_2_min_size = sizeof(WIAHeader2) - sizeof(WIAHeader2::compressor_data);
  if (header_2_size < header_2_min_size)
    return false;

  std::vector<u8> header_2(header_2_size);
  if (!m_file.ReadBytes(header_2.data(), header_2.size()))
    return false;

  SHA1 header_2_actual_hash;
  mbedtls_sha1_ret(header_2.data(), header_2.size(), header_2_actual_hash.data());
  if (m_header_1.header_2_hash != header_2_actual_hash)
    return false;

  std::memcpy(&m_header_2, header_2.data(), std::min(header_2.size(), sizeof(WIAHeader2)));

  if (m_header_2.compressor_data_size > sizeof(WIAHeader2::compressor_data) ||
      header_2_size < header_2_min_size + m_header_2.compressor_data_size)
  {
    return false;
  }

  const u32 chunk_size = Common::swap32(m_header_2.chunk_size);
  const auto is_power_of_two = [](u32 x) { return (x & (x - 1)) == 0; };
  if ((!m_rvz || chunk_size < VolumeWii::BLOCK_TOTAL_SIZE || !is_power_of_two(chunk_size)) &&
      chunk_size % VolumeWii::GROUP_TOTAL_SIZE != 0)
  {
    return false;
  }

  const u32 compression_type = Common::swap32(m_header_2.compression_type);
  m_compression_type = static_cast<WIACompressionType>(compression_type);
  if (m_compression_type > (m_rvz ? WIACompressionType::Zstd : WIACompressionType::LZMA2) ||
      (m_rvz && m_compression_type == WIACompressionType::Purge))
  {
    ERROR_LOG(DISCIO, "Unsupported compression type %u in %s", compression_type, path.c_str());
    return false;
  }

  const size_t number_of_partition_entries = Common::swap32(m_header_2.number_of_partition_entries);
  const size_t partition_entry_size = Common::swap32(m_header_2.partition_entry_size);
  std::vector<u8> partition_entries(partition_entry_size * number_of_partition_entries);
  if (!m_file.Seek(Common::swap64(m_header_2.partition_entries_offset), SEEK_SET))
    return false;
  if (!m_file.ReadBytes(partition_entries.data(), partition_entries.size()))
    return false;

  SHA1 partition_entries_actual_hash;
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(partition_entries.data()), partition_entries.size(),
                   partition_entries_actual_hash.data());
  if (m_header_2.partition_entries_hash != partition_entries_actual_hash)
    return false;

  const size_t copy_length = std::min(partition_entry_size, sizeof(PartitionEntry));
  const size_t memset_length = sizeof(PartitionEntry) - copy_length;
  u8* ptr = partition_entries.data();
  m_partition_entries.resize(number_of_partition_entries);
  for (size_t i = 0; i < number_of_partition_entries; ++i, ptr += partition_entry_size)
  {
    std::memcpy(&m_partition_entries[i], ptr, copy_length);
    std::memset(reinterpret_cast<u8*>(&m_partition_entries[i]) + copy_length, 0, memset_length);
  }

  for (size_t i = 0; i < m_partition_entries.size(); ++i)
  {
    const std::array<PartitionDataEntry, 2>& entries = m_partition_entries[i].data_entries;

    size_t non_empty_entries = 0;
    for (size_t j = 0; j < entries.size(); ++j)
    {
      const u32 number_of_sectors = Common::swap32(entries[j].number_of_sectors);
      if (number_of_sectors != 0)
      {
        ++non_empty_entries;

        const u32 last_sector = Common::swap32(entries[j].first_sector) + number_of_sectors;
        m_data_entries.emplace(last_sector * VolumeWii::BLOCK_TOTAL_SIZE, DataEntry(i, j));
      }
    }

    if (non_empty_entries > 1)
    {
      if (Common::swap32(entries[0].first_sector) > Common::swap32(entries[1].first_sector))
        return false;
    }
  }

  const u32 number_of_raw_data_entries = Common::swap32(m_header_2.number_of_raw_data_entries);
  m_raw_data_entries.resize(number_of_raw_data_entries);
  Chunk& raw_data_entries =
      ReadCompressedData(Common::swap64(m_header_2.raw_data_entries_offset),
                         Common::swap32(m_header_2.raw_data_entries_size),
                         number_of_raw_data_entries * sizeof(RawDataEntry), false);
  if (!raw_data_entries.ReadAll(&m_raw_data_entries))
    return false;

  for (size_t i = 0; i < m_raw_data_entries.size(); ++i)
  {
    const RawDataEntry& entry = m_raw_data_entries[i];
    const u64 data_size = Common::swap64(entry.data_size);
    if (data_size != 0)
      m_data_entries.emplace(Common::swap64(entry.data_offset) + data_size, DataEntry(i));
  }

  const u32 number_of_group_entries = Common::swap32(m_header_2.number_of_group_entries);
  m_group_entries.resize(number_of_group_entries);
  Chunk& group_entries = ReadCompressedData(Common::swap64(m_header_2.group_entries_offset),
                                            Common::swap32(m_header_2.group_entries_size),
                                            number_of_group_entries * sizeof(GroupEntry), false);
  if (!group_entries.ReadAll(&m_group_entries))
    return false;

  if (HasDataOverlap())
    return false;

  return true;
}

bool WIAFileReader::HasDataOverlap() const
{
  for (size_t i = 0; i < m_partition_entries.size(); ++i)
  {
    const std::array<PartitionDataEntry, 2>& entries = m_partition_entries[i].data_entries;
    for (size_t j = 0; j < entries.size(); ++j)
    {
      if (Common::swap32(entries[j].number_of_sectors) == 0)
        continue;

      const u64 data_offset = Common::swap32(entries[j].first_sector) * VolumeWii::BLOCK_TOTAL_SIZE;
      const auto it = m_data_entries.upper_bound(data_offset);
      if (it == m_data_entries.end())
        return true;  // Not an overlap, but an error nonetheless
      if (!it->second.is_partition || it->second.index != i || it->second.partition_data_index != j)
        return true;  // Overlap
    }
  }

  for (size_t i = 0; i < m_raw_data_entries.size(); ++i)
  {
    if (Common::swap64(m_raw_data_entries[i].data_size) == 0)
      continue;

    const u64 data_offset = Common::swap64(m_raw_data_entries[i].data_offset);
    const auto it = m_data_entries.upper_bound(data_offset);
    if (it == m_data_entries.end())
      return true;  // Not an overlap, but an error nonetheless
    if (it->second.is_partition || it->second.index != i)
      return true;  // Overlap
  }

  return false;
}

std::unique_ptr<WIAFileReader> WIAFileReader::Create(File::IOFile file, const std::string& path)
{
  std::unique_ptr<WIAFileReader> blob(new WIAFileReader(std::move(file), path));
  return blob->m_valid ? std::move(blob) : nullptr;
}

BlobType WIAFileReader::GetBlobType() const
{
  return m_rvz ? BlobType::RVZ : BlobType::WIA;
}

bool WIAFileReader::Read(u64 offset, u64 size, u8* out_ptr)
{
  if (offset + size > Common::swap64(m_header_1.iso_file_size))
    return false;

  if (offset < sizeof(WIAHeader2::disc_header))
  {
    const u64 bytes_to_read = std::min(sizeof(WIAHeader2::disc_header) - offset, size);
    std::memcpy(out_ptr, m_header_2.disc_header.data() + offset, bytes_to_read);
    offset += bytes_to_read;
    size -= bytes_to_read;
    out_ptr += bytes_to_read;
  }

  const u32 chunk_size = Common::swap32(m_header_2.chunk_size);
  while (size > 0)
  {
    const auto it = m_data_entries.upper_bound(offset);
    if (it == m_data_entries.end())
      return false;

    const DataEntry& data = it->second;
    if (data.is_partition)
    {
      const PartitionEntry& partition = m_partition_entries[it->second.index];

      const u32 partition_first_sector = Common::swap32(partition.data_entries[0].first_sector);
      const u64 partition_data_offset = partition_first_sector * VolumeWii::BLOCK_TOTAL_SIZE;

      const u32 second_number_of_sectors =
          Common::swap32(partition.data_entries[1].number_of_sectors);
      const u32 partition_total_sectors =
          second_number_of_sectors ? Common::swap32(partition.data_entries[1].first_sector) -
                                         partition_first_sector + second_number_of_sectors :
                                     Common::swap32(partition.data_entries[0].number_of_sectors);

      for (const PartitionDataEntry& partition_data : partition.data_entries)
      {
        if (size == 0)
          return true;

        const u32 first_sector = Common::swap32(partition_data.first_sector);
        const u32 number_of_sectors = Common::swap32(partition_data.number_of_sectors);

        const u64 data_offset = first_sector * VolumeWii::BLOCK_TOTAL_SIZE;
        const u64 data_size = number_of_sectors * VolumeWii::BLOCK_TOTAL_SIZE;

        if (data_size == 0)
          continue;

        if (data_offset + data_size <= offset)
          continue;

        if (offset < data_offset)
          return false;

        const u64 bytes_to_read = std::min(data_size - (offset - data_offset), size);

        m_exception_list.clear();
        m_write_to_exception_list = true;
        m_exception_list_last_group_index = std::numeric_limits<u64>::max();
        Common::ScopeGuard guard([this] { m_write_to_exception_list = false; });

        bool hash_exception_error = false;
        if (!m_encryption_cache.EncryptGroups(
                offset - partition_data_offset, bytes_to_read, out_ptr, partition_data_offset,
                partition_total_sectors * VolumeWii::BLOCK_DATA_SIZE, partition.partition_key,
                [this, &hash_exception_error](
                    VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP], u64 offset) {
                  // EncryptGroups calls ReadWiiDecrypted, which calls ReadFromGroups,
                  // which populates m_exception_list when m_write_to_exception_list == true
                  if (!ApplyHashExceptions(m_exception_list, hash_blocks))
                    hash_exception_error = true;
                }))
        {
          return false;
        }
        if (hash_exception_error)
          return false;

        offset += bytes_to_read;
        size -= bytes_to_read;
        out_ptr += bytes_to_read;
      }
    }
    else
    {
      const RawDataEntry& raw_data = m_raw_data_entries[data.index];
      if (!ReadFromGroups(&offset, &size, &out_ptr, chunk_size, VolumeWii::BLOCK_TOTAL_SIZE,
                          Common::swap64(raw_data.data_offset), Common::swap64(raw_data.data_size),
                          Common::swap32(raw_data.group_index),
                          Common::swap32(raw_data.number_of_groups), 0))
      {
        return false;
      }
    }
  }

  return true;
}

bool WIAFileReader::SupportsReadWiiDecrypted() const
{
  return !m_partition_entries.empty();
}

bool WIAFileReader::ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset)
{
  const u64 chunk_size = Common::swap32(m_header_2.chunk_size) * VolumeWii::BLOCK_DATA_SIZE /
                         VolumeWii::BLOCK_TOTAL_SIZE;

  const auto it = m_data_entries.upper_bound(partition_data_offset);
  if (it == m_data_entries.end() || !it->second.is_partition)
    return false;

  const PartitionEntry& partition = m_partition_entries[it->second.index];
  const u32 partition_first_sector = Common::swap32(partition.data_entries[0].first_sector);
  if (partition_data_offset != partition_first_sector * VolumeWii::BLOCK_TOTAL_SIZE)
    return false;

  for (const PartitionDataEntry& data : partition.data_entries)
  {
    if (size == 0)
      return true;

    const u64 data_offset =
        (Common::swap32(data.first_sector) - partition_first_sector) * VolumeWii::BLOCK_DATA_SIZE;
    const u64 data_size = Common::swap32(data.number_of_sectors) * VolumeWii::BLOCK_DATA_SIZE;

    if (!ReadFromGroups(&offset, &size, &out_ptr, chunk_size, VolumeWii::BLOCK_DATA_SIZE,
                        data_offset, data_size, Common::swap32(data.group_index),
                        Common::swap32(data.number_of_groups),
                        std::max<u64>(1, chunk_size / VolumeWii::GROUP_DATA_SIZE)))
    {
      return false;
    }
  }

  return size == 0;
}

bool WIAFileReader::ReadFromGroups(u64* offset, u64* size, u8** out_ptr, u64 chunk_size,
                                   u32 sector_size, u64 data_offset, u64 data_size, u32 group_index,
                                   u32 number_of_groups, u32 exception_lists)
{
  if (data_offset + data_size <= *offset)
    return true;

  if (*offset < data_offset)
    return false;

  const u64 skipped_data = data_offset % sector_size;
  data_offset -= skipped_data;
  data_size += skipped_data;

  const u64 start_group_index = (*offset - data_offset) / chunk_size;
  for (u64 i = start_group_index; i < number_of_groups && (*size) > 0; ++i)
  {
    const u64 total_group_index = group_index + i;
    if (total_group_index >= m_group_entries.size())
      return false;

    const GroupEntry group = m_group_entries[total_group_index];
    const u64 group_offset_in_data = i * chunk_size;
    const u64 offset_in_group = *offset - group_offset_in_data - data_offset;

    chunk_size = std::min(chunk_size, data_size - group_offset_in_data);

    const u64 bytes_to_read = std::min(chunk_size - offset_in_group, *size);
    const u32 group_data_size = Common::swap32(group.data_size);
    if (group_data_size == 0)
    {
      std::memset(*out_ptr, 0, bytes_to_read);
    }
    else
    {
      const u64 group_offset_in_file = static_cast<u64>(Common::swap32(group.data_offset)) << 2;
      Chunk& chunk =
          ReadCompressedData(group_offset_in_file, group_data_size, chunk_size, exception_lists);
      if (!chunk.Read(offset_in_group, bytes_to_read, *out_ptr))
      {
        m_cached_chunk_offset = std::numeric_limits<u64>::max();  // Invalidate the cache
        return false;
      }

      if (m_write_to_exception_list && m_exception_list_last_group_index != total_group_index)
      {
        const u64 exception_list_index = offset_in_group / VolumeWii::GROUP_DATA_SIZE;
        const u16 additional_offset =
            static_cast<u16>(group_offset_in_data % VolumeWii::GROUP_DATA_SIZE /
                             VolumeWii::BLOCK_DATA_SIZE * VolumeWii::BLOCK_HEADER_SIZE);
        chunk.GetHashExceptions(&m_exception_list, exception_list_index, additional_offset);
        m_exception_list_last_group_index = total_group_index;
      }
    }

    *offset += bytes_to_read;
    *size -= bytes_to_read;
    *out_ptr += bytes_to_read;
  }

  return true;
}

WIAFileReader::Chunk& WIAFileReader::ReadCompressedData(u64 offset_in_file, u64 compressed_size,
                                                        u64 decompressed_size, u32 exception_lists)
{
  if (offset_in_file == m_cached_chunk_offset)
    return m_cached_chunk;

  std::unique_ptr<Decompressor> decompressor;
  switch (m_compression_type)
  {
  case WIACompressionType::None:
    decompressor = std::make_unique<NoneDecompressor>();
    break;
  case WIACompressionType::Purge:
    decompressor = std::make_unique<PurgeDecompressor>(decompressed_size);
    break;
  case WIACompressionType::Bzip2:
    decompressor = std::make_unique<Bzip2Decompressor>();
    break;
  case WIACompressionType::LZMA:
    decompressor = std::make_unique<LZMADecompressor>(false, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  case WIACompressionType::LZMA2:
    decompressor = std::make_unique<LZMADecompressor>(true, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  case WIACompressionType::Zstd:
    decompressor = std::make_unique<ZstdDecompressor>();
    break;
  }

  const bool compressed_exception_lists = m_compression_type > WIACompressionType::Purge;

  m_cached_chunk = Chunk(&m_file, offset_in_file, compressed_size, decompressed_size,
                         exception_lists, compressed_exception_lists, std::move(decompressor));
  m_cached_chunk_offset = offset_in_file;
  return m_cached_chunk;
}

std::string WIAFileReader::VersionToString(u32 version)
{
  const u8 a = version >> 24;
  const u8 b = (version >> 16) & 0xff;
  const u8 c = (version >> 8) & 0xff;
  const u8 d = version & 0xff;

  if (d == 0 || d == 0xff)
    return StringFromFormat("%u.%02x.%02x", a, b, c);
  else
    return StringFromFormat("%u.%02x.%02x.beta%u", a, b, c, d);
}

u32 WIAFileReader::LZMA2DictionarySize(u8 p)
{
  return (static_cast<u32>(2) | (p & 1)) << (p / 2 + 11);
}

WIAFileReader::Decompressor::~Decompressor() = default;

bool WIAFileReader::NoneDecompressor::Decompress(const DecompressionBuffer& in,
                                                 DecompressionBuffer* out, size_t* in_bytes_read)
{
  const size_t length =
      std::min(in.bytes_written - *in_bytes_read, out->data.size() - out->bytes_written);

  std::memcpy(out->data.data() + out->bytes_written, in.data.data() + *in_bytes_read, length);

  *in_bytes_read += length;
  out->bytes_written += length;

  m_done = in.data.size() == *in_bytes_read;
  return true;
}

WIAFileReader::PurgeDecompressor::PurgeDecompressor(u64 decompressed_size)
    : m_decompressed_size(decompressed_size)
{
  mbedtls_sha1_init(&m_sha1_context);
}

bool WIAFileReader::PurgeDecompressor::Decompress(const DecompressionBuffer& in,
                                                  DecompressionBuffer* out, size_t* in_bytes_read)
{
  if (!m_started)
  {
    mbedtls_sha1_starts_ret(&m_sha1_context);

    // Include the exception lists in the SHA-1 calculation (but not in the compression...)
    mbedtls_sha1_update_ret(&m_sha1_context, in.data.data(), *in_bytes_read);

    m_started = true;
  }

  while (!m_done && in.bytes_written != *in_bytes_read &&
         (m_segment_bytes_written < sizeof(m_segment) || out->data.size() != out->bytes_written))
  {
    if (m_segment_bytes_written == 0 && *in_bytes_read == in.data.size() - sizeof(SHA1))
    {
      const size_t zeroes_to_write = std::min<size_t>(m_decompressed_size - m_out_bytes_written,
                                                      out->data.size() - out->bytes_written);

      std::memset(out->data.data() + out->bytes_written, 0, zeroes_to_write);

      out->bytes_written += zeroes_to_write;
      m_out_bytes_written += zeroes_to_write;

      if (m_out_bytes_written == m_decompressed_size && in.bytes_written == in.data.size())
      {
        SHA1 actual_hash;
        mbedtls_sha1_finish_ret(&m_sha1_context, actual_hash.data());

        SHA1 expected_hash;
        std::memcpy(expected_hash.data(), in.data.data() + *in_bytes_read, expected_hash.size());

        *in_bytes_read += expected_hash.size();
        m_done = true;

        if (actual_hash != expected_hash)
          return false;
      }

      return true;
    }

    if (m_segment_bytes_written < sizeof(m_segment))
    {
      const size_t bytes_to_copy =
          std::min(in.bytes_written - *in_bytes_read, sizeof(m_segment) - m_segment_bytes_written);

      std::memcpy(reinterpret_cast<u8*>(&m_segment) + m_segment_bytes_written,
                  in.data.data() + *in_bytes_read, bytes_to_copy);
      mbedtls_sha1_update_ret(&m_sha1_context, in.data.data() + *in_bytes_read, bytes_to_copy);

      *in_bytes_read += bytes_to_copy;
      m_bytes_read += bytes_to_copy;
      m_segment_bytes_written += bytes_to_copy;
    }

    if (m_segment_bytes_written < sizeof(m_segment))
      return true;

    const size_t offset = Common::swap32(m_segment.offset);
    const size_t size = Common::swap32(m_segment.size);

    if (m_out_bytes_written < offset)
    {
      const size_t zeroes_to_write =
          std::min(offset - m_out_bytes_written, out->data.size() - out->bytes_written);

      std::memset(out->data.data() + out->bytes_written, 0, zeroes_to_write);

      out->bytes_written += zeroes_to_write;
      m_out_bytes_written += zeroes_to_write;
    }

    if (m_out_bytes_written >= offset && m_out_bytes_written < offset + size)
    {
      const size_t bytes_to_copy = std::min(
          std::min(offset + size - m_out_bytes_written, out->data.size() - out->bytes_written),
          in.bytes_written - *in_bytes_read);

      std::memcpy(out->data.data() + out->bytes_written, in.data.data() + *in_bytes_read,
                  bytes_to_copy);
      mbedtls_sha1_update_ret(&m_sha1_context, in.data.data() + *in_bytes_read, bytes_to_copy);

      *in_bytes_read += bytes_to_copy;
      m_bytes_read += bytes_to_copy;
      out->bytes_written += bytes_to_copy;
      m_out_bytes_written += bytes_to_copy;
    }

    if (m_out_bytes_written >= offset + size)
      m_segment_bytes_written = 0;
  }

  return true;
}

WIAFileReader::Bzip2Decompressor::~Bzip2Decompressor()
{
  if (m_started)
    BZ2_bzDecompressEnd(&m_stream);
}

bool WIAFileReader::Bzip2Decompressor::Decompress(const DecompressionBuffer& in,
                                                  DecompressionBuffer* out, size_t* in_bytes_read)
{
  if (!m_started)
  {
    if (BZ2_bzDecompressInit(&m_stream, 0, 0) != BZ_OK)
      return false;

    m_started = true;
  }

  constexpr auto clamped_cast = [](size_t x) {
    return static_cast<unsigned int>(
        std::min<size_t>(std::numeric_limits<unsigned int>().max(), x));
  };

  char* const in_ptr = reinterpret_cast<char*>(const_cast<u8*>(in.data.data() + *in_bytes_read));
  m_stream.next_in = in_ptr;
  m_stream.avail_in = clamped_cast(in.bytes_written - *in_bytes_read);

  char* const out_ptr = reinterpret_cast<char*>(out->data.data() + out->bytes_written);
  m_stream.next_out = out_ptr;
  m_stream.avail_out = clamped_cast(out->data.size() - out->bytes_written);

  const int result = BZ2_bzDecompress(&m_stream);

  *in_bytes_read += m_stream.next_in - in_ptr;
  out->bytes_written += m_stream.next_out - out_ptr;

  m_done = result == BZ_STREAM_END;
  return result == BZ_OK || result == BZ_STREAM_END;
}

WIAFileReader::LZMADecompressor::LZMADecompressor(bool lzma2, const u8* filter_options,
                                                  size_t filter_options_size)
{
  m_options.preset_dict = nullptr;

  if (!lzma2 && filter_options_size == 5)
  {
    // The dictionary size is stored as a 32-bit little endian unsigned integer
    static_assert(sizeof(m_options.dict_size) == sizeof(u32));
    std::memcpy(&m_options.dict_size, filter_options + 1, sizeof(u32));

    const u8 d = filter_options[0];
    if (d >= (9 * 5 * 5))
    {
      m_error_occurred = true;
    }
    else
    {
      m_options.lc = d % 9;
      const u8 e = d / 9;
      m_options.pb = e / 5;
      m_options.lp = e % 5;
    }
  }
  else if (lzma2 && filter_options_size == 1)
  {
    const u8 d = filter_options[0];
    if (d > 40)
      m_error_occurred = true;
    else
      m_options.dict_size = d == 40 ? 0xFFFFFFFF : LZMA2DictionarySize(d);
  }
  else
  {
    m_error_occurred = true;
  }

  m_filters[0].id = lzma2 ? LZMA_FILTER_LZMA2 : LZMA_FILTER_LZMA1;
  m_filters[0].options = &m_options;
  m_filters[1].id = LZMA_VLI_UNKNOWN;
  m_filters[1].options = nullptr;
}

WIAFileReader::LZMADecompressor::~LZMADecompressor()
{
  if (m_started)
    lzma_end(&m_stream);
}

bool WIAFileReader::LZMADecompressor::Decompress(const DecompressionBuffer& in,
                                                 DecompressionBuffer* out, size_t* in_bytes_read)
{
  if (!m_started)
  {
    if (m_error_occurred || lzma_raw_decoder(&m_stream, m_filters) != LZMA_OK)
      return false;

    m_started = true;
  }

  const u8* const in_ptr = in.data.data() + *in_bytes_read;
  m_stream.next_in = in_ptr;
  m_stream.avail_in = in.bytes_written - *in_bytes_read;

  u8* const out_ptr = out->data.data() + out->bytes_written;
  m_stream.next_out = out_ptr;
  m_stream.avail_out = out->data.size() - out->bytes_written;

  const lzma_ret result = lzma_code(&m_stream, LZMA_RUN);

  *in_bytes_read += m_stream.next_in - in_ptr;
  out->bytes_written += m_stream.next_out - out_ptr;

  m_done = result == LZMA_STREAM_END;
  return result == LZMA_OK || result == LZMA_STREAM_END;
}

WIAFileReader::ZstdDecompressor::ZstdDecompressor()
{
  m_stream = ZSTD_createDStream();
}

WIAFileReader::ZstdDecompressor::~ZstdDecompressor()
{
  ZSTD_freeDStream(m_stream);
}

bool WIAFileReader::ZstdDecompressor::Decompress(const DecompressionBuffer& in,
                                                 DecompressionBuffer* out, size_t* in_bytes_read)
{
  if (!m_stream)
    return false;

  ZSTD_inBuffer in_buffer{in.data.data(), in.bytes_written, *in_bytes_read};
  ZSTD_outBuffer out_buffer{out->data.data(), out->data.size(), out->bytes_written};

  const size_t result = ZSTD_decompressStream(m_stream, &out_buffer, &in_buffer);

  *in_bytes_read = in_buffer.pos;
  out->bytes_written = out_buffer.pos;

  m_done = result == 0;
  return !ZSTD_isError(result);
}

WIAFileReader::Compressor::~Compressor() = default;

WIAFileReader::PurgeCompressor::PurgeCompressor()
{
  mbedtls_sha1_init(&m_sha1_context);
}

WIAFileReader::PurgeCompressor::~PurgeCompressor() = default;

bool WIAFileReader::PurgeCompressor::Start()
{
  m_buffer.clear();
  m_bytes_written = 0;

  mbedtls_sha1_starts_ret(&m_sha1_context);

  return true;
}

bool WIAFileReader::PurgeCompressor::AddPrecedingDataOnlyForPurgeHashing(const u8* data,
                                                                         size_t size)
{
  mbedtls_sha1_update_ret(&m_sha1_context, data, size);
  return true;
}

bool WIAFileReader::PurgeCompressor::Compress(const u8* data, size_t size)
{
  // We could add support for calling this twice if we're fine with
  // making the code more complicated, but there's no need to support it
  ASSERT_MSG(DISCIO, m_bytes_written == 0,
             "Calling PurgeCompressor::Compress() twice is not supported");

  m_buffer.resize(size + sizeof(PurgeSegment) + sizeof(SHA1));

  size_t bytes_read = 0;

  while (true)
  {
    const auto first_non_zero =
        std::find_if(data + bytes_read, data + size, [](u8 x) { return x != 0; });

    const u32 non_zero_data_start = static_cast<u32>(first_non_zero - data);
    if (non_zero_data_start == size)
      break;

    size_t non_zero_data_end = non_zero_data_start;
    size_t sequence_length = 0;
    for (size_t i = non_zero_data_start; i < size; ++i)
    {
      if (data[i] == 0)
      {
        ++sequence_length;
      }
      else
      {
        sequence_length = 0;
        non_zero_data_end = i + 1;
      }

      // To avoid wasting space, only count runs of zeroes that are of a certain length
      // (unless there is nothing after the run of zeroes, then we might as well always count it)
      if (sequence_length > sizeof(PurgeSegment))
        break;
    }

    const u32 non_zero_data_length = static_cast<u32>(non_zero_data_end - non_zero_data_start);

    const PurgeSegment segment{Common::swap32(non_zero_data_start),
                               Common::swap32(non_zero_data_length)};
    std::memcpy(m_buffer.data() + m_bytes_written, &segment, sizeof(segment));
    m_bytes_written += sizeof(segment);

    std::memcpy(m_buffer.data() + m_bytes_written, data + non_zero_data_start,
                non_zero_data_length);
    m_bytes_written += non_zero_data_length;

    bytes_read = non_zero_data_end;
  }

  return true;
}

bool WIAFileReader::PurgeCompressor::End()
{
  mbedtls_sha1_update_ret(&m_sha1_context, m_buffer.data(), m_bytes_written);

  mbedtls_sha1_finish_ret(&m_sha1_context, m_buffer.data() + m_bytes_written);
  m_bytes_written += sizeof(SHA1);

  ASSERT(m_bytes_written <= m_buffer.size());

  return true;
}

const u8* WIAFileReader::PurgeCompressor::GetData() const
{
  return m_buffer.data();
}

size_t WIAFileReader::PurgeCompressor::GetSize() const
{
  return m_bytes_written;
}

WIAFileReader::Bzip2Compressor::Bzip2Compressor(int compression_level)
    : m_compression_level(compression_level)
{
}

WIAFileReader::Bzip2Compressor::~Bzip2Compressor()
{
  BZ2_bzCompressEnd(&m_stream);
}

bool WIAFileReader::Bzip2Compressor::Start()
{
  ASSERT_MSG(DISCIO, m_stream.state == nullptr,
             "Called Bzip2Compressor::Start() twice without calling Bzip2Compressor::End()");

  m_buffer.clear();
  m_stream.next_out = reinterpret_cast<char*>(m_buffer.data());

  return BZ2_bzCompressInit(&m_stream, m_compression_level, 0, 0) == BZ_OK;
}

bool WIAFileReader::Bzip2Compressor::Compress(const u8* data, size_t size)
{
  m_stream.next_in = reinterpret_cast<char*>(const_cast<u8*>(data));
  m_stream.avail_in = static_cast<unsigned int>(size);

  ExpandBuffer(size);

  while (m_stream.avail_in != 0)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    if (BZ2_bzCompress(&m_stream, BZ_RUN) != BZ_RUN_OK)
      return false;
  }

  return true;
}

bool WIAFileReader::Bzip2Compressor::End()
{
  bool success = true;

  while (true)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    const int result = BZ2_bzCompress(&m_stream, BZ_FINISH);
    if (result != BZ_FINISH_OK && result != BZ_STREAM_END)
      success = false;
    if (result != BZ_FINISH_OK)
      break;
  }

  if (BZ2_bzCompressEnd(&m_stream) != BZ_OK)
    success = false;

  return success;
}

void WIAFileReader::Bzip2Compressor::ExpandBuffer(size_t bytes_to_add)
{
  const size_t bytes_written = GetSize();
  m_buffer.resize(m_buffer.size() + bytes_to_add);
  m_stream.next_out = reinterpret_cast<char*>(m_buffer.data()) + bytes_written;
  m_stream.avail_out = static_cast<unsigned int>(m_buffer.size() - bytes_written);
}

const u8* WIAFileReader::Bzip2Compressor::GetData() const
{
  return m_buffer.data();
}

size_t WIAFileReader::Bzip2Compressor::GetSize() const
{
  return static_cast<size_t>(reinterpret_cast<u8*>(m_stream.next_out) - m_buffer.data());
}

WIAFileReader::LZMACompressor::LZMACompressor(bool lzma2, int compression_level,
                                              u8 compressor_data_out[7],
                                              u8* compressor_data_size_out)
{
  // lzma_lzma_preset returns false on success for some reason
  if (lzma_lzma_preset(&m_options, static_cast<uint32_t>(compression_level)))
  {
    m_initialization_failed = true;
    return;
  }

  if (!lzma2)
  {
    if (compressor_data_size_out)
      *compressor_data_size_out = 5;

    if (compressor_data_out)
    {
      ASSERT(m_options.lc < 9);
      ASSERT(m_options.lp < 5);
      ASSERT(m_options.pb < 5);
      compressor_data_out[0] =
          static_cast<u8>((m_options.pb * 5 + m_options.lp) * 9 + m_options.lc);

      // The dictionary size is stored as a 32-bit little endian unsigned integer
      static_assert(sizeof(m_options.dict_size) == sizeof(u32));
      std::memcpy(compressor_data_out + 1, &m_options.dict_size, sizeof(u32));
    }
  }
  else
  {
    if (compressor_data_size_out)
      *compressor_data_size_out = 1;

    if (compressor_data_out)
    {
      u8 encoded_dict_size = 0;
      while (encoded_dict_size < 40 && m_options.dict_size > LZMA2DictionarySize(encoded_dict_size))
        ++encoded_dict_size;

      compressor_data_out[0] = encoded_dict_size;
    }
  }

  m_filters[0].id = lzma2 ? LZMA_FILTER_LZMA2 : LZMA_FILTER_LZMA1;
  m_filters[0].options = &m_options;
  m_filters[1].id = LZMA_VLI_UNKNOWN;
  m_filters[1].options = nullptr;
}

WIAFileReader::LZMACompressor::~LZMACompressor()
{
  lzma_end(&m_stream);
}

bool WIAFileReader::LZMACompressor::Start()
{
  if (m_initialization_failed)
    return false;

  m_buffer.clear();
  m_stream.next_out = m_buffer.data();

  return lzma_raw_encoder(&m_stream, m_filters) == LZMA_OK;
}

bool WIAFileReader::LZMACompressor::Compress(const u8* data, size_t size)
{
  m_stream.next_in = data;
  m_stream.avail_in = size;

  ExpandBuffer(size);

  while (m_stream.avail_in != 0)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    if (lzma_code(&m_stream, LZMA_RUN) != LZMA_OK)
      return false;
  }

  return true;
}

bool WIAFileReader::LZMACompressor::End()
{
  while (true)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    switch (lzma_code(&m_stream, LZMA_FINISH))
    {
    case LZMA_OK:
      break;
    case LZMA_STREAM_END:
      return true;
    default:
      return false;
    }
  }
}

void WIAFileReader::LZMACompressor::ExpandBuffer(size_t bytes_to_add)
{
  const size_t bytes_written = GetSize();
  m_buffer.resize(m_buffer.size() + bytes_to_add);
  m_stream.next_out = m_buffer.data() + bytes_written;
  m_stream.avail_out = m_buffer.size() - bytes_written;
}

const u8* WIAFileReader::LZMACompressor::GetData() const
{
  return m_buffer.data();
}

size_t WIAFileReader::LZMACompressor::GetSize() const
{
  return static_cast<size_t>(m_stream.next_out - m_buffer.data());
}

WIAFileReader::ZstdCompressor::ZstdCompressor(int compression_level)
{
  m_stream = ZSTD_createCStream();

  if (ZSTD_isError(ZSTD_CCtx_setParameter(m_stream, ZSTD_c_compressionLevel, compression_level)))
    m_stream = nullptr;
}

WIAFileReader::ZstdCompressor::~ZstdCompressor()
{
  ZSTD_freeCStream(m_stream);
}

bool WIAFileReader::ZstdCompressor::Start()
{
  if (!m_stream)
    return false;

  m_buffer.clear();
  m_out_buffer = {};

  return !ZSTD_isError(ZSTD_CCtx_reset(m_stream, ZSTD_reset_session_only));
}

bool WIAFileReader::ZstdCompressor::Compress(const u8* data, size_t size)
{
  ZSTD_inBuffer in_buffer{data, size, 0};

  ExpandBuffer(size);

  while (in_buffer.size != in_buffer.pos)
  {
    if (m_out_buffer.size == m_out_buffer.pos)
      ExpandBuffer(0x100);

    if (ZSTD_isError(ZSTD_compressStream(m_stream, &m_out_buffer, &in_buffer)))
      return false;
  }

  return true;
}

bool WIAFileReader::ZstdCompressor::End()
{
  while (true)
  {
    if (m_out_buffer.size == m_out_buffer.pos)
      ExpandBuffer(0x100);

    const size_t result = ZSTD_endStream(m_stream, &m_out_buffer);
    if (ZSTD_isError(result))
      return false;
    if (result == 0)
      return true;
  }
}

void WIAFileReader::ZstdCompressor::ExpandBuffer(size_t bytes_to_add)
{
  m_buffer.resize(m_buffer.size() + bytes_to_add);

  m_out_buffer.dst = m_buffer.data();
  m_out_buffer.size = m_buffer.size();
}

WIAFileReader::Chunk::Chunk() = default;

WIAFileReader::Chunk::Chunk(File::IOFile* file, u64 offset_in_file, u64 compressed_size,
                            u64 decompressed_size, u32 exception_lists,
                            bool compressed_exception_lists,
                            std::unique_ptr<Decompressor> decompressor)
    : m_file(file), m_offset_in_file(offset_in_file), m_exception_lists(exception_lists),
      m_compressed_exception_lists(compressed_exception_lists),
      m_decompressor(std::move(decompressor))
{
  constexpr size_t MAX_SIZE_PER_EXCEPTION_LIST =
      Common::AlignUp(VolumeWii::BLOCK_HEADER_SIZE, sizeof(SHA1)) / sizeof(SHA1) *
          VolumeWii::BLOCKS_PER_GROUP * sizeof(HashExceptionEntry) +
      sizeof(u16);

  m_out_bytes_allocated_for_exceptions =
      m_compressed_exception_lists ? MAX_SIZE_PER_EXCEPTION_LIST * m_exception_lists : 0;

  m_in.data.resize(compressed_size);
  m_out.data.resize(decompressed_size + m_out_bytes_allocated_for_exceptions);
}

bool WIAFileReader::Chunk::Read(u64 offset, u64 size, u8* out_ptr)
{
  if (!m_decompressor || !m_file ||
      offset + size > m_out.data.size() - m_out_bytes_allocated_for_exceptions)
  {
    return false;
  }

  while (offset + size > m_out.bytes_written - m_out_bytes_used_for_exceptions)
  {
    u64 bytes_to_read;
    if (offset + size == m_out.data.size())
    {
      // Read all the remaining data.
      bytes_to_read = m_in.data.size() - m_in.bytes_written;
    }
    else
    {
      // Pick a suitable amount of compressed data to read. The std::min line has to
      // be as it is, but the rest is a bit arbitrary and can be changed if desired.

      // The compressed data is probably not much bigger than the decompressed data.
      // Add a few bytes for possible compression overhead and for any hash exceptions.
      bytes_to_read =
          offset + size - (m_out.bytes_written - m_out_bytes_used_for_exceptions) + 0x100;

      // Align the access in an attempt to gain speed. But we don't actually know the
      // block size of the underlying storage device, so we just use the Wii block size.
      bytes_to_read =
          Common::AlignUp(bytes_to_read + m_offset_in_file, VolumeWii::BLOCK_TOTAL_SIZE) -
          m_offset_in_file;

      // Ensure we don't read too much.
      bytes_to_read = std::min<u64>(m_in.data.size() - m_in.bytes_written, bytes_to_read);
    }

    if (bytes_to_read == 0)
    {
      // Compressed size is larger than expected or decompressed size is smaller than expected
      return false;
    }

    if (!m_file->Seek(m_offset_in_file, SEEK_SET))
      return false;
    if (!m_file->ReadBytes(m_in.data.data() + m_in.bytes_written, bytes_to_read))
      return false;

    m_offset_in_file += bytes_to_read;
    m_in.bytes_written += bytes_to_read;

    if (m_exception_lists > 0 && !m_compressed_exception_lists)
    {
      if (!HandleExceptions(m_in.data.data(), m_in.data.size(), m_in.bytes_written,
                            &m_in_bytes_used_for_exceptions, true))
      {
        return false;
      }

      m_in_bytes_read = m_in_bytes_used_for_exceptions;
    }

    if (m_exception_lists == 0 || m_compressed_exception_lists)
    {
      if (!m_decompressor->Decompress(m_in, &m_out, &m_in_bytes_read))
        return false;
    }

    if (m_exception_lists > 0 && m_compressed_exception_lists)
    {
      if (!HandleExceptions(m_out.data.data(), m_out_bytes_allocated_for_exceptions,
                            m_out.bytes_written, &m_out_bytes_used_for_exceptions, false))
      {
        return false;
      }
    }

    if (m_exception_lists == 0)
    {
      const size_t expected_out_bytes = m_out.data.size() - m_out_bytes_allocated_for_exceptions +
                                        m_out_bytes_used_for_exceptions;

      if (m_out.bytes_written > expected_out_bytes)
        return false;  // Decompressed size is larger than expected

      // The reason why we need the m_in.bytes_written == m_in.data.size() check as part of
      // this conditional is because (for example) zstd can finish writing all data to m_out
      // before becoming done if we've given it all input data except the checksum at the end.
      if (m_out.bytes_written == expected_out_bytes && !m_decompressor->Done() &&
          m_in.bytes_written == m_in.data.size())
      {
        return false;  // Decompressed size is larger than expected
      }

      if (m_decompressor->Done() && m_in_bytes_read != m_in.data.size())
        return false;  // Compressed size is smaller than expected
    }
  }

  std::memcpy(out_ptr, m_out.data.data() + offset + m_out_bytes_used_for_exceptions, size);
  return true;
}

bool WIAFileReader::Chunk::HandleExceptions(const u8* data, size_t bytes_allocated,
                                            size_t bytes_written, size_t* bytes_used, bool align)
{
  while (m_exception_lists > 0)
  {
    if (sizeof(u16) + *bytes_used > bytes_allocated)
    {
      ERROR_LOG(DISCIO, "More hash exceptions than expected");
      return false;
    }
    if (sizeof(u16) + *bytes_used > bytes_written)
      return true;

    const u16 exceptions = Common::swap16(data + *bytes_used);

    size_t exception_list_size = exceptions * sizeof(HashExceptionEntry) + sizeof(u16);
    if (align && m_exception_lists == 1)
      exception_list_size = Common::AlignUp(*bytes_used + exception_list_size, 4) - *bytes_used;

    if (exception_list_size + *bytes_used > bytes_allocated)
    {
      ERROR_LOG(DISCIO, "More hash exceptions than expected");
      return false;
    }
    if (exception_list_size + *bytes_used > bytes_written)
      return true;

    *bytes_used += exception_list_size;
    --m_exception_lists;
  }

  return true;
}

void WIAFileReader::Chunk::GetHashExceptions(std::vector<HashExceptionEntry>* exception_list,
                                             u64 exception_list_index, u16 additional_offset) const
{
  ASSERT(m_exception_lists == 0);

  const u8* data_start = m_compressed_exception_lists ? m_out.data.data() : m_in.data.data();
  const u8* data = data_start;

  for (u64 i = exception_list_index; i > 0; --i)
    data += Common::swap16(data) * sizeof(HashExceptionEntry) + sizeof(u16);

  const u16 exceptions = Common::swap16(data);
  data += sizeof(u16);

  for (size_t i = 0; i < exceptions; ++i)
  {
    std::memcpy(&exception_list->emplace_back(), data, sizeof(HashExceptionEntry));
    data += sizeof(HashExceptionEntry);

    u16& offset = exception_list->back().offset;
    offset = Common::swap16(Common::swap16(offset) + additional_offset);
  }

  ASSERT(data <= data_start + (m_compressed_exception_lists ? m_out_bytes_used_for_exceptions :
                                                              m_in_bytes_used_for_exceptions));
}

bool WIAFileReader::ApplyHashExceptions(
    const std::vector<HashExceptionEntry>& exception_list,
    VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP])
{
  for (const HashExceptionEntry& exception : exception_list)
  {
    const u16 offset = Common::swap16(exception.offset);

    const size_t block_index = offset / VolumeWii::BLOCK_HEADER_SIZE;
    if (block_index > VolumeWii::BLOCKS_PER_GROUP)
      return false;

    const size_t offset_in_block = offset % VolumeWii::BLOCK_HEADER_SIZE;
    if (offset_in_block + sizeof(SHA1) > VolumeWii::BLOCK_HEADER_SIZE)
      return false;

    std::memcpy(reinterpret_cast<u8*>(&hash_blocks[block_index]) + offset_in_block, &exception.hash,
                sizeof(SHA1));
  }

  return true;
}

bool WIAFileReader::PadTo4(File::IOFile* file, u64* bytes_written)
{
  constexpr u32 ZEROES = 0;
  const u64 bytes_to_write = Common::AlignUp(*bytes_written, 4) - *bytes_written;
  if (bytes_to_write == 0)
    return true;

  *bytes_written += bytes_to_write;
  return file->WriteBytes(&ZEROES, bytes_to_write);
}

void WIAFileReader::AddRawDataEntry(u64 offset, u64 size, int chunk_size, u32* total_groups,
                                    std::vector<RawDataEntry>* raw_data_entries,
                                    std::vector<DataEntry>* data_entries)
{
  constexpr size_t SKIP_SIZE = sizeof(WIAHeader2::disc_header);
  const u64 skip = offset < SKIP_SIZE ? std::min(SKIP_SIZE - offset, size) : 0;

  offset += skip;
  size -= skip;

  if (size == 0)
    return;

  const u32 group_index = *total_groups;
  const u32 groups = static_cast<u32>(Common::AlignUp(size, chunk_size) / chunk_size);
  *total_groups += groups;

  data_entries->emplace_back(raw_data_entries->size());
  raw_data_entries->emplace_back(RawDataEntry{Common::swap64(offset), Common::swap64(size),
                                              Common::swap32(group_index), Common::swap32(groups)});
}

WIAFileReader::PartitionDataEntry WIAFileReader::CreatePartitionDataEntry(
    u64 offset, u64 size, u32 index, int chunk_size, u32* total_groups,
    const std::vector<PartitionEntry>& partition_entries, std::vector<DataEntry>* data_entries)
{
  const u32 group_index = *total_groups;
  const u64 rounded_size = Common::AlignDown(size, VolumeWii::BLOCK_TOTAL_SIZE);
  const u32 groups = static_cast<u32>(Common::AlignUp(rounded_size, chunk_size) / chunk_size);
  *total_groups += groups;

  data_entries->emplace_back(partition_entries.size(), index);
  return PartitionDataEntry{Common::swap32(offset / VolumeWii::BLOCK_TOTAL_SIZE),
                            Common::swap32(size / VolumeWii::BLOCK_TOTAL_SIZE),
                            Common::swap32(group_index), Common::swap32(groups)};
}

ConversionResultCode WIAFileReader::SetUpDataEntriesForWriting(
    const VolumeDisc* volume, int chunk_size, u64 iso_size, u32* total_groups,
    std::vector<PartitionEntry>* partition_entries, std::vector<RawDataEntry>* raw_data_entries,
    std::vector<DataEntry>* data_entries)
{
  std::vector<Partition> partitions;
  if (volume && volume->IsEncryptedAndHashed())
    partitions = volume->GetPartitions();

  std::sort(partitions.begin(), partitions.end(),
            [](const Partition& a, const Partition& b) { return a.offset < b.offset; });

  *total_groups = 0;

  u64 last_partition_end_offset = 0;

  const auto add_raw_data_entry = [&](u64 offset, u64 size) {
    return AddRawDataEntry(offset, size, chunk_size, total_groups, raw_data_entries, data_entries);
  };

  const auto create_partition_data_entry = [&](u64 offset, u64 size, u32 index) {
    return CreatePartitionDataEntry(offset, size, index, chunk_size, total_groups,
                                    *partition_entries, data_entries);
  };

  for (const Partition& partition : partitions)
  {
    // If a partition is odd in some way that prevents us from encoding it as a partition,
    // we encode it as raw data instead by skipping the current loop iteration.
    // Partitions can always be encoded as raw data, but it is less space efficient.

    if (partition.offset < last_partition_end_offset)
    {
      WARN_LOG(DISCIO, "Overlapping partitions at %" PRIx64, partition.offset);
      continue;
    }

    if (volume->ReadSwapped<u32>(partition.offset, PARTITION_NONE) != u32(0x10001))
    {
      // This looks more like garbage data than an actual partition.
      // The values of data_offset and data_size will very likely also be garbage.
      // Some WBFS writing programs scrub the SSBB Masterpiece partitions without
      // removing them from the partition table, causing this problem.
      WARN_LOG(DISCIO, "Invalid partition at %" PRIx64, partition.offset);
      continue;
    }

    std::optional<u64> data_offset =
        volume->ReadSwappedAndShifted(partition.offset + 0x2b8, PARTITION_NONE);
    std::optional<u64> data_size =
        volume->ReadSwappedAndShifted(partition.offset + 0x2bc, PARTITION_NONE);

    if (!data_offset || !data_size)
      return ConversionResultCode::ReadFailed;

    const u64 data_start = partition.offset + *data_offset;
    const u64 data_end = data_start + *data_size;

    if (data_start % VolumeWii::BLOCK_TOTAL_SIZE != 0)
    {
      WARN_LOG(DISCIO, "Misaligned partition at %" PRIx64, partition.offset);
      continue;
    }

    if (*data_size < VolumeWii::BLOCK_TOTAL_SIZE)
    {
      WARN_LOG(DISCIO, "Very small partition at %" PRIx64, partition.offset);
      continue;
    }

    if (data_end > iso_size)
    {
      WARN_LOG(DISCIO, "Too large partition at %" PRIx64, partition.offset);
      *data_size = iso_size - *data_offset - partition.offset;
    }

    const std::optional<u64> fst_offset = GetFSTOffset(*volume, partition);
    const std::optional<u64> fst_size = GetFSTSize(*volume, partition);

    if (!fst_offset || !fst_size)
      return ConversionResultCode::ReadFailed;

    const IOS::ES::TicketReader& ticket = volume->GetTicket(partition);
    if (!ticket.IsValid())
      return ConversionResultCode::ReadFailed;

    add_raw_data_entry(last_partition_end_offset, partition.offset - last_partition_end_offset);

    add_raw_data_entry(partition.offset, *data_offset);

    const u64 fst_end = volume->PartitionOffsetToRawOffset(*fst_offset + *fst_size, partition);
    const u64 split_point = std::min(
        data_end, Common::AlignUp(fst_end - data_start, VolumeWii::GROUP_TOTAL_SIZE) + data_start);

    PartitionEntry partition_entry;
    partition_entry.partition_key = ticket.GetTitleKey();
    partition_entry.data_entries[0] =
        create_partition_data_entry(data_start, split_point - data_start, 0);
    partition_entry.data_entries[1] =
        create_partition_data_entry(split_point, data_end - split_point, 1);

    // Note: We can't simply set last_partition_end_offset to data_end,
    // because construct_partition_data_entry may have rounded it
    last_partition_end_offset =
        (Common::swap32(partition_entry.data_entries[1].first_sector) +
         Common::swap32(partition_entry.data_entries[1].number_of_sectors)) *
        VolumeWii::BLOCK_TOTAL_SIZE;

    partition_entries->emplace_back(std::move(partition_entry));
  }

  add_raw_data_entry(last_partition_end_offset, iso_size - last_partition_end_offset);

  return ConversionResultCode::Success;
}

std::optional<std::vector<u8>> WIAFileReader::Compress(Compressor* compressor, const u8* data,
                                                       size_t size)
{
  if (compressor)
  {
    if (!compressor->Start() || !compressor->Compress(data, size) || !compressor->End())
      return std::nullopt;

    data = compressor->GetData();
    size = compressor->GetSize();
  }

  return std::vector<u8>(data, data + size);
}

void WIAFileReader::SetUpCompressor(std::unique_ptr<Compressor>* compressor,
                                    WIACompressionType compression_type, int compression_level,
                                    WIAHeader2* header_2)
{
  switch (compression_type)
  {
  case WIACompressionType::None:
    *compressor = nullptr;
    break;
  case WIACompressionType::Purge:
    *compressor = std::make_unique<PurgeCompressor>();
    break;
  case WIACompressionType::Bzip2:
    *compressor = std::make_unique<Bzip2Compressor>(compression_level);
    break;
  case WIACompressionType::LZMA:
  case WIACompressionType::LZMA2:
  {
    u8* compressor_data = nullptr;
    u8* compressor_data_size = nullptr;

    if (header_2)
    {
      compressor_data = header_2->compressor_data;
      compressor_data_size = &header_2->compressor_data_size;
    }

    const bool lzma2 = compression_type == WIACompressionType::LZMA2;
    *compressor = std::make_unique<LZMACompressor>(lzma2, compression_level, compressor_data,
                                                   compressor_data_size);
    break;
  }
  case WIACompressionType::Zstd:
    *compressor = std::make_unique<ZstdCompressor>(compression_level);
    break;
  }
}

static bool AllAre(const std::vector<u8>& data, u8 x)
{
  return std::all_of(data.begin(), data.end(), [x](u8 y) { return x == y; });
};

static bool AllAre(const u8* begin, const u8* end, u8 x)
{
  return std::all_of(begin, end, [x](u8 y) { return x == y; });
};

static bool AllZero(const std::vector<u8>& data)
{
  return AllAre(data, 0);
};

static bool AllSame(const std::vector<u8>& data)
{
  return AllAre(data, data.front());
};

static bool AllSame(const u8* begin, const u8* end)
{
  return AllAre(begin, end, *begin);
};

ConversionResult<WIAFileReader::OutputParameters>
WIAFileReader::ProcessAndCompress(CompressThreadState* state, CompressParameters parameters,
                                  const std::vector<PartitionEntry>& partition_entries,
                                  const std::vector<DataEntry>& data_entries,
                                  std::map<ReuseID, GroupEntry>* reusable_groups,
                                  std::mutex* reusable_groups_mutex, u64 chunks_per_wii_group,
                                  u64 exception_lists_per_chunk, bool compressed_exception_lists)
{
  const auto reuse_id_exists = [reusable_groups,
                                reusable_groups_mutex](const std::optional<ReuseID>& reuse_id) {
    if (!reuse_id)
      return false;

    std::lock_guard guard(*reusable_groups_mutex);
    const auto it = reusable_groups->find(*reuse_id);
    return it != reusable_groups->end();
  };

  std::vector<OutputParametersEntry> output_entries;

  if (!parameters.data_entry->is_partition)
  {
    OutputParametersEntry& entry = output_entries.emplace_back();
    entry.main_data = std::move(parameters.data);

    if (AllSame(entry.main_data))
      entry.reuse_id = ReuseID{nullptr, entry.main_data.size(), false, entry.main_data.front()};
  }
  else
  {
    const PartitionEntry& partition_entry = partition_entries[parameters.data_entry->index];

    mbedtls_aes_context aes_context;
    mbedtls_aes_setkey_dec(&aes_context, partition_entry.partition_key.data(), 128);

    const u64 groups = Common::AlignUp(parameters.data.size(), VolumeWii::GROUP_TOTAL_SIZE) /
                       VolumeWii::GROUP_TOTAL_SIZE;

    ASSERT(parameters.data.size() % VolumeWii::BLOCK_TOTAL_SIZE == 0);
    const u64 blocks = parameters.data.size() / VolumeWii::BLOCK_TOTAL_SIZE;

    const u64 blocks_per_chunk = chunks_per_wii_group == 1 ?
                                     exception_lists_per_chunk * VolumeWii::BLOCKS_PER_GROUP :
                                     VolumeWii::BLOCKS_PER_GROUP / chunks_per_wii_group;

    const u64 chunks = Common::AlignUp(blocks, blocks_per_chunk) / blocks_per_chunk;

    const u64 in_data_per_chunk = blocks_per_chunk * VolumeWii::BLOCK_TOTAL_SIZE;
    const u64 out_data_per_chunk = blocks_per_chunk * VolumeWii::BLOCK_DATA_SIZE;

    const auto create_reuse_id = [&partition_entry, blocks,
                                  blocks_per_chunk](u8 value, bool decrypted, u64 block) {
      const u64 size = std::min(blocks - block, blocks_per_chunk) * VolumeWii::BLOCK_DATA_SIZE;
      return ReuseID{&partition_entry.partition_key, size, decrypted, value};
    };

    const u8* parameters_data_end = parameters.data.data() + parameters.data.size();
    for (u64 i = 0; i < chunks; ++i)
    {
      const u64 block_index = i * blocks_per_chunk;

      OutputParametersEntry& entry = output_entries.emplace_back();
      std::optional<ReuseID>& reuse_id = entry.reuse_id;

      // Set this chunk as reusable if the encrypted data is AllSame
      const u8* data = parameters.data.data() + block_index * VolumeWii::BLOCK_TOTAL_SIZE;
      if (AllSame(data, std::min(parameters_data_end, data + in_data_per_chunk)))
        reuse_id = create_reuse_id(parameters.data.front(), false, i * blocks_per_chunk);

      if (!reuse_id_exists(reuse_id) &&
          !(reuse_id && std::any_of(output_entries.begin(), output_entries.begin() + i,
                                    [reuse_id](const auto& e) { return e.reuse_id == reuse_id; })))
      {
        const u64 bytes_left = (blocks - block_index) * VolumeWii::BLOCK_DATA_SIZE;
        entry.main_data.resize(std::min(out_data_per_chunk, bytes_left));
      }
    }

    if (!std::all_of(output_entries.begin(), output_entries.end(),
                     [](const OutputParametersEntry& entry) { return entry.main_data.empty(); }))
    {
      const u64 number_of_exception_lists =
          chunks_per_wii_group == 1 ? exception_lists_per_chunk : chunks;
      std::vector<std::vector<HashExceptionEntry>> exception_lists(number_of_exception_lists);

      for (u64 i = 0; i < groups; ++i)
      {
        const u64 offset_of_group = i * VolumeWii::GROUP_TOTAL_SIZE;
        const u64 write_offset_of_group = i * VolumeWii::GROUP_DATA_SIZE;

        const u64 blocks_in_this_group =
            std::min<u64>(VolumeWii::BLOCKS_PER_GROUP, blocks - i * VolumeWii::BLOCKS_PER_GROUP);

        for (u32 j = 0; j < VolumeWii::BLOCKS_PER_GROUP; ++j)
        {
          if (j < blocks_in_this_group)
          {
            const u64 offset_of_block = offset_of_group + j * VolumeWii::BLOCK_TOTAL_SIZE;
            VolumeWii::DecryptBlockData(parameters.data.data() + offset_of_block,
                                        state->decryption_buffer[j].data(), &aes_context);
          }
          else
          {
            state->decryption_buffer[j].fill(0);
          }
        }

        VolumeWii::HashGroup(state->decryption_buffer.data(), state->hash_buffer.data());

        for (u64 j = 0; j < blocks_in_this_group; ++j)
        {
          const u64 chunk_index = j / blocks_per_chunk;
          const u64 block_index_in_chunk = j % blocks_per_chunk;

          if (output_entries[chunk_index].main_data.empty())
            continue;

          const u64 exception_list_index = chunks_per_wii_group == 1 ? i : chunk_index;

          const u64 offset_of_block = offset_of_group + j * VolumeWii::BLOCK_TOTAL_SIZE;
          const u64 hash_offset_of_block = block_index_in_chunk * VolumeWii::BLOCK_HEADER_SIZE;

          VolumeWii::HashBlock hashes;
          VolumeWii::DecryptBlockHashes(parameters.data.data() + offset_of_block, &hashes,
                                        &aes_context);

          const auto compare_hash = [&](size_t offset_in_block) {
            ASSERT(offset_in_block + sizeof(SHA1) <= VolumeWii::BLOCK_HEADER_SIZE);

            const u8* desired_hash = reinterpret_cast<u8*>(&hashes) + offset_in_block;
            const u8* computed_hash =
                reinterpret_cast<u8*>(&state->hash_buffer[j]) + offset_in_block;

            // We want to store a hash exception either if there is a hash mismatch, or if this
            // chunk might get reused in a context where it is paired up (within a 2 MiB Wii group)
            // with chunks that are different from the chunks it currently is paired up with, since
            // that affects the recalculated hashes. Chunks which have been marked as reusable at
            // this point normally have zero matching hashes anyway, so this shouldn't waste space.
            if ((chunks_per_wii_group != 1 && output_entries[chunk_index].reuse_id) ||
                !std::equal(desired_hash, desired_hash + sizeof(SHA1), computed_hash))
            {
              const u64 hash_offset = hash_offset_of_block + offset_in_block;
              ASSERT(hash_offset <= std::numeric_limits<u16>::max());

              HashExceptionEntry& exception = exception_lists[exception_list_index].emplace_back();
              exception.offset = static_cast<u16>(Common::swap16(hash_offset));
              std::memcpy(exception.hash.data(), desired_hash, sizeof(SHA1));
            }
          };

          const auto compare_hashes = [&compare_hash](size_t offset, size_t size) {
            for (size_t l = 0; l < size; l += sizeof(SHA1))
              // The std::min is to ensure that we don't go beyond the end of HashBlock with
              // padding_2, which is 32 bytes long (not divisible by sizeof(SHA1), which is 20).
              compare_hash(offset + std::min(l, size - sizeof(SHA1)));
          };

          using HashBlock = VolumeWii::HashBlock;
          compare_hashes(offsetof(HashBlock, h0), sizeof(HashBlock::h0));
          compare_hashes(offsetof(HashBlock, padding_0), sizeof(HashBlock::padding_0));
          compare_hashes(offsetof(HashBlock, h1), sizeof(HashBlock::h1));
          compare_hashes(offsetof(HashBlock, padding_1), sizeof(HashBlock::padding_1));
          compare_hashes(offsetof(HashBlock, h2), sizeof(HashBlock::h2));
          compare_hashes(offsetof(HashBlock, padding_2), sizeof(HashBlock::padding_2));
        }

        for (u64 j = 0; j < blocks_in_this_group; ++j)
        {
          const u64 chunk_index = j / blocks_per_chunk;
          const u64 block_index_in_chunk = j % blocks_per_chunk;

          OutputParametersEntry& entry = output_entries[chunk_index];
          if (entry.main_data.empty())
            continue;

          const u64 write_offset_of_block =
              write_offset_of_group + block_index_in_chunk * VolumeWii::BLOCK_DATA_SIZE;

          std::memcpy(entry.main_data.data() + write_offset_of_block,
                      state->decryption_buffer[j].data(), VolumeWii::BLOCK_DATA_SIZE);
        }
      }

      for (size_t i = 0; i < exception_lists.size(); ++i)
      {
        OutputParametersEntry& entry = output_entries[chunks_per_wii_group == 1 ? 0 : i];
        if (entry.main_data.empty())
          continue;

        const std::vector<HashExceptionEntry>& in = exception_lists[i];
        std::vector<u8>& out = entry.exception_lists;

        const u16 exceptions = Common::swap16(static_cast<u16>(in.size()));
        PushBack(&out, exceptions);
        for (const HashExceptionEntry& exception : in)
          PushBack(&out, exception);
      }

      for (u64 i = 0; i < output_entries.size(); ++i)
      {
        OutputParametersEntry& entry = output_entries[i];
        if (entry.main_data.empty() || entry.reuse_id)
          continue;

        // Set this chunk as reusable if it lacks exceptions and the decrypted data is AllSame
        if (AllZero(entry.exception_lists) && AllSame(parameters.data))
          entry.reuse_id = create_reuse_id(parameters.data.front(), true, i * blocks_per_chunk);
      }
    }
  }

  for (OutputParametersEntry& entry : output_entries)
  {
    if (entry.main_data.empty())
      continue;

    // Special case - a compressed size of zero is treated by WIA as meaning the data is all zeroes
    const bool all_zero = AllZero(entry.exception_lists) && AllZero(entry.main_data);

    if (all_zero || reuse_id_exists(entry.reuse_id))
    {
      entry.exception_lists.clear();
      entry.main_data.clear();
      continue;
    }

    if (state->compressor)
    {
      if (!state->compressor->Start())
        return ConversionResultCode::InternalError;
    }

    if (!entry.exception_lists.empty())
    {
      if (compressed_exception_lists && state->compressor)
      {
        if (!state->compressor->Compress(entry.exception_lists.data(),
                                         entry.exception_lists.size()))
        {
          return ConversionResultCode::InternalError;
        }

        entry.exception_lists.clear();
      }
      else
      {
        if (!compressed_exception_lists)
        {
          while (entry.exception_lists.size() % 4 != 0)
            entry.exception_lists.push_back(0);
        }

        if (state->compressor)
        {
          if (!state->compressor->AddPrecedingDataOnlyForPurgeHashing(entry.exception_lists.data(),
                                                                      entry.exception_lists.size()))
          {
            return ConversionResultCode::InternalError;
          }
        }
      }
    }

    if (state->compressor)
    {
      if (!state->compressor->Compress(entry.main_data.data(), entry.main_data.size()))
        return ConversionResultCode::InternalError;
      if (!state->compressor->End())
        return ConversionResultCode::InternalError;
    }

    if (state->compressor)
    {
      const u8* data = state->compressor->GetData();
      const size_t size = state->compressor->GetSize();

      entry.main_data.resize(size);
      std::copy(data, data + size, entry.main_data.data());
    }
  }

  return OutputParameters{std::move(output_entries), parameters.bytes_read, parameters.group_index};
}

ConversionResultCode WIAFileReader::Output(const OutputParameters& parameters,
                                           File::IOFile* outfile,
                                           std::map<ReuseID, GroupEntry>* reusable_groups,
                                           std::mutex* reusable_groups_mutex,
                                           GroupEntry* group_entry, u64* bytes_written)
{
  for (const OutputParametersEntry& entry : parameters.entries)
  {
    if (entry.reuse_id)
    {
      std::lock_guard guard(*reusable_groups_mutex);
      const auto it = reusable_groups->find(*entry.reuse_id);
      if (it != reusable_groups->end())
      {
        *group_entry = it->second;
        ++group_entry;
        continue;
      }
    }

    const size_t data_size = entry.exception_lists.size() + entry.main_data.size();

    if (*bytes_written >> 2 > std::numeric_limits<u32>::max())
      return ConversionResultCode::InternalError;

    ASSERT((*bytes_written & 3) == 0);
    group_entry->data_offset = Common::swap32(static_cast<u32>(*bytes_written >> 2));
    group_entry->data_size = Common::swap32(static_cast<u32>(data_size));

    if (!outfile->WriteArray(entry.exception_lists.data(), entry.exception_lists.size()))
      return ConversionResultCode::WriteFailed;
    if (!outfile->WriteArray(entry.main_data.data(), entry.main_data.size()))
      return ConversionResultCode::WriteFailed;

    *bytes_written += data_size;

    if (entry.reuse_id)
    {
      std::lock_guard guard(*reusable_groups_mutex);
      reusable_groups->emplace(*entry.reuse_id, *group_entry);
    }

    if (!PadTo4(outfile, bytes_written))
      return ConversionResultCode::WriteFailed;

    ++group_entry;
  }

  return ConversionResultCode::Success;
}

ConversionResultCode WIAFileReader::RunCallback(size_t groups_written, u64 bytes_read,
                                                u64 bytes_written, u32 total_groups, u64 iso_size,
                                                CompressCB callback, void* arg)
{
  int ratio = 0;
  if (bytes_read != 0)
    ratio = static_cast<int>(100 * bytes_written / bytes_read);

  const std::string text =
      StringFromFormat(Common::GetStringT("%i of %i blocks. Compression ratio %i%%").c_str(),
                       groups_written, total_groups, ratio);

  const float completion = static_cast<float>(bytes_read) / iso_size;

  return callback(text, completion, arg) ? ConversionResultCode::Success :
                                           ConversionResultCode::Canceled;
}

bool WIAFileReader::WriteHeader(File::IOFile* file, const u8* data, size_t size, u64 upper_bound,
                                u64* bytes_written, u64* offset_out)
{
  // The first part of the check is to prevent this from running more than once. If *bytes_written
  // is past the upper bound, we are already at the end of the file, so we don't need to do anything
  if (*bytes_written <= upper_bound && *bytes_written + size > upper_bound)
  {
    WARN_LOG(DISCIO, "Headers did not fit in the allocated space. Writing to end of file instead");
    if (!file->Seek(0, SEEK_END))
      return false;
    *bytes_written = file->Tell();
  }

  *offset_out = *bytes_written;
  if (!file->WriteArray(data, size))
    return false;
  *bytes_written += size;
  return PadTo4(file, bytes_written);
}

ConversionResultCode
WIAFileReader::ConvertToWIA(BlobReader* infile, const VolumeDisc* infile_volume,
                            File::IOFile* outfile, bool rvz, WIACompressionType compression_type,
                            int compression_level, int chunk_size, CompressCB callback, void* arg)
{
  ASSERT(infile->IsDataSizeAccurate());
  ASSERT(chunk_size > 0);

  const u64 iso_size = infile->GetDataSize();
  const u64 chunks_per_wii_group = std::max<u64>(1, VolumeWii::GROUP_TOTAL_SIZE / chunk_size);
  const u64 exception_lists_per_chunk = std::max<u64>(1, chunk_size / VolumeWii::GROUP_TOTAL_SIZE);
  const bool compressed_exception_lists = compression_type > WIACompressionType::Purge;

  u64 bytes_read = 0;
  u64 bytes_written = 0;
  size_t groups_processed = 0;

  WIAHeader1 header_1{};
  WIAHeader2 header_2{};

  std::vector<PartitionEntry> partition_entries;
  std::vector<RawDataEntry> raw_data_entries;
  std::vector<GroupEntry> group_entries;

  u32 total_groups;
  std::vector<DataEntry> data_entries;

  const ConversionResultCode set_up_data_entries_result =
      SetUpDataEntriesForWriting(infile_volume, chunk_size, iso_size, &total_groups,
                                 &partition_entries, &raw_data_entries, &data_entries);
  if (set_up_data_entries_result != ConversionResultCode::Success)
    return set_up_data_entries_result;

  group_entries.resize(total_groups);

  const size_t partition_entries_size = partition_entries.size() * sizeof(PartitionEntry);
  const size_t raw_data_entries_size = raw_data_entries.size() * sizeof(RawDataEntry);
  const size_t group_entries_size = group_entries.size() * sizeof(GroupEntry);

  // Conservative estimate for how much space will be taken up by headers.
  // The compression methods None and Purge have very predictable overhead,
  // and the other methods are able to compress group entries well
  const u64 headers_size_upper_bound =
      Common::AlignUp(sizeof(WIAHeader1) + sizeof(WIAHeader2) + partition_entries_size +
                          raw_data_entries_size + group_entries_size + 0x100,
                      VolumeWii::BLOCK_TOTAL_SIZE);

  std::vector<u8> buffer;

  buffer.resize(headers_size_upper_bound);
  outfile->WriteBytes(buffer.data(), buffer.size());
  bytes_written = headers_size_upper_bound;

  if (!infile->Read(0, header_2.disc_header.size(), header_2.disc_header.data()))
    return ConversionResultCode::ReadFailed;
  // We intentially do not increment bytes_read here, since these bytes will be read again

  std::map<ReuseID, GroupEntry> reusable_groups;
  std::mutex reusable_groups_mutex;

  const auto set_up_compress_thread_state = [&](CompressThreadState* state) {
    SetUpCompressor(&state->compressor, compression_type, compression_level, nullptr);
    return ConversionResultCode::Success;
  };

  const auto process_and_compress = [&](CompressThreadState* state, CompressParameters parameters) {
    return ProcessAndCompress(state, std::move(parameters), partition_entries, data_entries,
                              &reusable_groups, &reusable_groups_mutex, chunks_per_wii_group,
                              exception_lists_per_chunk, compressed_exception_lists);
  };

  const auto output = [&](OutputParameters parameters) {
    const ConversionResultCode result =
        Output(parameters, outfile, &reusable_groups, &reusable_groups_mutex,
               &group_entries[parameters.group_index], &bytes_written);

    if (result != ConversionResultCode::Success)
      return result;

    return RunCallback(parameters.group_index + parameters.entries.size(), parameters.bytes_read,
                       bytes_written, total_groups, iso_size, callback, arg);
  };

  MultithreadedCompressor<CompressThreadState, CompressParameters, OutputParameters> mt_compressor(
      set_up_compress_thread_state, process_and_compress, output);

  for (const DataEntry& data_entry : data_entries)
  {
    u32 first_group;
    u32 last_group;

    u64 data_offset;
    u64 data_size;

    if (data_entry.is_partition)
    {
      const PartitionEntry& partition_entry = partition_entries[data_entry.index];
      const PartitionDataEntry& partition_data_entry =
          partition_entry.data_entries[data_entry.partition_data_index];

      first_group = Common::swap32(partition_data_entry.group_index);
      last_group = first_group + Common::swap32(partition_data_entry.number_of_groups);

      data_offset = Common::swap32(partition_data_entry.first_sector) * VolumeWii::BLOCK_TOTAL_SIZE;
      data_size =
          Common::swap32(partition_data_entry.number_of_sectors) * VolumeWii::BLOCK_TOTAL_SIZE;
    }
    else
    {
      const RawDataEntry& raw_data_entry = raw_data_entries[data_entry.index];

      first_group = Common::swap32(raw_data_entry.group_index);
      last_group = first_group + Common::swap32(raw_data_entry.number_of_groups);

      data_offset = Common::swap64(raw_data_entry.data_offset);
      data_size = Common::swap64(raw_data_entry.data_size);

      const u64 skipped_data = data_offset % VolumeWii::BLOCK_TOTAL_SIZE;
      data_offset -= skipped_data;
      data_size += skipped_data;
    }

    ASSERT(groups_processed == first_group);
    ASSERT(bytes_read == data_offset);

    while (groups_processed < last_group)
    {
      const ConversionResultCode status = mt_compressor.GetStatus();
      if (status != ConversionResultCode::Success)
        return status;

      u64 bytes_to_read = chunk_size;
      if (data_entry.is_partition)
        bytes_to_read = std::max<u64>(bytes_to_read, VolumeWii::GROUP_TOTAL_SIZE);
      bytes_to_read = std::min<u64>(bytes_to_read, data_offset + data_size - bytes_read);

      buffer.resize(bytes_to_read);
      if (!infile->Read(bytes_read, bytes_to_read, buffer.data()))
        return ConversionResultCode::ReadFailed;
      bytes_read += bytes_to_read;

      mt_compressor.CompressAndWrite(
          CompressParameters{buffer, &data_entry, bytes_read, groups_processed});

      groups_processed += Common::AlignUp(bytes_to_read, chunk_size) / chunk_size;
    }
  }

  ASSERT(groups_processed == total_groups);
  ASSERT(bytes_read == iso_size);

  mt_compressor.Shutdown();

  const ConversionResultCode status = mt_compressor.GetStatus();
  if (status != ConversionResultCode::Success)
    return status;

  std::unique_ptr<Compressor> compressor;
  SetUpCompressor(&compressor, compression_type, compression_level, &header_2);

  const std::optional<std::vector<u8>> compressed_raw_data_entries = Compress(
      compressor.get(), reinterpret_cast<u8*>(raw_data_entries.data()), raw_data_entries_size);
  if (!compressed_raw_data_entries)
    return ConversionResultCode::InternalError;

  const std::optional<std::vector<u8>> compressed_group_entries =
      Compress(compressor.get(), reinterpret_cast<u8*>(group_entries.data()), group_entries_size);
  if (!compressed_group_entries)
    return ConversionResultCode::InternalError;

  bytes_written = sizeof(WIAHeader1) + sizeof(WIAHeader2);
  if (!outfile->Seek(sizeof(WIAHeader1) + sizeof(WIAHeader2), SEEK_SET))
    return ConversionResultCode::WriteFailed;

  u64 partition_entries_offset;
  if (!WriteHeader(outfile, reinterpret_cast<u8*>(partition_entries.data()), partition_entries_size,
                   headers_size_upper_bound, &bytes_written, &partition_entries_offset))
  {
    return ConversionResultCode::WriteFailed;
  }

  u64 raw_data_entries_offset;
  if (!WriteHeader(outfile, compressed_raw_data_entries->data(),
                   compressed_raw_data_entries->size(), headers_size_upper_bound, &bytes_written,
                   &raw_data_entries_offset))
  {
    return ConversionResultCode::WriteFailed;
  }

  u64 group_entries_offset;
  if (!WriteHeader(outfile, compressed_group_entries->data(), compressed_group_entries->size(),
                   headers_size_upper_bound, &bytes_written, &group_entries_offset))
  {
    return ConversionResultCode::WriteFailed;
  }

  u32 disc_type = 0;
  if (infile_volume)
  {
    if (infile_volume->GetVolumeType() == Platform::GameCubeDisc)
      disc_type = 1;
    else if (infile_volume->GetVolumeType() == Platform::WiiDisc)
      disc_type = 2;
  }

  header_2.disc_type = Common::swap32(disc_type);
  header_2.compression_type = Common::swap32(static_cast<u32>(compression_type));
  header_2.compression_level = Common::swap32(static_cast<u32>(compression_level));
  header_2.chunk_size = Common::swap32(static_cast<u32>(chunk_size));

  header_2.number_of_partition_entries = Common::swap32(static_cast<u32>(partition_entries.size()));
  header_2.partition_entry_size = Common::swap32(sizeof(PartitionEntry));
  header_2.partition_entries_offset = Common::swap64(partition_entries_offset);

  if (partition_entries.data() == nullptr)
    partition_entries.reserve(1);  // Avoid a crash in mbedtls_sha1_ret
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(partition_entries.data()), partition_entries_size,
                   header_2.partition_entries_hash.data());

  header_2.number_of_raw_data_entries = Common::swap32(static_cast<u32>(raw_data_entries.size()));
  header_2.raw_data_entries_offset = Common::swap64(raw_data_entries_offset);
  header_2.raw_data_entries_size =
      Common::swap32(static_cast<u32>(compressed_raw_data_entries->size()));

  header_2.number_of_group_entries = Common::swap32(static_cast<u32>(group_entries.size()));
  header_2.group_entries_offset = Common::swap64(group_entries_offset);
  header_2.group_entries_size = Common::swap32(static_cast<u32>(compressed_group_entries->size()));

  header_1.magic = rvz ? RVZ_MAGIC : WIA_MAGIC;
  header_1.version = Common::swap32(rvz ? RVZ_VERSION : WIA_VERSION);
  header_1.version_compatible =
      Common::swap32(rvz ? RVZ_VERSION_WRITE_COMPATIBLE : WIA_VERSION_WRITE_COMPATIBLE);
  header_1.header_2_size = Common::swap32(sizeof(WIAHeader2));
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(&header_2), sizeof(header_2),
                   header_1.header_2_hash.data());
  header_1.iso_file_size = Common::swap64(infile->GetDataSize());
  header_1.wia_file_size = Common::swap64(outfile->GetSize());
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(&header_1), offsetof(WIAHeader1, header_1_hash),
                   header_1.header_1_hash.data());

  if (!outfile->Seek(0, SEEK_SET))
    return ConversionResultCode::WriteFailed;

  if (!outfile->WriteArray(&header_1, 1))
    return ConversionResultCode::WriteFailed;
  if (!outfile->WriteArray(&header_2, 1))
    return ConversionResultCode::WriteFailed;

  return ConversionResultCode::Success;
}

bool ConvertToWIA(BlobReader* infile, const std::string& infile_path,
                  const std::string& outfile_path, bool rvz, WIACompressionType compression_type,
                  int compression_level, int chunk_size, CompressCB callback, void* arg)
{
  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  std::unique_ptr<VolumeDisc> infile_volume = CreateDisc(infile_path);

  const ConversionResultCode result =
      WIAFileReader::ConvertToWIA(infile, infile_volume.get(), &outfile, rvz, compression_type,
                                  compression_level, chunk_size, callback, arg);

  if (result == ConversionResultCode::ReadFailed)
    PanicAlertT("Failed to read from the input file \"%s\".", infile_path.c_str());

  if (result == ConversionResultCode::WriteFailed)
  {
    PanicAlertT("Failed to write the output file \"%s\".\n"
                "Check that you have enough space available on the target drive.",
                outfile_path.c_str());
  }

  if (result != ConversionResultCode::Success)
  {
    // Remove the incomplete output file
    outfile.Close();
    File::Delete(outfile_path);
  }

  return result == ConversionResultCode::Success;
}

}  // namespace DiscIO
