// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/WIABlob.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include <bzlib.h>
#include <lzma.h>
#include <mbedtls/sha1.h>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

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
  m_compression_type = static_cast<CompressionType>(compression_type);
  if (m_compression_type > CompressionType::LZMA2)
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

        if (!m_encryption_cache.EncryptGroups(
                offset - partition_data_offset, bytes_to_read, out_ptr, partition_data_offset,
                partition_total_sectors * VolumeWii::BLOCK_DATA_SIZE, partition.partition_key))
        {
          return false;
        }

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
  case CompressionType::None:
    decompressor = std::make_unique<NoneDecompressor>();
    break;
  case CompressionType::Purge:
    decompressor = std::make_unique<PurgeDecompressor>(decompressed_size);
    break;
  case CompressionType::Bzip2:
    decompressor = std::make_unique<Bzip2Decompressor>();
    break;
  case CompressionType::LZMA:
    decompressor = std::make_unique<LZMADecompressor>(false, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  case CompressionType::LZMA2:
    decompressor = std::make_unique<LZMADecompressor>(true, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  }

  const bool compressed_exception_lists = m_compression_type > CompressionType::Purge;

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
      m_options.dict_size = d == 40 ? 0xFFFFFFFF : (static_cast<u32>(2) | (d & 1)) << (d / 2 + 11);
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

}  // namespace DiscIO
