// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WIABlob.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <zstd.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/LaggedFibonacciGenerator.h"
#include "DiscIO/MultithreadedCompressor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"
#include "DiscIO/WIACompression.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
static void PushBack(std::vector<u8>* vector, const u8* begin, const u8* end)
{
  const size_t offset_in_vector = vector->size();
  vector->resize(offset_in_vector + (end - begin));
  std::copy(begin, end, vector->data() + offset_in_vector);
}

template <typename T>
static void PushBack(std::vector<u8>* vector, const T& x)
{
  static_assert(std::is_trivially_copyable_v<T>);

  const u8* x_ptr = reinterpret_cast<const u8*>(&x);
  PushBack(vector, x_ptr, x_ptr + sizeof(T));
}

std::pair<int, int> GetAllowedCompressionLevels(WIARVZCompressionType compression_type, bool gui)
{
  switch (compression_type)
  {
  case WIARVZCompressionType::Bzip2:
  case WIARVZCompressionType::LZMA:
  case WIARVZCompressionType::LZMA2:
    return {1, 9};
  case WIARVZCompressionType::Zstd:
    // The actual minimum level can be gotten by calling ZSTD_minCLevel(). However, returning that
    // would make the UI rather weird, because it is a negative number with very large magnitude.
    // Note: Level 0 is a special number which means "default level" (level 3 as of this writing).
    if (gui)
      return {1, ZSTD_maxCLevel()};
    else
      return {ZSTD_minCLevel(), ZSTD_maxCLevel()};
  default:
    return {0, -1};
  }
}

template <bool RVZ>
WIARVZFileReader<RVZ>::WIARVZFileReader(File::IOFile file, const std::string& path)
    : m_file(std::move(file)), m_path(path), m_encryption_cache(this)
{
  m_valid = Initialize(path);
}

template <bool RVZ>
WIARVZFileReader<RVZ>::~WIARVZFileReader() = default;

template <bool RVZ>
bool WIARVZFileReader<RVZ>::Initialize(const std::string& path)
{
  if (!m_file.Seek(0, File::SeekOrigin::Begin) || !m_file.ReadArray(&m_header_1, 1))
    return false;

  if ((!RVZ && m_header_1.magic != WIA_MAGIC) || (RVZ && m_header_1.magic != RVZ_MAGIC))
    return false;

  const u32 version = RVZ ? RVZ_VERSION : WIA_VERSION;
  const u32 version_read_compatible =
      RVZ ? RVZ_VERSION_READ_COMPATIBLE : WIA_VERSION_READ_COMPATIBLE;

  const u32 file_version = Common::swap32(m_header_1.version);
  const u32 file_version_compatible = Common::swap32(m_header_1.version_compatible);

  if (version < file_version_compatible || version_read_compatible > file_version)
  {
    ERROR_LOG_FMT(DISCIO, "Unsupported version {} in {}", VersionToString(file_version), path);
    return false;
  }

  const auto header_1_actual_hash = Common::SHA1::CalculateDigest(
      reinterpret_cast<const u8*>(&m_header_1), sizeof(m_header_1) - Common::SHA1::DIGEST_LEN);
  if (m_header_1.header_1_hash != header_1_actual_hash)
    return false;

  if (Common::swap64(m_header_1.wia_file_size) != m_file.GetSize())
  {
    ERROR_LOG_FMT(DISCIO, "File size is incorrect for {}", path);
    return false;
  }

  const u32 header_2_size = Common::swap32(m_header_1.header_2_size);
  const u32 header_2_min_size = sizeof(WIAHeader2) - sizeof(WIAHeader2::compressor_data);
  if (header_2_size < header_2_min_size)
    return false;

  std::vector<u8> header_2(header_2_size);
  if (!m_file.ReadBytes(header_2.data(), header_2.size()))
    return false;

  const auto header_2_actual_hash = Common::SHA1::CalculateDigest(header_2);
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
  if ((!RVZ || chunk_size < VolumeWii::BLOCK_TOTAL_SIZE || !is_power_of_two(chunk_size)) &&
      chunk_size % VolumeWii::GROUP_TOTAL_SIZE != 0)
  {
    return false;
  }

  const u32 compression_type = Common::swap32(m_header_2.compression_type);
  m_compression_type = static_cast<WIARVZCompressionType>(compression_type);
  if (m_compression_type > (RVZ ? WIARVZCompressionType::Zstd : WIARVZCompressionType::LZMA2) ||
      (RVZ && m_compression_type == WIARVZCompressionType::Purge))
  {
    ERROR_LOG_FMT(DISCIO, "Unsupported compression type {} in {}", compression_type, path);
    return false;
  }

  const size_t number_of_partition_entries = Common::swap32(m_header_2.number_of_partition_entries);
  const size_t partition_entry_size = Common::swap32(m_header_2.partition_entry_size);
  std::vector<u8> partition_entries(partition_entry_size * number_of_partition_entries);
  if (!m_file.Seek(Common::swap64(m_header_2.partition_entries_offset), File::SeekOrigin::Begin))
    return false;
  if (!m_file.ReadBytes(partition_entries.data(), partition_entries.size()))
    return false;

  const auto partition_entries_actual_hash = Common::SHA1::CalculateDigest(partition_entries);
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
                         number_of_raw_data_entries * sizeof(RawDataEntry), m_compression_type);
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
  Chunk& group_entries =
      ReadCompressedData(Common::swap64(m_header_2.group_entries_offset),
                         Common::swap32(m_header_2.group_entries_size),
                         number_of_group_entries * sizeof(GroupEntry), m_compression_type);
  if (!group_entries.ReadAll(&m_group_entries))
    return false;

  if (HasDataOverlap())
    return false;

  return true;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::HasDataOverlap() const
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

template <bool RVZ>
std::unique_ptr<WIARVZFileReader<RVZ>> WIARVZFileReader<RVZ>::Create(File::IOFile file,
                                                                     const std::string& path)
{
  std::unique_ptr<WIARVZFileReader> blob(new WIARVZFileReader(std::move(file), path));
  return blob->m_valid ? std::move(blob) : nullptr;
}

template <bool RVZ>
BlobType WIARVZFileReader<RVZ>::GetBlobType() const
{
  return RVZ ? BlobType::RVZ : BlobType::WIA;
}

template <bool RVZ>
std::unique_ptr<BlobReader> WIARVZFileReader<RVZ>::CopyReader() const
{
  return Create(m_file.Duplicate("rb"), m_path);
}

template <bool RVZ>
std::string WIARVZFileReader<RVZ>::GetCompressionMethod() const
{
  switch (m_compression_type)
  {
  case WIARVZCompressionType::Purge:
    return "Purge";
  case WIARVZCompressionType::Bzip2:
    return "bzip2";
  case WIARVZCompressionType::LZMA:
    return "LZMA";
  case WIARVZCompressionType::LZMA2:
    return "LZMA2";
  case WIARVZCompressionType::Zstd:
    return "Zstandard";
  default:
    return {};
  }
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::Read(u64 offset, u64 size, u8* out_ptr)
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
                    VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP], u64 offset_) {
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

template <bool RVZ>
const typename WIARVZFileReader<RVZ>::PartitionEntry*
WIARVZFileReader<RVZ>::GetPartition(u64 partition_data_offset, u32* partition_first_sector) const
{
  const auto it = m_data_entries.upper_bound(partition_data_offset);
  if (it == m_data_entries.end() || !it->second.is_partition)
    return nullptr;

  const PartitionEntry* partition = &m_partition_entries[it->second.index];
  *partition_first_sector = Common::swap32(partition->data_entries[0].first_sector);
  if (partition_data_offset != *partition_first_sector * VolumeWii::BLOCK_TOTAL_SIZE)
    return nullptr;

  return partition;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::SupportsReadWiiDecrypted(u64 offset, u64 size,
                                                     u64 partition_data_offset) const
{
  u32 partition_first_sector;
  const PartitionEntry* partition = GetPartition(partition_data_offset, &partition_first_sector);
  if (!partition)
    return false;

  for (const PartitionDataEntry& data : partition->data_entries)
  {
    const u32 start_sector = Common::swap32(data.first_sector) - partition_first_sector;
    const u32 end_sector = start_sector + Common::swap32(data.number_of_sectors);

    if (offset + size <= end_sector * VolumeWii::BLOCK_DATA_SIZE)
      return true;
  }

  return false;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr,
                                             u64 partition_data_offset)
{
  u32 partition_first_sector;
  const PartitionEntry* partition = GetPartition(partition_data_offset, &partition_first_sector);
  if (!partition)
    return false;

  const u64 chunk_size = Common::swap32(m_header_2.chunk_size) * VolumeWii::BLOCK_DATA_SIZE /
                         VolumeWii::BLOCK_TOTAL_SIZE;

  for (const PartitionDataEntry& data : partition->data_entries)
  {
    if (size == 0)
      return true;

    const u64 data_offset =
        (Common::swap32(data.first_sector) - partition_first_sector) * VolumeWii::BLOCK_DATA_SIZE;
    const u64 data_size = Common::swap32(data.number_of_sectors) * VolumeWii::BLOCK_DATA_SIZE;

    if (!ReadFromGroups(
            &offset, &size, &out_ptr, chunk_size, VolumeWii::BLOCK_DATA_SIZE, data_offset,
            data_size, Common::swap32(data.group_index), Common::swap32(data.number_of_groups),
            std::max<u32>(1, static_cast<u32>(chunk_size / VolumeWii::GROUP_DATA_SIZE))))
    {
      return false;
    }
  }

  return size == 0;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::ReadFromGroups(u64* offset, u64* size, u8** out_ptr, u64 chunk_size,
                                           u32 sector_size, u64 data_offset, u64 data_size,
                                           u32 group_index, u32 number_of_groups,
                                           u32 exception_lists)
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
    u32 group_data_size = Common::swap32(group.data_size);

    WIARVZCompressionType compression_type = m_compression_type;
    u32 rvz_packed_size = 0;
    if constexpr (RVZ)
    {
      if ((group_data_size & 0x80000000) == 0)
        compression_type = WIARVZCompressionType::None;

      group_data_size &= 0x7FFFFFFF;

      rvz_packed_size = Common::swap32(group.rvz_packed_size);
    }

    if (group_data_size == 0)
    {
      std::memset(*out_ptr, 0, bytes_to_read);
    }
    else
    {
      const u64 group_offset_in_file = static_cast<u64>(Common::swap32(group.data_offset)) << 2;

      Chunk& chunk =
          ReadCompressedData(group_offset_in_file, group_data_size, chunk_size, compression_type,
                             exception_lists, rvz_packed_size, group_offset_in_data);

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

template <bool RVZ>
typename WIARVZFileReader<RVZ>::Chunk&
WIARVZFileReader<RVZ>::ReadCompressedData(u64 offset_in_file, u64 compressed_size,
                                          u64 decompressed_size,
                                          WIARVZCompressionType compression_type,
                                          u32 exception_lists, u32 rvz_packed_size, u64 data_offset)
{
  if (offset_in_file == m_cached_chunk_offset)
    return m_cached_chunk;

  std::unique_ptr<Decompressor> decompressor;
  switch (compression_type)
  {
  case WIARVZCompressionType::None:
    decompressor = std::make_unique<NoneDecompressor>();
    break;
  case WIARVZCompressionType::Purge:
    decompressor = std::make_unique<PurgeDecompressor>(rvz_packed_size == 0 ? decompressed_size :
                                                                              rvz_packed_size);
    break;
  case WIARVZCompressionType::Bzip2:
    decompressor = std::make_unique<Bzip2Decompressor>();
    break;
  case WIARVZCompressionType::LZMA:
    decompressor = std::make_unique<LZMADecompressor>(false, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  case WIARVZCompressionType::LZMA2:
    decompressor = std::make_unique<LZMADecompressor>(true, m_header_2.compressor_data,
                                                      m_header_2.compressor_data_size);
    break;
  case WIARVZCompressionType::Zstd:
    decompressor = std::make_unique<ZstdDecompressor>();
    break;
  }

  const bool compressed_exception_lists = compression_type > WIARVZCompressionType::Purge;

  m_cached_chunk =
      Chunk(&m_file, offset_in_file, compressed_size, decompressed_size, exception_lists,
            compressed_exception_lists, rvz_packed_size, data_offset, std::move(decompressor));
  m_cached_chunk_offset = offset_in_file;
  return m_cached_chunk;
}

template <bool RVZ>
std::string WIARVZFileReader<RVZ>::VersionToString(u32 version)
{
  const u8 a = version >> 24;
  const u8 b = (version >> 16) & 0xff;
  const u8 c = (version >> 8) & 0xff;
  const u8 d = version & 0xff;

  if (d == 0 || d == 0xff)
    return fmt::format("{}.{:02x}.{:02x}", a, b, c);
  else
    return fmt::format("{}.{:02x}.{:02x}.beta{}", a, b, c, d);
}

template <bool RVZ>
WIARVZFileReader<RVZ>::Chunk::Chunk() = default;

template <bool RVZ>
WIARVZFileReader<RVZ>::Chunk::Chunk(File::IOFile* file, u64 offset_in_file, u64 compressed_size,
                                    u64 decompressed_size, u32 exception_lists,
                                    bool compressed_exception_lists, u32 rvz_packed_size,
                                    u64 data_offset, std::unique_ptr<Decompressor> decompressor)
    : m_decompressor(std::move(decompressor)), m_file(file), m_offset_in_file(offset_in_file),
      m_exception_lists(exception_lists), m_compressed_exception_lists(compressed_exception_lists),
      m_rvz_packed_size(rvz_packed_size), m_data_offset(data_offset)
{
  constexpr size_t MAX_SIZE_PER_EXCEPTION_LIST =
      Common::AlignUp(VolumeWii::BLOCK_HEADER_SIZE, Common::SHA1::DIGEST_LEN) /
          Common::SHA1::DIGEST_LEN * VolumeWii::BLOCKS_PER_GROUP * sizeof(HashExceptionEntry) +
      sizeof(u16);

  m_out_bytes_allocated_for_exceptions =
      m_compressed_exception_lists ? MAX_SIZE_PER_EXCEPTION_LIST * m_exception_lists : 0;

  m_in.data.resize(compressed_size);
  m_out.data.resize(decompressed_size + m_out_bytes_allocated_for_exceptions);
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::Chunk::Read(u64 offset, u64 size, u8* out_ptr)
{
  if (!m_decompressor || !m_file ||
      offset + size > m_out.data.size() - m_out_bytes_allocated_for_exceptions)
  {
    return false;
  }

  while (offset + size > GetOutBytesWrittenExcludingExceptions())
  {
    u64 bytes_to_read;
    if (offset + size == m_out.data.size())
    {
      // Read all the remaining data.
      bytes_to_read = m_in.data.size() - m_in.bytes_written;
    }
    else
    {
      // Pick a suitable amount of compressed data to read. We have to ensure that bytes_to_read
      // is larger than 0 and smaller than or equal to the number of bytes available to read,
      // but the rest is a bit arbitrary and could be changed.

      // The compressed data is probably not much bigger than the decompressed data.
      // Add a few bytes for possible compression overhead and for any hash exceptions.
      bytes_to_read = offset + size - GetOutBytesWrittenExcludingExceptions() + 0x100;

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

    if (!m_file->Seek(m_offset_in_file, File::SeekOrigin::Begin))
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
      if (!Decompress())
        return false;
    }

    if (m_exception_lists > 0 && m_compressed_exception_lists)
    {
      if (!HandleExceptions(m_out.data.data(), m_out_bytes_allocated_for_exceptions,
                            m_out.bytes_written, &m_out_bytes_used_for_exceptions, false))
      {
        return false;
      }

      if (m_rvz_packed_size != 0 && m_exception_lists == 0)
      {
        if (!Decompress())
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

template <bool RVZ>
bool WIARVZFileReader<RVZ>::Chunk::Decompress()
{
  if (m_rvz_packed_size != 0 && m_exception_lists == 0)
  {
    const size_t bytes_to_move = m_out.bytes_written - m_out_bytes_used_for_exceptions;

    DecompressionBuffer in{std::vector<u8>(bytes_to_move), bytes_to_move};
    std::memcpy(in.data.data(), m_out.data.data() + m_out_bytes_used_for_exceptions, bytes_to_move);

    m_out.bytes_written = m_out_bytes_used_for_exceptions;

    m_decompressor = std::make_unique<RVZPackDecompressor>(std::move(m_decompressor), std::move(in),
                                                           m_data_offset, m_rvz_packed_size);

    m_rvz_packed_size = 0;
  }

  return m_decompressor->Decompress(m_in, &m_out, &m_in_bytes_read);
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::Chunk::HandleExceptions(const u8* data, size_t bytes_allocated,
                                                    size_t bytes_written, size_t* bytes_used,
                                                    bool align)
{
  while (m_exception_lists > 0)
  {
    if (sizeof(u16) + *bytes_used > bytes_allocated)
    {
      ERROR_LOG_FMT(DISCIO, "More hash exceptions than expected");
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
      ERROR_LOG_FMT(DISCIO, "More hash exceptions than expected");
      return false;
    }
    if (exception_list_size + *bytes_used > bytes_written)
      return true;

    *bytes_used += exception_list_size;
    --m_exception_lists;
  }

  return true;
}

template <bool RVZ>
void WIARVZFileReader<RVZ>::Chunk::GetHashExceptions(
    std::vector<HashExceptionEntry>* exception_list, u64 exception_list_index,
    u16 additional_offset) const
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

template <bool RVZ>
size_t WIARVZFileReader<RVZ>::Chunk::GetOutBytesWrittenExcludingExceptions() const
{
  return m_exception_lists == 0 ? m_out.bytes_written - m_out_bytes_used_for_exceptions : 0;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::ApplyHashExceptions(
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
    if (offset_in_block + Common::SHA1::DIGEST_LEN > VolumeWii::BLOCK_HEADER_SIZE)
      return false;

    std::memcpy(reinterpret_cast<u8*>(&hash_blocks[block_index]) + offset_in_block, &exception.hash,
                Common::SHA1::DIGEST_LEN);
  }

  return true;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::PadTo4(File::IOFile* file, u64* bytes_written)
{
  constexpr u32 ZEROES = 0;
  const u64 bytes_to_write = Common::AlignUp(*bytes_written, 4) - *bytes_written;
  if (bytes_to_write == 0)
    return true;

  *bytes_written += bytes_to_write;
  return file->WriteBytes(&ZEROES, bytes_to_write);
}

template <bool RVZ>
void WIARVZFileReader<RVZ>::AddRawDataEntry(u64 offset, u64 size, int chunk_size, u32* total_groups,
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

template <bool RVZ>
typename WIARVZFileReader<RVZ>::PartitionDataEntry WIARVZFileReader<RVZ>::CreatePartitionDataEntry(
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

template <bool RVZ>
ConversionResultCode WIARVZFileReader<RVZ>::SetUpDataEntriesForWriting(
    const VolumeDisc* volume, int chunk_size, u64 iso_size, u32* total_groups,
    std::vector<PartitionEntry>* partition_entries, std::vector<RawDataEntry>* raw_data_entries,
    std::vector<DataEntry>* data_entries, std::vector<const FileSystem*>* partition_file_systems)
{
  std::vector<Partition> partitions;
  if (volume && volume->HasWiiHashes() && volume->HasWiiEncryption())
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
      WARN_LOG_FMT(DISCIO, "Overlapping partitions at {:x}", partition.offset);
      continue;
    }

    if (volume->ReadSwapped<u32>(partition.offset, PARTITION_NONE) != 0x10001U)
    {
      // This looks more like garbage data than an actual partition.
      // The values of data_offset and data_size will very likely also be garbage.
      // Some WBFS writing programs scrub the SSBB Masterpiece partitions without
      // removing them from the partition table, causing this problem.
      WARN_LOG_FMT(DISCIO, "Invalid partition at {:x}", partition.offset);
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
      WARN_LOG_FMT(DISCIO, "Misaligned partition at {:x}", partition.offset);
      continue;
    }

    if (*data_size < VolumeWii::BLOCK_TOTAL_SIZE)
    {
      WARN_LOG_FMT(DISCIO, "Very small partition at {:x}", partition.offset);
      continue;
    }

    if (data_end > iso_size)
    {
      WARN_LOG_FMT(DISCIO, "Too large partition at {:x}", partition.offset);
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
    partition_file_systems->emplace_back(volume->GetFileSystem(partition));
  }

  add_raw_data_entry(last_partition_end_offset, iso_size - last_partition_end_offset);

  return ConversionResultCode::Success;
}

template <bool RVZ>
std::optional<std::vector<u8>> WIARVZFileReader<RVZ>::Compress(Compressor* compressor,
                                                               const u8* data, size_t size)
{
  if (compressor)
  {
    if (!compressor->Start(size) || !compressor->Compress(data, size) || !compressor->End())
      return std::nullopt;

    data = compressor->GetData();
    size = compressor->GetSize();
  }

  return std::vector<u8>(data, data + size);
}

template <bool RVZ>
void WIARVZFileReader<RVZ>::SetUpCompressor(std::unique_ptr<Compressor>* compressor,
                                            WIARVZCompressionType compression_type,
                                            int compression_level, WIAHeader2* header_2)
{
  switch (compression_type)
  {
  case WIARVZCompressionType::None:
    *compressor = nullptr;
    break;
  case WIARVZCompressionType::Purge:
    *compressor = std::make_unique<PurgeCompressor>();
    break;
  case WIARVZCompressionType::Bzip2:
    *compressor = std::make_unique<Bzip2Compressor>(compression_level);
    break;
  case WIARVZCompressionType::LZMA:
  case WIARVZCompressionType::LZMA2:
  {
    u8* compressor_data = nullptr;
    u8* compressor_data_size = nullptr;

    if (header_2)
    {
      compressor_data = header_2->compressor_data;
      compressor_data_size = &header_2->compressor_data_size;
    }

    const bool lzma2 = compression_type == WIARVZCompressionType::LZMA2;
    *compressor = std::make_unique<LZMACompressor>(lzma2, compression_level, compressor_data,
                                                   compressor_data_size);
    break;
  }
  case WIARVZCompressionType::Zstd:
    *compressor = std::make_unique<ZstdCompressor>(compression_level);
    break;
  }
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::TryReuse(std::map<ReuseID, GroupEntry>* reusable_groups,
                                     std::mutex* reusable_groups_mutex,
                                     OutputParametersEntry* entry)
{
  if (entry->reused_group)
    return true;

  if (!entry->reuse_id)
    return false;

  std::lock_guard guard(*reusable_groups_mutex);
  const auto it = reusable_groups->find(*entry->reuse_id);
  if (it == reusable_groups->end())
    return false;

  entry->reused_group = it->second;
  return true;
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

template <typename OutputParametersEntry>
static void RVZPack(const u8* in, OutputParametersEntry* out, u64 bytes_per_chunk, size_t chunks,
                    u64 total_size, u64 data_offset, bool multipart, bool allow_junk_reuse,
                    bool compression, const FileSystem* file_system)
{
  using Seed = std::array<u32, LaggedFibonacciGenerator::SEED_SIZE>;
  struct JunkInfo
  {
    size_t start_offset;
    Seed seed;
  };

  constexpr size_t SEED_SIZE = LaggedFibonacciGenerator::SEED_SIZE * sizeof(u32);

  // Maps end_offset -> (start_offset, seed)
  std::map<size_t, JunkInfo> junk_info;

  size_t position = 0;
  while (position < total_size)
  {
    // Skip the 0 to 32 zero bytes that typically come after a file
    size_t zeroes = 0;
    while (position + zeroes < total_size && in[position + zeroes] == 0)
      ++zeroes;

    // If there are very many zero bytes (perhaps the PRNG junk data has been scrubbed?)
    // and we aren't using compression, it makes sense to encode the zero bytes as junk.
    // If we are using compression, the compressor will likely encode zeroes better than we can
    if (!compression && zeroes > SEED_SIZE)
      junk_info.emplace(position + zeroes, JunkInfo{position, {}});

    position += zeroes;
    data_offset += zeroes;

    const size_t bytes_to_read =
        std::min(Common::AlignUp(data_offset + 1, VolumeWii::BLOCK_TOTAL_SIZE) - data_offset,
                 total_size - position);

    const size_t data_offset_mod = static_cast<size_t>(data_offset % VolumeWii::BLOCK_TOTAL_SIZE);

    Seed seed;
    const size_t bytes_reconstructed = LaggedFibonacciGenerator::GetSeed(
        in + position, bytes_to_read, data_offset_mod, seed.data());

    if (bytes_reconstructed > 0)
      junk_info.emplace(position + bytes_reconstructed, JunkInfo{position, seed});

    if (file_system)
    {
      const std::unique_ptr<DiscIO::FileInfo> file_info =
          file_system->FindFileInfo(data_offset + bytes_reconstructed);

      // If we're at a file and there's more space in this block after the file,
      // continue after the file instead of skipping to the next block
      if (file_info)
      {
        const u64 file_end_offset = file_info->GetOffset() + file_info->GetSize();
        if (file_end_offset < data_offset + bytes_to_read)
        {
          position += file_end_offset - data_offset;
          data_offset = file_end_offset;
          continue;
        }
      }
    }

    position += bytes_to_read;
    data_offset += bytes_to_read;
  }

  for (size_t i = 0; i < chunks; ++i)
  {
    OutputParametersEntry& entry = out[i];
    if (entry.reused_group)
      continue;

    u64 current_offset = i * bytes_per_chunk;
    const u64 end_offset = std::min(current_offset + bytes_per_chunk, total_size);

    const bool store_junk_efficiently = allow_junk_reuse || !entry.reuse_id;

    // TODO: It would be possible to support skipping RVZ packing even when the chunk size is larger
    // than 2 MiB (multipart == true), but it would be more effort than it's worth since Dolphin's
    // converter doesn't expose chunk sizes larger than 2 MiB to the user anyway
    bool first_loop_iteration = !multipart;

    while (current_offset < end_offset)
    {
      u64 next_junk_start = end_offset;
      u64 next_junk_end = end_offset;
      Seed* seed = nullptr;
      if (store_junk_efficiently && end_offset - current_offset > SEED_SIZE)
      {
        const auto next_junk_it = junk_info.upper_bound(current_offset + SEED_SIZE);
        if (next_junk_it != junk_info.end() &&
            next_junk_it->second.start_offset + SEED_SIZE < end_offset)
        {
          next_junk_start = std::max<u64>(current_offset, next_junk_it->second.start_offset);
          next_junk_end = std::min<u64>(end_offset, next_junk_it->first);
          seed = &next_junk_it->second.seed;
        }
      }

      if (first_loop_iteration)
      {
        if (next_junk_start == end_offset)
        {
          // Storing this chunk without RVZ packing would be inefficient, so store it without
          PushBack(&entry.main_data, in + current_offset, in + end_offset);
          break;
        }

        first_loop_iteration = false;
      }

      const u64 non_junk_bytes = next_junk_start - current_offset;
      if (non_junk_bytes > 0)
      {
        const u8* ptr = in + current_offset;

        PushBack(&entry.main_data, Common::swap32(static_cast<u32>(non_junk_bytes)));
        PushBack(&entry.main_data, ptr, ptr + non_junk_bytes);

        current_offset += non_junk_bytes;
        entry.rvz_packed_size += sizeof(u32) + non_junk_bytes;
      }

      const u64 junk_bytes = next_junk_end - current_offset;
      if (junk_bytes > 0)
      {
        PushBack(&entry.main_data, Common::swap32(static_cast<u32>(junk_bytes) | 0x80000000));
        PushBack(&entry.main_data, *seed);

        current_offset += junk_bytes;
        entry.rvz_packed_size += sizeof(u32) + SEED_SIZE;
      }
    }
  }
}

template <typename OutputParametersEntry>
static void RVZPack(const u8* in, OutputParametersEntry* out, u64 size, u64 data_offset,
                    bool allow_junk_reuse, bool compression, const FileSystem* file_system)
{
  RVZPack(in, out, size, 1, size, data_offset, false, allow_junk_reuse, compression, file_system);
}

template <bool RVZ>
ConversionResult<typename WIARVZFileReader<RVZ>::OutputParameters>
WIARVZFileReader<RVZ>::ProcessAndCompress(CompressThreadState* state, CompressParameters parameters,
                                          const std::vector<PartitionEntry>& partition_entries,
                                          const std::vector<DataEntry>& data_entries,
                                          const FileSystem* file_system,
                                          std::map<ReuseID, GroupEntry>* reusable_groups,
                                          std::mutex* reusable_groups_mutex,
                                          u64 chunks_per_wii_group, u64 exception_lists_per_chunk,
                                          bool compressed_exception_lists, bool compression)
{
  std::vector<OutputParametersEntry> output_entries;

  if (!parameters.data_entry->is_partition)
  {
    OutputParametersEntry& entry = output_entries.emplace_back();
    std::vector<u8>& data = parameters.data;

    if (AllSame(data))
      entry.reuse_id = ReuseID{WiiKey{}, data.size(), false, data.front()};

    if constexpr (RVZ)
    {
      RVZPack(data.data(), output_entries.data(), data.size(), parameters.data_offset, true,
              compression, file_system);
    }
    else
    {
      entry.main_data = std::move(data);
    }
  }
  else
  {
    const PartitionEntry& partition_entry = partition_entries[parameters.data_entry->index];

    auto aes_context = Common::AES::CreateContextDecrypt(partition_entry.partition_key.data());

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

    const size_t first_chunk = output_entries.size();

    const auto create_reuse_id = [&partition_entry, blocks,
                                  blocks_per_chunk](u8 value, bool encrypted, u64 block) {
      const u64 size = std::min(blocks - block, blocks_per_chunk) * VolumeWii::BLOCK_DATA_SIZE;
      return ReuseID{partition_entry.partition_key, size, encrypted, value};
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
        reuse_id = create_reuse_id(parameters.data.front(), true, i * blocks_per_chunk);

      TryReuse(reusable_groups, reusable_groups_mutex, &entry);
      if (!entry.reused_group && reuse_id)
      {
        const auto it = std::find_if(output_entries.begin(), output_entries.begin() + i,
                                     [reuse_id](const auto& e) { return e.reuse_id == reuse_id; });
        if (it != output_entries.begin() + i)
          entry.reused_group = it->reused_group;
      }
    }

    if (!std::all_of(output_entries.begin(), output_entries.end(),
                     [](const OutputParametersEntry& entry) { return entry.reused_group; }))
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
                                        state->decryption_buffer[j].data(), aes_context.get());
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

          if (output_entries[chunk_index].reused_group)
            continue;

          const u64 exception_list_index = chunks_per_wii_group == 1 ? i : chunk_index;

          const u64 offset_of_block = offset_of_group + j * VolumeWii::BLOCK_TOTAL_SIZE;
          const u64 hash_offset_of_block = block_index_in_chunk * VolumeWii::BLOCK_HEADER_SIZE;

          VolumeWii::HashBlock hashes;
          VolumeWii::DecryptBlockHashes(parameters.data.data() + offset_of_block, &hashes,
                                        aes_context.get());

          const auto compare_hash = [&](size_t offset_in_block) {
            ASSERT(offset_in_block + Common::SHA1::DIGEST_LEN <= VolumeWii::BLOCK_HEADER_SIZE);

            const u8* desired_hash = reinterpret_cast<u8*>(&hashes) + offset_in_block;
            const u8* computed_hash =
                reinterpret_cast<u8*>(&state->hash_buffer[j]) + offset_in_block;

            // We want to store a hash exception either if there is a hash mismatch, or if this
            // chunk might get reused in a context where it is paired up (within a 2 MiB Wii group)
            // with chunks that are different from the chunks it currently is paired up with, since
            // that affects the recalculated hashes. Chunks which have been marked as reusable at
            // this point normally have zero matching hashes anyway, so this shouldn't waste space.
            if ((chunks_per_wii_group != 1 && output_entries[chunk_index].reuse_id) ||
                !std::equal(desired_hash, desired_hash + Common::SHA1::DIGEST_LEN, computed_hash))
            {
              const u64 hash_offset = hash_offset_of_block + offset_in_block;
              ASSERT(hash_offset <= std::numeric_limits<u16>::max());

              HashExceptionEntry& exception = exception_lists[exception_list_index].emplace_back();
              exception.offset = static_cast<u16>(Common::swap16(hash_offset));
              std::memcpy(exception.hash.data(), desired_hash, Common::SHA1::DIGEST_LEN);
            }
          };

          const auto compare_hashes = [&compare_hash](size_t offset, size_t size) {
            for (size_t l = 0; l < size; l += Common::SHA1::DIGEST_LEN)
              // The std::min is to ensure that we don't go beyond the end of HashBlock with
              // padding_2, which is 32 bytes long (not divisible by SHA1::DIGEST_LEN, which is 20).
              compare_hash(offset + std::min(l, size - Common::SHA1::DIGEST_LEN));
          };

          using HashBlock = VolumeWii::HashBlock;
          compare_hashes(offsetof(HashBlock, h0), sizeof(HashBlock::h0));
          compare_hashes(offsetof(HashBlock, padding_0), sizeof(HashBlock::padding_0));
          compare_hashes(offsetof(HashBlock, h1), sizeof(HashBlock::h1));
          compare_hashes(offsetof(HashBlock, padding_1), sizeof(HashBlock::padding_1));
          compare_hashes(offsetof(HashBlock, h2), sizeof(HashBlock::h2));
          compare_hashes(offsetof(HashBlock, padding_2), sizeof(HashBlock::padding_2));
        }

        static_assert(std::is_trivially_copyable_v<
                      typename decltype(CompressThreadState::decryption_buffer)::value_type>);
        if constexpr (RVZ)
        {
          // We must not store junk efficiently for chunks that may get reused at a position
          // which has a different value of data_offset % VolumeWii::BLOCK_TOTAL_SIZE
          const bool allow_junk_reuse = chunks_per_wii_group == 1;

          const u64 bytes_per_chunk = std::min(out_data_per_chunk, VolumeWii::GROUP_DATA_SIZE);
          const u64 total_size = blocks_in_this_group * VolumeWii::BLOCK_DATA_SIZE;
          const u64 data_offset = parameters.data_offset + write_offset_of_group;

          RVZPack(state->decryption_buffer[0].data(), output_entries.data() + first_chunk,
                  bytes_per_chunk, chunks, total_size, data_offset, groups > 1, allow_junk_reuse,
                  compression, file_system);
        }
        else
        {
          const u8* in_ptr = state->decryption_buffer[0].data();
          for (u64 j = 0; j < chunks; ++j)
          {
            OutputParametersEntry& entry = output_entries[first_chunk + j];

            if (!entry.reused_group)
            {
              const u64 bytes_left = (blocks - j * blocks_per_chunk) * VolumeWii::BLOCK_DATA_SIZE;
              const u64 bytes_to_write_total = std::min(out_data_per_chunk, bytes_left);

              if (i == 0)
                entry.main_data.resize(bytes_to_write_total);

              const u64 bytes_to_write = std::min(bytes_to_write_total, VolumeWii::GROUP_DATA_SIZE);

              std::memcpy(entry.main_data.data() + write_offset_of_group, in_ptr, bytes_to_write);

              // Set this chunk as reusable if the decrypted data is AllSame.
              // There is also a requirement that it lacks exceptions, but this is checked later
              if (i == 0 && !entry.reuse_id)
              {
                if (AllSame(in_ptr, in_ptr + bytes_to_write))
                  entry.reuse_id = create_reuse_id(*in_ptr, false, j * blocks_per_chunk);
              }
              else
              {
                if (entry.reuse_id && !entry.reuse_id->encrypted &&
                    (!AllSame(in_ptr, in_ptr + bytes_to_write) || entry.reuse_id->value != *in_ptr))
                {
                  entry.reuse_id.reset();
                }
              }
            }

            in_ptr += out_data_per_chunk;
          }
        }
      }

      for (size_t i = 0; i < exception_lists.size(); ++i)
      {
        OutputParametersEntry& entry = output_entries[chunks_per_wii_group == 1 ? 0 : i];
        if (entry.reused_group)
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

        // If this chunk was set as reusable because the decrypted data is AllSame,
        // but it has exceptions, unmark it as reusable
        if (entry.reuse_id && !entry.reuse_id->encrypted && !AllZero(entry.exception_lists))
          entry.reuse_id.reset();
      }
    }
  }

  for (OutputParametersEntry& entry : output_entries)
  {
    TryReuse(reusable_groups, reusable_groups_mutex, &entry);
    if (entry.reused_group)
      continue;

    // Special case - a compressed size of zero is treated by WIA as meaning the data is all zeroes
    if (entry.reuse_id && !entry.reuse_id->encrypted && entry.reuse_id->value == 0)
    {
      entry.exception_lists.clear();
      entry.main_data.clear();
      if constexpr (RVZ)
      {
        entry.rvz_packed_size = 0;
        entry.compressed = false;
      }
      continue;
    }

    const auto pad_exception_lists = [&entry]() {
      while (entry.exception_lists.size() % 4 != 0)
        entry.exception_lists.push_back(0);
    };

    if (state->compressor)
    {
      if (!state->compressor->Start(entry.exception_lists.size() + entry.main_data.size()))
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
      }
      else
      {
        if (!compressed_exception_lists)
          pad_exception_lists();

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

    bool compressed = !!state->compressor;
    if constexpr (RVZ)
    {
      size_t uncompressed_size = entry.main_data.size();
      if (compressed_exception_lists)
        uncompressed_size += Common::AlignUp(entry.exception_lists.size(), 4);

      compressed = state->compressor && state->compressor->GetSize() < uncompressed_size;
      entry.compressed = compressed;

      if (!compressed)
        pad_exception_lists();
    }

    if (compressed)
    {
      const u8* data = state->compressor->GetData();
      const size_t size = state->compressor->GetSize();

      entry.main_data.resize(size);
      std::copy(data, data + size, entry.main_data.data());

      if (compressed_exception_lists)
        entry.exception_lists.clear();
    }
  }

  return OutputParameters{std::move(output_entries), parameters.bytes_read, parameters.group_index};
}

template <bool RVZ>
ConversionResultCode WIARVZFileReader<RVZ>::Output(std::vector<OutputParametersEntry>* entries,
                                                   File::IOFile* outfile,
                                                   std::map<ReuseID, GroupEntry>* reusable_groups,
                                                   std::mutex* reusable_groups_mutex,
                                                   GroupEntry* group_entry, u64* bytes_written)
{
  for (OutputParametersEntry& entry : *entries)
  {
    TryReuse(reusable_groups, reusable_groups_mutex, &entry);
    if (entry.reused_group)
    {
      *group_entry = *entry.reused_group;
      ++group_entry;
      continue;
    }

    if (*bytes_written >> 2 > std::numeric_limits<u32>::max())
      return ConversionResultCode::InternalError;

    ASSERT((*bytes_written & 3) == 0);
    group_entry->data_offset = Common::swap32(static_cast<u32>(*bytes_written >> 2));

    u32 data_size = static_cast<u32>(entry.exception_lists.size() + entry.main_data.size());
    if constexpr (RVZ)
    {
      data_size = (data_size & 0x7FFFFFFF) | (static_cast<u32>(entry.compressed) << 31);
      group_entry->rvz_packed_size = Common::swap32(static_cast<u32>(entry.rvz_packed_size));
    }
    group_entry->data_size = Common::swap32(data_size);

    if (!outfile->WriteArray(entry.exception_lists.data(), entry.exception_lists.size()))
      return ConversionResultCode::WriteFailed;
    if (!outfile->WriteArray(entry.main_data.data(), entry.main_data.size()))
      return ConversionResultCode::WriteFailed;

    *bytes_written += entry.exception_lists.size() + entry.main_data.size();

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

template <bool RVZ>
ConversionResultCode WIARVZFileReader<RVZ>::RunCallback(size_t groups_written, u64 bytes_read,
                                                        u64 bytes_written, u32 total_groups,
                                                        u64 iso_size, CompressCB callback)
{
  int ratio = 0;
  if (bytes_read != 0)
    ratio = static_cast<int>(100 * bytes_written / bytes_read);

  const std::string text = Common::FmtFormatT("{0} of {1} blocks. Compression ratio {2}%",
                                              groups_written, total_groups, ratio);

  const float completion = static_cast<float>(bytes_read) / iso_size;

  return callback(text, completion) ? ConversionResultCode::Success :
                                      ConversionResultCode::Canceled;
}

template <bool RVZ>
bool WIARVZFileReader<RVZ>::WriteHeader(File::IOFile* file, const u8* data, size_t size,
                                        u64 upper_bound, u64* bytes_written, u64* offset_out)
{
  // The first part of the check is to prevent this from running more than once. If *bytes_written
  // is past the upper bound, we are already at the end of the file, so we don't need to do anything
  if (*bytes_written <= upper_bound && *bytes_written + size > upper_bound)
  {
    WARN_LOG_FMT(DISCIO,
                 "Headers did not fit in the allocated space. Writing to end of file instead");
    if (!file->Seek(0, File::SeekOrigin::End))
      return false;
    *bytes_written = file->Tell();
  }

  *offset_out = *bytes_written;
  if (!file->WriteArray(data, size))
    return false;
  *bytes_written += size;
  return PadTo4(file, bytes_written);
}

template <bool RVZ>
ConversionResultCode
WIARVZFileReader<RVZ>::Convert(BlobReader* infile, const VolumeDisc* infile_volume,
                               File::IOFile* outfile, WIARVZCompressionType compression_type,
                               int compression_level, int chunk_size, CompressCB callback)
{
  ASSERT(infile->GetDataSizeType() == DataSizeType::Accurate);
  ASSERT(chunk_size > 0);

  const u64 iso_size = infile->GetDataSize();
  const u64 chunks_per_wii_group = std::max<u64>(1, VolumeWii::GROUP_TOTAL_SIZE / chunk_size);
  const u64 exception_lists_per_chunk = std::max<u64>(1, chunk_size / VolumeWii::GROUP_TOTAL_SIZE);
  const bool compressed_exception_lists = compression_type > WIARVZCompressionType::Purge;

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

  const FileSystem* non_partition_file_system =
      infile_volume ? infile_volume->GetFileSystem(PARTITION_NONE) : nullptr;
  std::vector<const FileSystem*> partition_file_systems;

  const ConversionResultCode set_up_data_entries_result = SetUpDataEntriesForWriting(
      infile_volume, chunk_size, iso_size, &total_groups, &partition_entries, &raw_data_entries,
      &data_entries, &partition_file_systems);
  if (set_up_data_entries_result != ConversionResultCode::Success)
    return set_up_data_entries_result;

  group_entries.resize(total_groups);

  const size_t partition_entries_size = partition_entries.size() * sizeof(PartitionEntry);
  const size_t raw_data_entries_size = raw_data_entries.size() * sizeof(RawDataEntry);
  const size_t group_entries_size = group_entries.size() * sizeof(GroupEntry);

  // An estimate for how much space will be taken up by headers.
  // We will reserve this much space at the beginning of the file, and if the headers don't
  // fit in that space, we will need to write them at the end of the file instead.
  const u64 headers_size_upper_bound = [&] {
    // 0x100 is added to account for compression overhead (in particular for Purge).
    u64 upper_bound = sizeof(WIAHeader1) + sizeof(WIAHeader2) + partition_entries_size +
                      raw_data_entries_size + 0x100;

    // Compared to WIA, RVZ adds an extra member to the GroupEntry struct. This added data usually
    // compresses well, so we'll assume the compression ratio for RVZ GroupEntries is 9 / 16 or
    // better. This constant is somehwat arbitrarily chosen, but no games were found that get a
    // worse compression ratio than that. There are some games that get a worse ratio than 1 / 2,
    // such as Metroid: Other M (PAL) with the default settings.
    if (RVZ && compression_type > WIARVZCompressionType::Purge)
      upper_bound += static_cast<u64>(group_entries_size) * 9 / 16;
    else
      upper_bound += group_entries_size;

    // This alignment is also somewhat arbitrary.
    return Common::AlignUp(upper_bound, VolumeWii::BLOCK_TOTAL_SIZE);
  }();

  std::vector<u8> buffer;

  buffer.resize(headers_size_upper_bound);
  outfile->WriteBytes(buffer.data(), buffer.size());
  bytes_written = headers_size_upper_bound;

  if (!infile->Read(0, header_2.disc_header.size(), header_2.disc_header.data()))
    return ConversionResultCode::ReadFailed;
  // We intentionally do not increment bytes_read here, since these bytes will be read again

  std::map<ReuseID, GroupEntry> reusable_groups;
  std::mutex reusable_groups_mutex;

  const auto set_up_compress_thread_state = [&](CompressThreadState* state) {
    SetUpCompressor(&state->compressor, compression_type, compression_level, nullptr);
    return ConversionResultCode::Success;
  };

  const auto process_and_compress = [&](CompressThreadState* state, CompressParameters parameters) {
    const DataEntry& data_entry = *parameters.data_entry;
    const FileSystem* file_system = data_entry.is_partition ?
                                        partition_file_systems[data_entry.index] :
                                        non_partition_file_system;

    const bool compression = compression_type != WIARVZCompressionType::None;

    return ProcessAndCompress(state, std::move(parameters), partition_entries, data_entries,
                              file_system, &reusable_groups, &reusable_groups_mutex,
                              chunks_per_wii_group, exception_lists_per_chunk,
                              compressed_exception_lists, compression);
  };

  const auto output = [&](OutputParameters parameters) {
    const ConversionResultCode result =
        Output(&parameters.entries, outfile, &reusable_groups, &reusable_groups_mutex,
               &group_entries[parameters.group_index], &bytes_written);

    if (result != ConversionResultCode::Success)
      return result;

    return RunCallback(parameters.group_index + parameters.entries.size(), parameters.bytes_read,
                       bytes_written, total_groups, iso_size, callback);
  };

  MultithreadedCompressor<CompressThreadState, CompressParameters, OutputParameters> mt_compressor(
      set_up_compress_thread_state, process_and_compress, output);

  for (const DataEntry& data_entry : data_entries)
  {
    u32 first_group;
    u32 last_group;

    u64 data_offset;
    u64 data_size;

    u64 data_offset_in_partition;

    if (data_entry.is_partition)
    {
      const PartitionEntry& partition_entry = partition_entries[data_entry.index];
      const PartitionDataEntry& partition_data_entry =
          partition_entry.data_entries[data_entry.partition_data_index];

      first_group = Common::swap32(partition_data_entry.group_index);
      last_group = first_group + Common::swap32(partition_data_entry.number_of_groups);

      const u32 first_sector = Common::swap32(partition_data_entry.first_sector);
      data_offset = first_sector * VolumeWii::BLOCK_TOTAL_SIZE;
      data_size =
          Common::swap32(partition_data_entry.number_of_sectors) * VolumeWii::BLOCK_TOTAL_SIZE;

      const u32 block_in_partition =
          first_sector - Common::swap32(partition_entry.data_entries[0].first_sector);
      data_offset_in_partition = block_in_partition * VolumeWii::BLOCK_DATA_SIZE;
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

      data_offset_in_partition = data_offset;
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

      mt_compressor.CompressAndWrite(CompressParameters{
          buffer, &data_entry, data_offset_in_partition, bytes_read, groups_processed});

      data_offset += bytes_to_read;
      data_size -= bytes_to_read;

      if (data_entry.is_partition)
      {
        data_offset_in_partition +=
            bytes_to_read / VolumeWii::BLOCK_TOTAL_SIZE * VolumeWii::BLOCK_DATA_SIZE;
      }
      else
      {
        data_offset_in_partition += bytes_to_read;
      }

      groups_processed += Common::AlignUp(bytes_to_read, chunk_size) / chunk_size;
    }

    ASSERT(data_size == 0);
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
  if (!outfile->Seek(sizeof(WIAHeader1) + sizeof(WIAHeader2), File::SeekOrigin::Begin))
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
  header_2.compression_level =
      static_cast<s32>(Common::swap32(static_cast<u32>(compression_level)));
  header_2.chunk_size = Common::swap32(static_cast<u32>(chunk_size));

  header_2.number_of_partition_entries = Common::swap32(static_cast<u32>(partition_entries.size()));
  header_2.partition_entry_size = Common::swap32(sizeof(PartitionEntry));
  header_2.partition_entries_offset = Common::swap64(partition_entries_offset);

  header_2.partition_entries_hash = Common::SHA1::CalculateDigest(partition_entries);

  header_2.number_of_raw_data_entries = Common::swap32(static_cast<u32>(raw_data_entries.size()));
  header_2.raw_data_entries_offset = Common::swap64(raw_data_entries_offset);
  header_2.raw_data_entries_size =
      Common::swap32(static_cast<u32>(compressed_raw_data_entries->size()));

  header_2.number_of_group_entries = Common::swap32(static_cast<u32>(group_entries.size()));
  header_2.group_entries_offset = Common::swap64(group_entries_offset);
  header_2.group_entries_size = Common::swap32(static_cast<u32>(compressed_group_entries->size()));

  header_1.magic = RVZ ? RVZ_MAGIC : WIA_MAGIC;
  header_1.version = Common::swap32(RVZ ? RVZ_VERSION : WIA_VERSION);
  header_1.version_compatible =
      Common::swap32(RVZ ? RVZ_VERSION_WRITE_COMPATIBLE : WIA_VERSION_WRITE_COMPATIBLE);
  header_1.header_2_size = Common::swap32(sizeof(WIAHeader2));
  header_1.header_2_hash =
      Common::SHA1::CalculateDigest(reinterpret_cast<const u8*>(&header_2), sizeof(header_2));
  header_1.iso_file_size = Common::swap64(infile->GetDataSize());
  header_1.wia_file_size = Common::swap64(outfile->GetSize());
  header_1.header_1_hash = Common::SHA1::CalculateDigest(reinterpret_cast<const u8*>(&header_1),
                                                         offsetof(WIAHeader1, header_1_hash));

  if (!outfile->Seek(0, File::SeekOrigin::Begin))
    return ConversionResultCode::WriteFailed;

  if (!outfile->WriteArray(&header_1, 1))
    return ConversionResultCode::WriteFailed;
  if (!outfile->WriteArray(&header_2, 1))
    return ConversionResultCode::WriteFailed;

  return ConversionResultCode::Success;
}

bool ConvertToWIAOrRVZ(BlobReader* infile, const std::string& infile_path,
                       const std::string& outfile_path, bool rvz,
                       WIARVZCompressionType compression_type, int compression_level,
                       int chunk_size, CompressCB callback)
{
  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertFmtT(
        "Failed to open the output file \"{0}\".\n"
        "Check that you have permissions to write the target folder and that the media can "
        "be written.",
        outfile_path);
    return false;
  }

  std::unique_ptr<VolumeDisc> infile_volume = CreateDisc(infile_path);

  const auto convert = rvz ? RVZFileReader::Convert : WIAFileReader::Convert;
  const ConversionResultCode result =
      convert(infile, infile_volume.get(), &outfile, compression_type, compression_level,
              chunk_size, callback);

  if (result == ConversionResultCode::ReadFailed)
    PanicAlertFmtT("Failed to read from the input file \"{0}\".", infile_path);

  if (result == ConversionResultCode::WriteFailed)
  {
    PanicAlertFmtT("Failed to write the output file \"{0}\".\n"
                   "Check that you have enough space available on the target drive.",
                   outfile_path);
  }

  if (result != ConversionResultCode::Success)
  {
    // Remove the incomplete output file
    outfile.Close();
    File::Delete(outfile_path);
  }

  return result == ConversionResultCode::Success;
}

template class WIARVZFileReader<false>;
template class WIARVZFileReader<true>;

}  // namespace DiscIO
