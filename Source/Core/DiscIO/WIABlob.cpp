// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/WIABlob.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <utility>

#include <bzlib.h>
#include <lzma.h>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
WIAFileReader::WIAFileReader(File::IOFile file, const std::string& path) : m_file(std::move(file))
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
  // TODO: Check hash

  const size_t copy_length = std::min(partition_entry_size, sizeof(PartitionEntry));
  const size_t memset_length = sizeof(PartitionEntry) - copy_length;
  u8* ptr = partition_entries.data();
  m_partition_entries.resize(number_of_partition_entries);
  for (size_t i = 0; i < number_of_partition_entries; ++i, ptr += partition_entry_size)
  {
    std::memcpy(&m_partition_entries[i], ptr, copy_length);
    std::memset(reinterpret_cast<u8*>(&m_partition_entries[i]) + copy_length, 0, memset_length);
  }

  for (const PartitionEntry& partition : m_partition_entries)
  {
    if (Common::swap32(partition.data_entries[1].number_of_sectors) != 0)
    {
      const u32 first_end = Common::swap32(partition.data_entries[0].first_sector) +
                            Common::swap32(partition.data_entries[0].number_of_sectors);
      const u32 second_start = Common::swap32(partition.data_entries[1].first_sector);
      if (first_end > second_start)
        return false;
    }
  }

  std::sort(m_partition_entries.begin(), m_partition_entries.end(),
            [](const PartitionEntry& a, const PartitionEntry& b) {
              return Common::swap32(a.data_entries[0].first_sector) <
                     Common::swap32(b.data_entries[0].first_sector);
            });

  const u32 number_of_raw_data_entries = Common::swap32(m_header_2.number_of_raw_data_entries);
  m_raw_data_entries.resize(number_of_raw_data_entries);
  if (!ReadCompressedData(number_of_raw_data_entries * sizeof(RawDataEntry),
                          Common::swap64(m_header_2.raw_data_entries_offset),
                          Common::swap32(m_header_2.raw_data_entries_size),
                          reinterpret_cast<u8*>(m_raw_data_entries.data()), false))
  {
    return false;
  }

  std::sort(m_raw_data_entries.begin(), m_raw_data_entries.end(),
            [](const RawDataEntry& a, const RawDataEntry& b) {
              return Common::swap64(a.data_offset) < Common::swap64(b.data_offset);
            });

  const u32 number_of_group_entries = Common::swap32(m_header_2.number_of_group_entries);
  m_group_entries.resize(number_of_group_entries);
  if (!ReadCompressedData(number_of_group_entries * sizeof(GroupEntry),
                          Common::swap64(m_header_2.group_entries_offset),
                          Common::swap32(m_header_2.group_entries_size),
                          reinterpret_cast<u8*>(m_group_entries.data()), false))
  {
    return false;
  }

  return true;
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
  for (RawDataEntry raw_data : m_raw_data_entries)
  {
    if (size == 0)
      return true;

    if (!ReadFromGroups(&offset, &size, &out_ptr, chunk_size, VolumeWii::BLOCK_TOTAL_SIZE,
                        Common::swap64(raw_data.data_offset), Common::swap64(raw_data.data_size),
                        Common::swap32(raw_data.group_index),
                        Common::swap32(raw_data.number_of_groups), false))
    {
      return false;
    }
  }

  return size == 0;
}

bool WIAFileReader::SupportsReadWiiDecrypted() const
{
  return !m_partition_entries.empty();
}

bool WIAFileReader::ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset)
{
  const u64 chunk_size = Common::swap32(m_header_2.chunk_size) * VolumeWii::BLOCK_DATA_SIZE /
                         VolumeWii::BLOCK_TOTAL_SIZE;
  for (const PartitionEntry& partition : m_partition_entries)
  {
    const u32 partition_first_sector = Common::swap32(partition.data_entries[0].first_sector);
    if (partition_data_offset != partition_first_sector * VolumeWii::BLOCK_TOTAL_SIZE)
      continue;

    for (const PartitionDataEntry& data : partition.data_entries)
    {
      if (size == 0)
        return true;

      const u64 data_offset =
          (Common::swap32(data.first_sector) - partition_first_sector) * VolumeWii::BLOCK_DATA_SIZE;
      const u64 data_size = Common::swap32(data.number_of_sectors) * VolumeWii::BLOCK_DATA_SIZE;

      if (!ReadFromGroups(&offset, &size, &out_ptr, chunk_size, VolumeWii::BLOCK_DATA_SIZE,
                          data_offset, data_size, Common::swap32(data.group_index),
                          Common::swap32(data.number_of_groups), true))
      {
        return false;
      }
    }

    return size == 0;
  }

  return false;
}

bool WIAFileReader::ReadFromGroups(u64* offset, u64* size, u8** out_ptr, u64 chunk_size,
                                   u32 sector_size, u64 data_offset, u64 data_size, u32 group_index,
                                   u32 number_of_groups, bool exception_list)
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
      if (!ReadCompressedData(chunk_size, group_offset_in_file, group_data_size, offset_in_group,
                              bytes_to_read, *out_ptr, exception_list))
      {
        return false;
      }
    }

    *offset += bytes_to_read;
    *size -= bytes_to_read;
    *out_ptr += bytes_to_read;
  }

  return true;
}

bool WIAFileReader::ReadCompressedData(u32 decompressed_data_size, u64 data_offset, u64 data_size,
                                       u8* out_ptr, bool exception_list)
{
  switch (m_compression_type)
  {
  case CompressionType::None:
  {
    return ReadCompressedData(decompressed_data_size, data_offset, data_size, 0,
                              decompressed_data_size, out_ptr, exception_list);
  }

  case CompressionType::Purge:
  {
    if (!m_file.Seek(data_offset, SEEK_SET))
      return false;

    if (exception_list)
    {
      const std::optional<u64> exception_size = ReadExceptionListFromFile();
      if (!exception_size)
        return false;

      data_size -= *exception_size;
    }

    const u64 hash_offset = data_size - sizeof(SHA1);
    u32 offset_in_data = 0;
    u32 offset_in_decompressed_data = 0;

    while (offset_in_data < hash_offset)
    {
      PurgeSegment purge_segment;
      if (!m_file.ReadArray(&purge_segment, 1))
        return false;

      const u32 segment_offset = Common::swap32(purge_segment.offset);
      const u32 segment_size = Common::swap32(purge_segment.size);

      if (segment_offset < offset_in_decompressed_data)
        return false;

      const u32 blank_bytes = segment_offset - offset_in_decompressed_data;
      std::memset(out_ptr, 0, blank_bytes);
      out_ptr += blank_bytes;

      if (segment_size != 0 && !m_file.ReadBytes(out_ptr, segment_size))
        return false;
      out_ptr += segment_size;

      offset_in_data += sizeof(PurgeSegment) + segment_size;
      offset_in_decompressed_data = segment_offset + segment_size;
    }

    if (offset_in_data != hash_offset || offset_in_decompressed_data > decompressed_data_size)
      return false;

    std::memset(out_ptr, 0, decompressed_data_size - offset_in_decompressed_data);

    SHA1 expected_hash;
    if (!m_file.ReadArray(&expected_hash, 1))
      return false;

    // TODO: Check hash

    return true;
  }

  case CompressionType::Bzip2:
  case CompressionType::LZMA:
  case CompressionType::LZMA2:
  {
    std::vector<u8> compressed_data(data_size);
    if (!m_file.Seek(data_offset, SEEK_SET) || !m_file.ReadBytes(compressed_data.data(), data_size))
      return false;

    std::unique_ptr<Decompressor> decompressor;
    switch (m_compression_type)
    {
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

    if (!decompressor->Start(compressed_data.data(), compressed_data.size()))
      return false;

    if (exception_list)
    {
      u16 exceptions;
      if (decompressor->Read(reinterpret_cast<u8*>(&exceptions), sizeof(exceptions)) !=
          sizeof(exceptions))
      {
        return false;
      }

      std::vector<HashExceptionEntry> exception_entries(Common::swap16(exceptions));
      u8* exceptions_data = reinterpret_cast<u8*>(exception_entries.data());
      const size_t exceptions_size = exception_entries.size() * sizeof(HashExceptionEntry);
      if (decompressor->Read(exceptions_data, exceptions_size) != exceptions_size)
        return false;

      // TODO: Actually handle the exceptions
    }

    if (decompressor->Read(out_ptr, decompressed_data_size) != decompressed_data_size)
      return false;

    if (!decompressor->DoneReading())
      return false;

    return true;
  }
  }

  return false;
}

bool WIAFileReader::ReadCompressedData(u32 decompressed_data_size, u64 data_offset, u64 data_size,
                                       u64 offset_in_data, u64 size_in_data, u8* out_ptr,
                                       bool exception_list)
{
  if (m_compression_type == CompressionType::None)
  {
    if (!m_file.Seek(data_offset, SEEK_SET))
      return false;

    if (exception_list)
    {
      const std::optional<u64> exception_list_size = ReadExceptionListFromFile();
      if (!exception_list_size)
        return false;

      data_size -= *exception_list_size;
    }

    if (!m_file.Seek(offset_in_data, SEEK_CUR) || !m_file.ReadBytes(out_ptr, size_in_data))
      return false;

    return true;
  }
  else
  {
    // TODO: Caching
    std::vector<u8> buffer(decompressed_data_size);
    if (!ReadCompressedData(decompressed_data_size, data_offset, data_size, buffer.data(),
                            exception_list))
    {
      return false;
    }
    std::memcpy(out_ptr, buffer.data() + offset_in_data, size_in_data);
    return true;
  }
}

std::optional<u64> WIAFileReader::ReadExceptionListFromFile()
{
  u16 exceptions;
  if (!m_file.ReadArray(&exceptions, 1))
    return std::nullopt;

  const u64 exception_list_size = Common::AlignUp(
      sizeof(exceptions) + Common::swap16(exceptions) * sizeof(HashExceptionEntry), 4);

  if (!m_file.Seek(exception_list_size - sizeof(exceptions), SEEK_CUR))
    return std::nullopt;

  // TODO: Actually handle the exceptions

  return exception_list_size;
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

WIAFileReader::Bzip2Decompressor::~Bzip2Decompressor()
{
  End();
}

bool WIAFileReader::Bzip2Decompressor::Start(const u8* in_ptr, u64 size)
{
  if (m_started)
    return false;

  m_stream.bzalloc = nullptr;
  m_stream.bzfree = nullptr;
  m_stream.opaque = nullptr;

  m_started = BZ2_bzDecompressInit(&m_stream, 0, 0) == BZ_OK;

  m_stream.next_in = reinterpret_cast<char*>(const_cast<u8*>(in_ptr));
  m_stream.avail_in = size;

  return m_started;
}

u64 WIAFileReader::Bzip2Decompressor::Read(u8* out_ptr, u64 size)
{
  if (!m_started || m_error_occurred || m_stream.avail_in == 0)
    return 0;

  m_stream.next_out = reinterpret_cast<char*>(out_ptr);
  m_stream.avail_out = size;

  const int result = BZ2_bzDecompress(&m_stream);
  m_error_occurred = result != BZ_OK && result != BZ_STREAM_END;

  return m_error_occurred ? 0 : m_stream.next_out - reinterpret_cast<char*>(out_ptr);
}

bool WIAFileReader::Bzip2Decompressor::DoneReading() const
{
  return m_started && !m_error_occurred && m_stream.avail_in == 0;
}

void WIAFileReader::Bzip2Decompressor::End()
{
  if (m_started && !m_ended)
  {
    BZ2_bzDecompressEnd(&m_stream);
    m_ended = true;
  }
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
  End();
}

bool WIAFileReader::LZMADecompressor::Start(const u8* in_ptr, u64 size)
{
  if (m_started || m_error_occurred)
    return false;

  m_started = lzma_raw_decoder(&m_stream, m_filters) == LZMA_OK;

  m_stream.next_in = in_ptr;
  m_stream.avail_in = size;

  return m_started;
}

u64 WIAFileReader::LZMADecompressor::Read(u8* out_ptr, u64 size)
{
  if (!m_started || m_error_occurred || m_stream.avail_in == 0)
    return 0;

  m_stream.next_out = out_ptr;
  m_stream.avail_out = size;

  const lzma_ret result = lzma_code(&m_stream, LZMA_RUN);
  m_error_occurred = result != LZMA_OK && result != LZMA_STREAM_END;

  return m_error_occurred ? 0 : m_stream.next_out - out_ptr;
}

bool WIAFileReader::LZMADecompressor::DoneReading() const
{
  return m_started && !m_error_occurred && m_stream.avail_in == 0;
}

void WIAFileReader::LZMADecompressor::End()
{
  if (m_started && !m_ended)
  {
    lzma_end(&m_stream);
    m_ended = true;
  }
}

}  // namespace DiscIO
