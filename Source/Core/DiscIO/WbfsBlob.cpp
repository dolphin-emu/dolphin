// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WbfsBlob.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/md5.h>
#include <xxhash.h>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/LaggedFibonacciGenerator.h"
#include "DiscIO/NKitHeader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
static constexpr u64 WII_SECTOR_SIZE = 0x8000;
static constexpr u64 WII_SECTOR_COUNT = 143432 * 2;
static constexpr u64 WII_DISC_HEADER_SIZE = 256;

WbfsFileReader::WbfsFileReader(File::DirectIOFile file, const std::string& path)
    : m_size(0), m_good(false), m_encryption_cache(this)
{
  if (!AddFileToList(std::move(file)))
    return;
  if (!path.empty())
    OpenAdditionalFiles(path);
  if (!ReadHeader())
    return;
  m_good = true;

  // Read disc header copy for disc_id and disc_num (needed for junk reconstruction)
  m_files[0].file.Seek(m_hd_sector_size, File::SeekOrigin::Begin);
  u8 disc_header_buf[7];
  if (m_files[0].file.Read(disc_header_buf, sizeof(disc_header_buf)))
  {
    std::memcpy(m_disc_id, disc_header_buf, 4);
    m_disc_num = disc_header_buf[6];
  }

  // Grab disc info (assume slot 0, checked in ReadHeader())
  // TODO: Multi-disc WBFS support (disc_table can hold up to 500 entries).
  // Offset would be: m_hd_sector_size + WII_DISC_HEADER_SIZE + i * m_disc_info_size
  m_wlba_table.resize(m_blocks_per_disc);
  m_files[0].file.Seek(m_hd_sector_size + WII_DISC_HEADER_SIZE, File::SeekOrigin::Begin);
  m_files[0].file.Read(Common::AsWritableU8Span(m_wlba_table));
  for (size_t i = 0; i < m_blocks_per_disc; i++)
    m_wlba_table[i] = Common::swap16(m_wlba_table[i]);

  ReadNKitHeader();
}

WbfsFileReader::~WbfsFileReader() = default;

std::unique_ptr<BlobReader> WbfsFileReader::CopyReader() const
{
  auto retval = std::unique_ptr<WbfsFileReader>(new WbfsFileReader(m_files[0].file));
  for (size_t ix = 1; ix < m_files.size(); ix++)
    retval->AddFileToList(m_files[ix].file);
  return retval;
}

u64 WbfsFileReader::GetDataSize() const
{
  if (m_has_nkit_header && m_nkit_info.digests.disc_size > 0)
    return m_nkit_info.digests.disc_size;
  return WII_SECTOR_COUNT * WII_SECTOR_SIZE;
}

void WbfsFileReader::OpenAdditionalFiles(const std::string& path)
{
  if (path.length() < 4)
    return;

  ASSERT(!m_files.empty());  // The code below gives .wbf0 for index 0, but it should be .wbfs

  while (true)
  {
    // Replace last character with index (e.g. wbfs = wbf1)
    if (m_files.size() >= 10)
      return;
    std::string current_path = path;
    current_path.back() = static_cast<char>('0' + m_files.size());
    if (!AddFileToList(File::DirectIOFile(current_path, File::AccessMode::Read)))
      return;
  }
}

bool WbfsFileReader::AddFileToList(File::DirectIOFile file)
{
  if (!file.IsOpen())
    return false;

  const u64 file_size = file.GetSize();
  m_files.emplace_back(std::move(file), m_size, file_size);
  m_size += file_size;

  return true;
}

bool WbfsFileReader::ReadHeader()
{
  // Read hd size info
  m_files[0].file.Seek(0, File::SeekOrigin::Begin);
  m_files[0].file.Read(Common::AsWritableU8Span(m_header));
  if (m_header.magic != WBFS_MAGIC)
    return false;

  m_header.hd_sector_count = Common::swap32(m_header.hd_sector_count);
  m_hd_sector_size = 1ull << m_header.hd_sector_shift;

  if (m_size != (m_header.hd_sector_count * m_hd_sector_size))
    return false;

  // Read wbfs cluster info
  m_wbfs_sector_size = 1ull << m_header.wbfs_sector_shift;

  if (m_wbfs_sector_size < WII_SECTOR_SIZE)
    return false;

  m_blocks_per_disc =
      (WII_SECTOR_COUNT * WII_SECTOR_SIZE + m_wbfs_sector_size - 1) / m_wbfs_sector_size;
  m_disc_info_size =
      Common::AlignUp(WII_DISC_HEADER_SIZE + m_blocks_per_disc * sizeof(u16), m_hd_sector_size);

  return m_header.disc_table[0] != 0;
}

bool WbfsFileReader::ReadNKitHeader()
{
  const u64 gap_type_blocks = (DL_DVD_SIZE + m_wbfs_sector_size - 1) / m_wbfs_sector_size;
  m_nkit_info = NKit::ReadHeaderFromFile(m_files[0].file, NKit::HEADER_OFFSET, gap_type_blocks);
  if (!m_nkit_info.valid)
    return false;

  m_has_nkit_header = true;
  return true;
}

bool WbfsFileReader::IsBlockStored(u64 block_index) const
{
  return block_index < m_blocks_per_disc && m_wlba_table[block_index] != 0;
}

bool WbfsFileReader::IsGapJunk(u64 block_index) const
{
  return NKit::IsGapJunk(m_nkit_info.gap_type, block_index);
}

void WbfsFileReader::GenerateJunkData(u64 disc_offset, std::span<u8> out) const
{
  LaggedFibonacciGenerator::FillJunkData(out, disc_offset, m_disc_id, m_disc_num);
}

void WbfsFileReader::ReadPartitionInfo()
{
  std::unique_ptr<VolumeDisc> volume = CreateDisc(CopyReader());
  if (!volume)
    return;

  for (const Partition& partition : volume->GetPartitions())
  {
    const auto& ticket = volume->GetTicket(partition);

    u8 data_info[8];
    if (!Read(partition.offset + 0x2B8, 8, data_info))
      continue;

    PartitionEntry entry;
    entry.data_offset = partition.offset + (static_cast<u64>(Common::swap32(data_info)) << 2);
    entry.data_size = static_cast<u64>(Common::swap32(data_info + 4)) << 2;
    entry.key = ticket.GetTitleKey();
    m_partitions.push_back(entry);
  }
}

const WbfsFileReader::PartitionEntry* WbfsFileReader::FindPartition(u64 disc_offset) const
{
  for (const auto& p : m_partitions)
  {
    if (disc_offset >= p.data_offset && disc_offset < p.data_offset + p.data_size)
      return &p;
  }
  return nullptr;
}

bool WbfsFileReader::SupportsReadWiiDecrypted(u64 offset, u64 size, u64 partition_data_offset) const
{
  if (!m_has_nkit_header)
    return false;

  const u64 block_in_partition = offset / VolumeWii::BLOCK_DATA_SIZE;
  const u64 disc_offset = partition_data_offset + block_in_partition * VolumeWii::BLOCK_TOTAL_SIZE;
  const u64 wbfs_block = disc_offset >> m_header.wbfs_sector_shift;

  return wbfs_block < m_blocks_per_disc && !IsBlockStored(wbfs_block);
}

bool WbfsFileReader::ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset,
                                      Common::AES::Context* aes_context)
{
  std::vector<u8> encrypted(VolumeWii::BLOCK_TOTAL_SIZE);

  while (size > 0)
  {
    const u64 block_in_partition = offset / VolumeWii::BLOCK_DATA_SIZE;
    const u64 offset_in_block = offset % VolumeWii::BLOCK_DATA_SIZE;
    const u64 bytes_in_block = VolumeWii::BLOCK_DATA_SIZE - offset_in_block;
    const size_t read_size = static_cast<size_t>(std::min(bytes_in_block, size));

    const u64 disc_offset =
        partition_data_offset + block_in_partition * VolumeWii::BLOCK_TOTAL_SIZE;
    const u64 wbfs_block = disc_offset >> m_header.wbfs_sector_shift;

    if (IsBlockStored(wbfs_block))
    {
      // Stored blocks need decryption. This path is reachable even with standard 2MiB
      // WBFS sectors because partition data may not start at a sector-aligned offset,
      // causing an encryption group to straddle two WBFS blocks.
      u64 available;
      auto& file = SeekToCluster(disc_offset, &available);
      if (available < VolumeWii::BLOCK_TOTAL_SIZE ||
          !file.Read(encrypted.data(), VolumeWii::BLOCK_TOTAL_SIZE))
        return false;
      if (offset_in_block == 0 && read_size == VolumeWii::BLOCK_DATA_SIZE)
      {
        VolumeWii::DecryptBlockData(encrypted.data(), out_ptr, aes_context);
      }
      else
      {
        u8 decrypted[VolumeWii::BLOCK_DATA_SIZE];
        VolumeWii::DecryptBlockData(encrypted.data(), decrypted, aes_context);
        std::memcpy(out_ptr, decrypted + offset_in_block, read_size);
      }
    }
    else if (IsGapJunk(wbfs_block))
    {
      const u64 junk_offset = block_in_partition * VolumeWii::BLOCK_DATA_SIZE + offset_in_block;
      LaggedFibonacciGenerator::FillJunkData({out_ptr, read_size}, junk_offset, m_disc_id,
                                             m_disc_num);
    }
    else
    {
      std::memset(out_ptr, 0, read_size);
    }

    out_ptr += read_size;
    offset += read_size;
    size -= read_size;
  }
  return true;
}

bool WbfsFileReader::Read(u64 offset, u64 nbytes, u8* out_ptr)
{
  if (offset + nbytes > GetDataSize())
    return false;

  while (nbytes)
  {
    const u64 base_cluster = offset >> m_header.wbfs_sector_shift;
    const u64 cluster_offset = offset & (m_wbfs_sector_size - 1);
    const u64 bytes_in_cluster = m_wbfs_sector_size - cluster_offset;
    u64 read_size = std::min(bytes_in_cluster, nbytes);

    if (base_cluster >= m_blocks_per_disc || !IsBlockStored(base_cluster))
    {
      if (m_has_nkit_header && base_cluster < m_blocks_per_disc)
      {
        const PartitionEntry* pe = FindPartition(offset);
        if (pe)
        {
          // Non-stored partition sectors need re-encryption via WiiEncryptionCache
          const u64 partition_offset = offset - pe->data_offset;
          const u64 partition_data_decrypted_size =
              pe->data_size / VolumeWii::BLOCK_TOTAL_SIZE * VolumeWii::BLOCK_DATA_SIZE;
          if (!m_encryption_cache.EncryptGroups(partition_offset, read_size, out_ptr,
                                                pe->data_offset, partition_data_decrypted_size,
                                                pe->key))
          {
            std::memset(out_ptr, 0, static_cast<size_t>(read_size));
          }
        }
        else if (IsGapJunk(base_cluster))
        {
          GenerateJunkData(offset, {out_ptr, read_size});
        }
        else
        {
          std::memset(out_ptr, 0, static_cast<size_t>(read_size));
        }
      }
      else
      {
        std::memset(out_ptr, 0, static_cast<size_t>(read_size));
      }
    }
    else
    {
      u64 available;
      auto& data_file = SeekToCluster(offset, &available);
      if (available == 0)
        return false;
      read_size = std::min(read_size, available);
      if (!data_file.Read(out_ptr, read_size))
        return false;
    }

    out_ptr += read_size;
    nbytes -= read_size;
    offset += read_size;
  }

  return true;
}

File::DirectIOFile& WbfsFileReader::SeekToCluster(u64 offset, u64* available)
{
  u64 base_cluster = (offset >> m_header.wbfs_sector_shift);
  if (base_cluster < m_blocks_per_disc)
  {
    u64 cluster_address = m_wbfs_sector_size * m_wlba_table[base_cluster];
    u64 cluster_offset = offset & (m_wbfs_sector_size - 1);
    u64 final_address = cluster_address + cluster_offset;

    for (FileEntry& file_entry : m_files)
    {
      if (final_address < (file_entry.base_address + file_entry.size))
      {
        file_entry.file.Seek(final_address - file_entry.base_address, File::SeekOrigin::Begin);
        if (available)
        {
          u64 till_end_of_file = file_entry.size - (final_address - file_entry.base_address);
          u64 till_end_of_sector = m_wbfs_sector_size - cluster_offset;
          *available = std::min(till_end_of_file, till_end_of_sector);
        }

        return file_entry.file;
      }
    }
  }

  ERROR_LOG_FMT(DISCIO, "Read beyond end of disc");
  if (available)
    *available = 0;
  m_files[0].file.Seek(0, File::SeekOrigin::Begin);
  return m_files[0].file;
}

std::unique_ptr<WbfsFileReader> WbfsFileReader::Create(File::DirectIOFile file,
                                                       const std::string& path)
{
  auto reader = std::unique_ptr<WbfsFileReader>(new WbfsFileReader(std::move(file), path));

  if (!reader->IsGood())
    reader.reset();

  if (reader && reader->m_has_nkit_header)
    reader->ReadPartitionInfo();

  return reader;
}


}  // namespace DiscIO
