// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/WIABlob.h"

#include <algorithm>
#include <cstring>
#include <memory>

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
  if (compression_type != 0)
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

  // TODO: Compression
  const u32 number_of_raw_data_entries = Common::swap32(m_header_2.number_of_raw_data_entries);
  m_raw_data_entries.resize(number_of_raw_data_entries);
  if (!m_file.Seek(Common::swap64(m_header_2.raw_data_entries_offset), SEEK_SET))
    return false;
  if (!m_file.ReadArray(m_raw_data_entries.data(), number_of_raw_data_entries))
    return false;

  std::sort(m_raw_data_entries.begin(), m_raw_data_entries.end(),
            [](const RawDataEntry& a, const RawDataEntry& b) {
              return Common::swap64(a.data_offset) < Common::swap64(b.data_offset);
            });

  // TODO: Compression
  const u32 number_of_group_entries = Common::swap32(m_header_2.number_of_group_entries);
  m_group_entries.resize(number_of_group_entries);
  if (!m_file.Seek(Common::swap64(m_header_2.group_entries_offset), SEEK_SET))
    return false;
  if (!m_file.ReadArray(m_group_entries.data(), number_of_group_entries))
    return false;

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

    u64 data_offset = Common::swap64(raw_data.data_offset);
    u64 data_size = Common::swap64(raw_data.data_size);

    if (data_offset + data_size <= offset)
      continue;

    if (offset < data_offset)
      return false;

    const u64 skipped_data = data_offset % VolumeWii::BLOCK_TOTAL_SIZE;
    data_offset -= skipped_data;
    data_size += skipped_data;

    const u32 number_of_groups = Common::swap32(raw_data.number_of_groups);
    const u64 group_index = (offset - data_offset) / chunk_size;
    for (u64 i = group_index; i < number_of_groups && size > 0; ++i)
    {
      const u64 total_group_index = Common::swap32(raw_data.group_index) + i;
      if (total_group_index >= m_group_entries.size())
        return false;

      const GroupEntry group = m_group_entries[total_group_index];
      const u64 group_offset_on_disc = data_offset + i * chunk_size;
      const u64 offset_in_group = offset - group_offset_on_disc;

      // TODO: Compression

      const u64 group_offset_in_file = static_cast<u64>(Common::swap32(group.data_offset)) << 2;
      const u64 offset_in_file = group_offset_in_file + offset_in_group;
      const u64 bytes_to_read = std::min(chunk_size - offset_in_group, size);
      if (!m_file.Seek(offset_in_file, SEEK_SET) || !m_file.ReadBytes(out_ptr, bytes_to_read))
        return false;

      offset += bytes_to_read;
      size -= bytes_to_read;
      out_ptr += bytes_to_read;
    }
  }

  return size == 0;
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
}  // namespace DiscIO
