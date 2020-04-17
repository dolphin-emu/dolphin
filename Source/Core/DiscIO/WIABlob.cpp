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
#include <optional>
#include <utility>

#include <bzlib.h>
#include <lzma.h>
#include <mbedtls/sha1.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
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

  if (m_header_1.magic != WIA_MAGIC)
    return false;

  const u32 version = Common::swap32(m_header_1.version);
  const u32 version_compatible = Common::swap32(m_header_1.version_compatible);
  if (WIA_VERSION < version_compatible || WIA_VERSION_READ_COMPATIBLE > version)
  {
    ERROR_LOG(DISCIO, "Unsupported WIA version %s in %s", VersionToString(version).c_str(),
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
  if (chunk_size % VolumeWii::GROUP_TOTAL_SIZE != 0)
    return false;

  const u32 compression_type = Common::swap32(m_header_2.compression_type);
  m_compression_type = static_cast<WIACompressionType>(compression_type);
  if (m_compression_type > WIACompressionType::LZMA2)
  {
    ERROR_LOG(DISCIO, "Unsupported WIA compression type %u in %s", compression_type, path.c_str());
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

        bool hash_exception_error = false;
        if (!m_encryption_cache.EncryptGroups(
                offset - partition_data_offset, bytes_to_read, out_ptr, partition_data_offset,
                partition_total_sectors * VolumeWii::BLOCK_DATA_SIZE, partition.partition_key,
                [this, chunk_size, first_sector, partition_first_sector, &hash_exception_error](
                    VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP], u64 offset) {
                  const u64 partition_part_offset =
                      (first_sector - partition_first_sector) * VolumeWii::BLOCK_TOTAL_SIZE;
                  const u64 index =
                      (offset - partition_part_offset) % chunk_size / VolumeWii::GROUP_TOTAL_SIZE;

                  // EncryptGroups calls ReadWiiDecrypted, which populates m_cached_chunk
                  if (!m_cached_chunk.ApplyHashExceptions(hash_blocks, index))
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
                        chunk_size / VolumeWii::GROUP_DATA_SIZE))
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
    const u64 group_offset = data_offset + i * chunk_size;
    const u64 offset_in_group = *offset - group_offset;

    chunk_size = std::min(chunk_size, data_offset + data_size - group_offset);

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

bool WIAFileReader::PurgeCompressor::AddPrecedingDataOnlyForPurgeHashing(const u8* data, size_t size)
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
    *compressor_data_size_out = 5;

    ASSERT(m_options.lc < 9);
    ASSERT(m_options.lp < 5);
    ASSERT(m_options.pb < 5);
    compressor_data_out[0] = static_cast<u8>((m_options.pb * 5 + m_options.lp) * 9 + m_options.lc);

    // The dictionary size is stored as a 32-bit little endian unsigned integer
    static_assert(sizeof(m_options.dict_size) == sizeof(u32));
    std::memcpy(compressor_data_out + 1, &m_options.dict_size, sizeof(u32));
  }
  else
  {
    *compressor_data_size_out = 1;

    u8 encoded_dict_size = 0;
    while (encoded_dict_size < 40 && m_options.dict_size > LZMA2DictionarySize(encoded_dict_size))
      ++encoded_dict_size;

    compressor_data_out[0] = encoded_dict_size;
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

      if (m_out.bytes_written == expected_out_bytes && !m_decompressor->Done())
        return false;  // Decompressed size is larger than expected

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

bool WIAFileReader::Chunk::ApplyHashExceptions(
    VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP], u64 exception_list_index) const
{
  if (m_exception_lists > 0)
    return false;  // We still have exception lists left to read

  const u8* data = m_compressed_exception_lists ? m_out.data.data() : m_in.data.data();

  for (u64 i = exception_list_index; i > 0; --i)
    data += Common::swap16(data) * sizeof(HashExceptionEntry) + sizeof(u16);

  const u16 exceptions = Common::swap16(data);
  data += sizeof(u16);

  for (size_t i = 0; i < exceptions; ++i)
  {
    HashExceptionEntry exception;
    std::memcpy(&exception, data, sizeof(HashExceptionEntry));
    data += sizeof(HashExceptionEntry);

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

WIAFileReader::ConversionResult WIAFileReader::SetUpDataEntriesForWriting(
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
      return ConversionResult::ReadFailed;

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
      return ConversionResult::ReadFailed;

    const IOS::ES::TicketReader& ticket = volume->GetTicket(partition);
    if (!ticket.IsValid())
      return ConversionResult::ReadFailed;

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

  return ConversionResult::Success;
}

WIAFileReader::ConversionResult WIAFileReader::CompressAndWriteGroup(
    File::IOFile* file, u64* bytes_written, std::vector<GroupEntry>* group_entries,
    size_t* groups_written, Compressor* compressor, bool compressed_exception_lists,
    const std::vector<u8>& exception_lists, const std::vector<u8>& main_data)
{
  const u64 data_offset = *bytes_written;

  if (compressor)
  {
    if (!compressor->Start())
      return ConversionResult::InternalError;
  }

  if (!exception_lists.empty())
  {
    if (compressed_exception_lists && compressor)
    {
      if (!compressor->Compress(exception_lists.data(), exception_lists.size()))
        return ConversionResult::InternalError;
    }
    else
    {
      *bytes_written += exception_lists.size();
      if (!file->WriteArray(exception_lists.data(), exception_lists.size()))
        return ConversionResult::WriteFailed;

      const u64 offset_of_padding = *bytes_written;
      if (!compressed_exception_lists)
      {
        if (!PadTo4(file, bytes_written))
          return ConversionResult::WriteFailed;
      }
      const u64 padding_written = *bytes_written - offset_of_padding;

      // Some extra stuff we have to do because Purge for some reason is supposed to hash
      // the exception lists and the following padding but not actually compress them...
      if (compressor)
      {
        if (!compressor->AddPrecedingDataOnlyForPurgeHashing(exception_lists.data(),
                                                             exception_lists.size()))
        {
          return ConversionResult::InternalError;
        }

        constexpr u32 ZEROES = 0;
        if (!compressor->AddPrecedingDataOnlyForPurgeHashing(reinterpret_cast<u8*>(ZEROES),
                                                             padding_written))
        {
          return ConversionResult::InternalError;
        }
      }
    }
  }

  if (compressor)
  {
    if (!compressor->Compress(main_data.data(), main_data.size()))
      return ConversionResult::InternalError;
    if (!compressor->End())
      return ConversionResult::InternalError;
  }

  const u8* data = compressor ? compressor->GetData() : main_data.data();
  const size_t size = compressor ? compressor->GetSize() : main_data.size();

  *bytes_written += size;
  if (!file->WriteArray(data, size))
    return ConversionResult::WriteFailed;

  if (*bytes_written >> 2 > std::numeric_limits<u32>::max())
    return ConversionResult::InternalError;

  ASSERT((data_offset & 3) == 0);
  GroupEntry& group_entry = (*group_entries)[*groups_written];
  group_entry.data_offset = Common::swap32(static_cast<u32>(data_offset >> 2));
  group_entry.data_size = Common::swap32(static_cast<u32>(*bytes_written - data_offset));
  ++*groups_written;

  if (!PadTo4(file, bytes_written))
    return ConversionResult::WriteFailed;

  return ConversionResult::Success;
}

WIAFileReader::ConversionResult
WIAFileReader::CompressAndWrite(File::IOFile* file, u64* bytes_written, Compressor* compressor,
                                const u8* data, size_t size, size_t* size_out)
{
  if (compressor)
  {
    if (!compressor->Start() || !compressor->Compress(data, size) || !compressor->End())
      return ConversionResult::InternalError;

    data = compressor->GetData();
    size = compressor->GetSize();
  }

  *size_out = size;

  *bytes_written += size;
  if (!file->WriteArray(data, size))
    return ConversionResult::WriteFailed;

  if (!PadTo4(file, bytes_written))
    return ConversionResult::WriteFailed;

  return ConversionResult::Success;
}

WIAFileReader::ConversionResult
WIAFileReader::ConvertToWIA(BlobReader* infile, const VolumeDisc* infile_volume,
                            File::IOFile* outfile, WIACompressionType compression_type,
                            int compression_level, int chunk_size, CompressCB callback, void* arg)
{
  ASSERT(infile->IsDataSizeAccurate());
  ASSERT(chunk_size > 0);

  const u64 iso_size = infile->GetDataSize();
  const u64 exception_lists_per_chunk = chunk_size / VolumeWii::GROUP_TOTAL_SIZE;
  const bool compressed_exception_lists = compression_type > WIACompressionType::Purge;

  u64 bytes_read = 0;
  u64 bytes_written = 0;
  size_t groups_written = 0;

  // These two headers will be filled in with proper values later
  WIAHeader1 header_1{};
  WIAHeader2 header_2{};
  if (!outfile->WriteArray(&header_1, 1) || !outfile->WriteArray(&header_2, 1))
    return ConversionResult::WriteFailed;
  bytes_written += sizeof(WIAHeader1) + sizeof(WIAHeader2);
  if (!PadTo4(outfile, &bytes_written))
    return ConversionResult::WriteFailed;

  std::unique_ptr<Compressor> compressor;
  switch (compression_type)
  {
  case WIACompressionType::None:
    compressor = nullptr;
    break;
  case WIACompressionType::Purge:
    compressor = std::make_unique<PurgeCompressor>();
    break;
  case WIACompressionType::Bzip2:
    compressor = std::make_unique<Bzip2Compressor>(compression_level);
    break;
  case WIACompressionType::LZMA:
    compressor = std::make_unique<LZMACompressor>(
        false, compression_level, header_2.compressor_data, &header_2.compressor_data_size);
    break;
  case WIACompressionType::LZMA2:
    compressor = std::make_unique<LZMACompressor>(true, compression_level, header_2.compressor_data,
                                                  &header_2.compressor_data_size);
    break;
  }

  std::vector<PartitionEntry> partition_entries;
  std::vector<RawDataEntry> raw_data_entries;
  std::vector<GroupEntry> group_entries;

  const auto run_callback = [&] {
    int ratio = 0;
    if (bytes_read != 0)
      ratio = static_cast<int>(100 * bytes_written / bytes_read);

    const std::string temp =
        StringFromFormat(Common::GetStringT("%i of %i blocks. Compression ratio %i%%").c_str(),
                         groups_written, group_entries.size(), ratio);

    float completion = 0.0f;
    if (group_entries.size() != 0)
      completion = static_cast<float>(groups_written) / group_entries.size();

    return callback(temp, completion, arg);
  };

  if (!run_callback())
    return ConversionResult::Canceled;

  u32 total_groups;
  std::vector<DataEntry> data_entries;

  const ConversionResult set_up_data_entries_result =
      SetUpDataEntriesForWriting(infile_volume, chunk_size, iso_size, &total_groups,
                                 &partition_entries, &raw_data_entries, &data_entries);
  if (set_up_data_entries_result != ConversionResult::Success)
    return set_up_data_entries_result;

  group_entries.resize(total_groups);

  if (!infile->Read(0, header_2.disc_header.size(), header_2.disc_header.data()))
    return ConversionResult::ReadFailed;
  // We intentially do not increment bytes_read here, since these bytes will be read again

  using WiiBlockData = std::array<u8, VolumeWii::BLOCK_DATA_SIZE>;

  std::vector<u8> buffer;
  std::vector<u8> exceptions_buffer;
  std::vector<WiiBlockData> decryption_buffer;
  std::vector<VolumeWii::HashBlock> hash_buffer;

  if (!partition_entries.empty())
  {
    decryption_buffer.resize(VolumeWii::BLOCKS_PER_GROUP);
    hash_buffer.resize(VolumeWii::BLOCKS_PER_GROUP);
  }

  for (const DataEntry& data_entry : data_entries)
  {
    if (data_entry.is_partition)
    {
      const PartitionEntry& partition_entry = partition_entries[data_entry.index];
      const PartitionDataEntry& partition_data_entry =
          partition_entry.data_entries[data_entry.partition_data_index];

      const u32 first_group = Common::swap32(partition_data_entry.group_index);
      const u32 last_group = first_group + Common::swap32(partition_data_entry.number_of_groups);

      const u64 data_offset =
          Common::swap32(partition_data_entry.first_sector) * VolumeWii::BLOCK_TOTAL_SIZE;
      const u64 data_size =
          Common::swap32(partition_data_entry.number_of_sectors) * VolumeWii::BLOCK_TOTAL_SIZE;

      ASSERT(groups_written == first_group);
      ASSERT(bytes_read == data_offset);

      mbedtls_aes_context aes_context;
      mbedtls_aes_setkey_dec(&aes_context, partition_entry.partition_key.data(), 128);

      for (u32 i = first_group; i < last_group; ++i)
      {
        const u64 bytes_to_read = std::min<u64>(chunk_size, data_offset + data_size - bytes_read);

        const u64 groups = Common::AlignUp(bytes_to_read, VolumeWii::GROUP_TOTAL_SIZE) /
                           VolumeWii::GROUP_TOTAL_SIZE;

        ASSERT(bytes_to_read % VolumeWii::BLOCK_TOTAL_SIZE == 0);
        const u64 blocks = bytes_to_read / VolumeWii::BLOCK_TOTAL_SIZE;
        const u64 bytes_to_write = blocks * VolumeWii::BLOCK_DATA_SIZE;

        buffer.resize(bytes_to_read);
        if (!infile->Read(bytes_read, bytes_to_read, buffer.data()))
          return ConversionResult::ReadFailed;
        bytes_read += bytes_to_read;

        std::vector<std::vector<HashExceptionEntry>> exception_lists(exception_lists_per_chunk);

        for (u64 j = 0; j < groups; ++j)
        {
          const u64 offset_of_group = j * VolumeWii::GROUP_TOTAL_SIZE;
          const u64 write_offset_of_group = j * VolumeWii::GROUP_DATA_SIZE;

          const u64 blocks_in_this_group =
              std::min<u64>(VolumeWii::BLOCKS_PER_GROUP, blocks - j * VolumeWii::BLOCKS_PER_GROUP);

          for (u32 k = 0; k < VolumeWii::BLOCKS_PER_GROUP; ++k)
          {
            if (k < blocks_in_this_group)
            {
              const u64 offset_of_block = offset_of_group + k * VolumeWii::BLOCK_TOTAL_SIZE;
              VolumeWii::DecryptBlockData(buffer.data() + offset_of_block,
                                          decryption_buffer[k].data(), &aes_context);
            }
            else
            {
              decryption_buffer[k].fill(0);
            }
          }

          VolumeWii::HashGroup(decryption_buffer.data(), hash_buffer.data());

          for (u64 k = 0; k < blocks_in_this_group; ++k)
          {
            const u64 offset_of_block = offset_of_group + k * VolumeWii::BLOCK_TOTAL_SIZE;
            const u64 hash_offset_of_block = k * VolumeWii::BLOCK_HEADER_SIZE;

            VolumeWii::HashBlock hashes;
            VolumeWii::DecryptBlockHashes(buffer.data() + offset_of_block, &hashes, &aes_context);

            const auto compare_hash = [&](size_t offset_in_block) {
              ASSERT(offset_in_block + sizeof(SHA1) <= VolumeWii::BLOCK_HEADER_SIZE);

              const u8* desired_hash = reinterpret_cast<u8*>(&hashes) + offset_in_block;
              const u8* computed_hash = reinterpret_cast<u8*>(&hash_buffer[k]) + offset_in_block;

              if (!std::equal(desired_hash, desired_hash + sizeof(SHA1), computed_hash))
              {
                const u64 hash_offset = hash_offset_of_block + offset_in_block;
                ASSERT(hash_offset <= std::numeric_limits<u16>::max());

                HashExceptionEntry& exception = exception_lists[j].emplace_back();
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

          for (u64 k = 0; k < blocks_in_this_group; ++k)
          {
            std::memcpy(buffer.data() + write_offset_of_group + k * VolumeWii::BLOCK_DATA_SIZE,
                        decryption_buffer[k].data(), VolumeWii::BLOCK_DATA_SIZE);
          }
        }

        exceptions_buffer.clear();
        for (const std::vector<HashExceptionEntry>& exception_list : exception_lists)
        {
          const u16 exceptions = Common::swap16(static_cast<u16>(exception_list.size()));
          PushBack(&exceptions_buffer, exceptions);
          for (const HashExceptionEntry& exception : exception_list)
            PushBack(&exceptions_buffer, exception);
        }

        buffer.resize(bytes_to_write);

        const ConversionResult write_result = CompressAndWriteGroup(
            outfile, &bytes_written, &group_entries, &groups_written, compressor.get(),
            compressed_exception_lists, exceptions_buffer, buffer);

        if (write_result != ConversionResult::Success)
          return write_result;

        if (!run_callback())
          return ConversionResult::Canceled;
      }
    }
    else
    {
      const RawDataEntry& raw_data_entry = raw_data_entries[data_entry.index];

      const u32 first_group = Common::swap32(raw_data_entry.group_index);
      const u32 last_group = first_group + Common::swap32(raw_data_entry.number_of_groups);

      u64 data_offset = Common::swap64(raw_data_entry.data_offset);
      u64 data_size = Common::swap64(raw_data_entry.data_size);

      const u64 skipped_data = data_offset % VolumeWii::BLOCK_TOTAL_SIZE;
      data_offset -= skipped_data;
      data_size += skipped_data;

      ASSERT(groups_written == first_group);
      ASSERT(bytes_read == data_offset);

      exceptions_buffer.clear();

      for (u32 i = first_group; i < last_group; ++i)
      {
        const u64 bytes_to_read = std::min<u64>(chunk_size, data_offset + data_size - bytes_read);

        buffer.resize(bytes_to_read);

        if (!infile->Read(bytes_read, bytes_to_read, buffer.data()))
          return ConversionResult::ReadFailed;
        bytes_read += bytes_to_read;

        const ConversionResult write_result = CompressAndWriteGroup(
            outfile, &bytes_written, &group_entries, &groups_written, compressor.get(),
            compressed_exception_lists, exceptions_buffer, buffer);

        if (write_result != ConversionResult::Success)
          return write_result;

        if (!run_callback())
          return ConversionResult::Canceled;
      }
    }
  }

  ASSERT(groups_written == total_groups);
  ASSERT(bytes_read == iso_size);

  const u64 partition_entries_offset = bytes_written;
  const u64 partition_entries_size = partition_entries.size() * sizeof(PartitionEntry);
  if (!outfile->WriteArray(partition_entries.data(), partition_entries.size()))
    return ConversionResult::WriteFailed;
  bytes_written += partition_entries_size;
  if (!PadTo4(outfile, &bytes_written))
    return ConversionResult::WriteFailed;

  const u64 raw_data_entries_offset = bytes_written;
  size_t raw_data_entries_size = raw_data_entries.size() * sizeof(RawDataEntry);
  const ConversionResult raw_data_result = CompressAndWrite(
      outfile, &bytes_written, compressor.get(), reinterpret_cast<u8*>(raw_data_entries.data()),
      raw_data_entries_size, &raw_data_entries_size);

  const u64 group_entries_offset = bytes_written;
  size_t group_entries_size = group_entries.size() * sizeof(GroupEntry);
  const ConversionResult groups_result = CompressAndWrite(
      outfile, &bytes_written, compressor.get(), reinterpret_cast<u8*>(group_entries.data()),
      group_entries_size, &group_entries_size);

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
  header_2.raw_data_entries_size = Common::swap32(static_cast<u32>(raw_data_entries_size));

  header_2.number_of_group_entries = Common::swap32(static_cast<u32>(group_entries.size()));
  header_2.group_entries_offset = Common::swap64(group_entries_offset);
  header_2.group_entries_size = Common::swap32(static_cast<u32>(group_entries_size));

  header_1.magic = WIA_MAGIC;
  header_1.version = Common::swap32(WIA_VERSION);
  header_1.version_compatible = Common::swap32(WIA_VERSION_WRITE_COMPATIBLE);
  header_1.header_2_size = Common::swap32(sizeof(WIAHeader2));
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(&header_2), sizeof(header_2),
                   header_1.header_2_hash.data());
  header_1.iso_file_size = Common::swap64(infile->GetDataSize());
  header_1.wia_file_size = Common::swap64(bytes_written);
  mbedtls_sha1_ret(reinterpret_cast<const u8*>(&header_1), offsetof(WIAHeader1, header_1_hash),
                   header_1.header_1_hash.data());

  if (!outfile->Seek(0, SEEK_SET))
    return ConversionResult::WriteFailed;
  if (!outfile->WriteArray(&header_1, 1) || !outfile->WriteArray(&header_2, 1))
    return ConversionResult::WriteFailed;

  return ConversionResult::Success;
}

bool ConvertToWIA(BlobReader* infile, const std::string& infile_path,
                  const std::string& outfile_path, WIACompressionType compression_type,
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

  const WIAFileReader::ConversionResult result =
      WIAFileReader::ConvertToWIA(infile, infile_volume.get(), &outfile, compression_type,
                                  compression_level, chunk_size, callback, arg);

  if (result == WIAFileReader::ConversionResult::ReadFailed)
    PanicAlertT("Failed to read from the input file \"%s\".", infile_path.c_str());

  if (result == WIAFileReader::ConversionResult::WriteFailed)
  {
    PanicAlertT("Failed to write the output file \"%s\".\n"
                "Check that you have enough space available on the target drive.",
                outfile_path.c_str());
  }

  if (result != WIAFileReader::ConversionResult::Success)
  {
    // Remove the incomplete output file
    outfile.Close();
    File::Delete(outfile_path);
  }

  return result == WIAFileReader::ConversionResult::Success;
}

}  // namespace DiscIO
