// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/VolumeWii.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Enums.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiSaveBanner.h"

namespace DiscIO
{
VolumeWii::VolumeWii(std::unique_ptr<BlobReader> reader)
    : m_reader(std::move(reader)), m_game_partition(PARTITION_NONE),
      m_last_decrypted_block(UINT64_MAX)
{
  ASSERT(m_reader);

  m_encrypted = m_reader->ReadSwapped<u32>(0x60) == u32(0);

  for (u32 partition_group = 0; partition_group < 4; ++partition_group)
  {
    const std::optional<u32> number_of_partitions =
        m_reader->ReadSwapped<u32>(0x40000 + (partition_group * 8));
    if (!number_of_partitions)
      continue;

    const std::optional<u64> partition_table_offset =
        ReadSwappedAndShifted(0x40000 + (partition_group * 8) + 4, PARTITION_NONE);
    if (!partition_table_offset)
      continue;

    for (u32 i = 0; i < number_of_partitions; i++)
    {
      const std::optional<u64> partition_offset =
          ReadSwappedAndShifted(*partition_table_offset + (i * 8), PARTITION_NONE);
      if (!partition_offset)
        continue;

      const Partition partition(*partition_offset);

      const std::optional<u32> partition_type =
          m_reader->ReadSwapped<u32>(*partition_table_offset + (i * 8) + 4);
      if (!partition_type)
        continue;

      // If this is the game partition, set m_game_partition
      if (m_game_partition == PARTITION_NONE && *partition_type == 0)
        m_game_partition = partition;

      auto get_ticket = [this, partition]() -> IOS::ES::TicketReader {
        std::vector<u8> ticket_buffer(sizeof(IOS::ES::Ticket));
        if (!m_reader->Read(partition.offset, ticket_buffer.size(), ticket_buffer.data()))
          return INVALID_TICKET;
        return IOS::ES::TicketReader{std::move(ticket_buffer)};
      };

      auto get_tmd = [this, partition]() -> IOS::ES::TMDReader {
        const std::optional<u32> tmd_size = m_reader->ReadSwapped<u32>(partition.offset + 0x2a4);
        const std::optional<u64> tmd_address =
            ReadSwappedAndShifted(partition.offset + 0x2a8, PARTITION_NONE);
        if (!tmd_size || !tmd_address)
          return INVALID_TMD;
        if (!IOS::ES::IsValidTMDSize(*tmd_size))
        {
          // This check is normally done by ES in ES_DiVerify, but that would happen too late
          // (after allocating the buffer), so we do the check here.
          PanicAlert("Invalid TMD size");
          return INVALID_TMD;
        }
        std::vector<u8> tmd_buffer(*tmd_size);
        if (!m_reader->Read(partition.offset + *tmd_address, *tmd_size, tmd_buffer.data()))
          return INVALID_TMD;
        return IOS::ES::TMDReader{std::move(tmd_buffer)};
      };

      auto get_key = [this, partition]() -> std::unique_ptr<mbedtls_aes_context> {
        const IOS::ES::TicketReader& ticket = *m_partitions[partition].ticket;
        if (!ticket.IsValid())
          return nullptr;
        const std::array<u8, 16> key = ticket.GetTitleKey();
        std::unique_ptr<mbedtls_aes_context> aes_context = std::make_unique<mbedtls_aes_context>();
        mbedtls_aes_setkey_dec(aes_context.get(), key.data(), 128);
        return aes_context;
      };

      auto get_file_system = [this, partition]() -> std::unique_ptr<FileSystem> {
        auto file_system = std::make_unique<FileSystemGCWii>(this, partition);
        return file_system->IsValid() ? std::move(file_system) : nullptr;
      };

      auto get_data_offset = [this, partition]() -> u64 {
        return ReadSwappedAndShifted(partition.offset + 0x2b8, PARTITION_NONE).value_or(0);
      };

      m_partitions.emplace(
          partition, PartitionDetails{Common::Lazy<std::unique_ptr<mbedtls_aes_context>>(get_key),
                                      Common::Lazy<IOS::ES::TicketReader>(get_ticket),
                                      Common::Lazy<IOS::ES::TMDReader>(get_tmd),
                                      Common::Lazy<std::unique_ptr<FileSystem>>(get_file_system),
                                      Common::Lazy<u64>(get_data_offset), *partition_type});
    }
  }
}

VolumeWii::~VolumeWii()
{
}

bool VolumeWii::Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const
{
  if (partition == PARTITION_NONE)
    return m_reader->Read(offset, length, buffer);

  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;

  if (!m_encrypted)
  {
    return m_reader->Read(partition.offset + *partition_details.data_offset + offset, length,
                          buffer);
  }

  if (m_reader->SupportsReadWiiDecrypted())
    return m_reader->ReadWiiDecrypted(offset, length, buffer, partition.offset);

  mbedtls_aes_context* aes_context = partition_details.key->get();
  if (!aes_context)
    return false;

  std::vector<u8> read_buffer(BLOCK_TOTAL_SIZE);
  while (length > 0)
  {
    // Calculate offsets
    u64 block_offset_on_disc = partition.offset + *partition_details.data_offset +
                               offset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE;
    u64 data_offset_in_block = offset % BLOCK_DATA_SIZE;

    if (m_last_decrypted_block != block_offset_on_disc)
    {
      // Read the current block
      if (!m_reader->Read(block_offset_on_disc, BLOCK_TOTAL_SIZE, read_buffer.data()))
        return false;

      // Decrypt the block's data.
      // 0x3D0 - 0x3DF in read_buffer will be overwritten,
      // but that won't affect anything, because we won't
      // use the content of read_buffer anymore after this
      mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, BLOCK_DATA_SIZE, &read_buffer[0x3D0],
                            &read_buffer[BLOCK_HEADER_SIZE], m_last_decrypted_block_data);
      m_last_decrypted_block = block_offset_on_disc;

      // The only thing we currently use from the 0x000 - 0x3FF part
      // of the block is the IV (at 0x3D0), but it also contains SHA-1
      // hashes that IOS uses to check that discs aren't tampered with.
      // http://wiibrew.org/wiki/Wii_Disc#Encrypted
    }

    // Copy the decrypted data
    u64 copy_size = std::min(length, BLOCK_DATA_SIZE - data_offset_in_block);
    memcpy(buffer, &m_last_decrypted_block_data[data_offset_in_block],
           static_cast<size_t>(copy_size));

    // Update offsets
    length -= copy_size;
    buffer += copy_size;
    offset += copy_size;
  }

  return true;
}

bool VolumeWii::IsEncryptedAndHashed() const
{
  return m_encrypted;
}

std::vector<Partition> VolumeWii::GetPartitions() const
{
  std::vector<Partition> partitions;
  for (const auto& pair : m_partitions)
    partitions.push_back(pair.first);
  return partitions;
}

Partition VolumeWii::GetGamePartition() const
{
  return m_game_partition;
}

std::optional<u32> VolumeWii::GetPartitionType(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? it->second.type : std::optional<u32>();
}

std::optional<u64> VolumeWii::GetTitleID(const Partition& partition) const
{
  const IOS::ES::TicketReader& ticket = GetTicket(partition);
  if (!ticket.IsValid())
    return {};
  return ticket.GetTitleId();
}

const IOS::ES::TicketReader& VolumeWii::GetTicket(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? *it->second.ticket : INVALID_TICKET;
}

const IOS::ES::TMDReader& VolumeWii::GetTMD(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? *it->second.tmd : INVALID_TMD;
}

const FileSystem* VolumeWii::GetFileSystem(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? it->second.file_system->get() : nullptr;
}

u64 VolumeWii::EncryptedPartitionOffsetToRawOffset(u64 offset, const Partition& partition,
                                                   u64 partition_data_offset)
{
  if (partition == PARTITION_NONE)
    return offset;

  return partition.offset + partition_data_offset + (offset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE) +
         (offset % BLOCK_DATA_SIZE);
}

u64 VolumeWii::PartitionOffsetToRawOffset(u64 offset, const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return offset;
  const u64 data_offset = *it->second.data_offset;

  if (!m_encrypted)
    return partition.offset + data_offset + offset;

  return EncryptedPartitionOffsetToRawOffset(offset, partition, data_offset);
}

std::string VolumeWii::GetGameID(const Partition& partition) const
{
  char id[6];

  if (!Read(0, sizeof(id), reinterpret_cast<u8*>(id), partition))
    return std::string();

  return DecodeString(id);
}

Region VolumeWii::GetRegion() const
{
  const std::optional<u32> region_code = m_reader->ReadSwapped<u32>(0x4E000);
  if (!region_code)
    return Region::Unknown;
  const Region region = static_cast<Region>(*region_code);
  return region <= Region::NTSC_K ? region : Region::Unknown;
}

Country VolumeWii::GetCountry(const Partition& partition) const
{
  // The 0 that we use as a default value is mapped to Country::Unknown and Region::Unknown
  const u8 country_byte = ReadSwapped<u8>(3, partition).value_or(0);
  const Region region = GetRegion();

  if (CountryCodeToRegion(country_byte, Platform::WiiDisc, region) != region)
    return TypicalCountryForRegion(region);

  return CountryCodeToCountry(country_byte, Platform::WiiDisc, region);
}

std::string VolumeWii::GetMakerID(const Partition& partition) const
{
  char maker_id[2];

  if (!Read(0x4, sizeof(maker_id), reinterpret_cast<u8*>(&maker_id), partition))
    return std::string();

  return DecodeString(maker_id);
}

std::optional<u16> VolumeWii::GetRevision(const Partition& partition) const
{
  std::optional<u8> revision = ReadSwapped<u8>(7, partition);
  return revision ? *revision : std::optional<u16>();
}

std::string VolumeWii::GetInternalName(const Partition& partition) const
{
  char name_buffer[0x60];
  if (Read(0x20, sizeof(name_buffer), reinterpret_cast<u8*>(&name_buffer), partition))
    return DecodeString(name_buffer);

  return "";
}

std::map<Language, std::string> VolumeWii::GetLongNames() const
{
  std::vector<char16_t> names(NAMES_TOTAL_CHARS);
  names.resize(ReadFile(*this, GetGamePartition(), "opening.bnr",
                        reinterpret_cast<u8*>(names.data()), NAMES_TOTAL_BYTES, 0x5C));
  return ReadWiiNames(names);
}

std::vector<u32> VolumeWii::GetBanner(u32* width, u32* height) const
{
  *width = 0;
  *height = 0;

  const std::optional<u64> title_id = GetTitleID(GetGamePartition());
  if (!title_id)
    return std::vector<u32>();

  return WiiSaveBanner(*title_id).GetBanner(width, height);
}

std::string VolumeWii::GetApploaderDate(const Partition& partition) const
{
  char date[16];

  if (!Read(0x2440, sizeof(date), reinterpret_cast<u8*>(&date), partition))
    return std::string();

  return DecodeString(date);
}

Platform VolumeWii::GetVolumeType() const
{
  return Platform::WiiDisc;
}

std::optional<u8> VolumeWii::GetDiscNumber(const Partition& partition) const
{
  return ReadSwapped<u8>(6, partition);
}

BlobType VolumeWii::GetBlobType() const
{
  return m_reader->GetBlobType();
}

u64 VolumeWii::GetSize() const
{
  return m_reader->GetDataSize();
}

u64 VolumeWii::GetRawSize() const
{
  return m_reader->GetRawSize();
}

bool VolumeWii::CheckIntegrity(const Partition& partition) const
{
  if (!m_encrypted)
    return false;

  // Get the decryption key for the partition
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;
  mbedtls_aes_context* aes_context = partition_details.key->get();
  if (!aes_context)
    return false;

  // Get partition data size
  const auto part_data_size = ReadSwappedAndShifted(partition.offset + 0x2BC, PARTITION_NONE);
  if (!part_data_size)
    return false;

  const u32 num_clusters = static_cast<u32>(part_data_size.value() / 0x8000);
  for (u32 cluster_id = 0; cluster_id < num_clusters; ++cluster_id)
  {
    const u64 cluster_offset =
        partition.offset + *partition_details.data_offset + static_cast<u64>(cluster_id) * 0x8000;

    // Read and decrypt the cluster metadata
    u8 cluster_metadata_crypted[0x400];
    u8 cluster_metadata[0x400];
    u8 iv[16] = {0};
    if (!m_reader->Read(cluster_offset, sizeof(cluster_metadata_crypted), cluster_metadata_crypted))
    {
      WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", cluster_id);
      return false;
    }
    mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, sizeof(cluster_metadata), iv,
                          cluster_metadata_crypted, cluster_metadata);

    // Some clusters have invalid data and metadata because they aren't
    // meant to be read by the game (for example, holes between files). To
    // try to avoid reporting errors because of these clusters, we check
    // the 0x00 paddings in the metadata.
    //
    // This may cause some false negatives though: some bad clusters may be
    // skipped because they are *too* bad and are not even recognized as
    // valid clusters. To be improved.
    const u8* pad_begin = cluster_metadata + 0x26C;
    const u8* pad_end = pad_begin + 0x14;
    const bool meaningless = std::any_of(pad_begin, pad_end, [](u8 val) { return val != 0; });

    if (meaningless)
      continue;

    u8 cluster_data[0x7C00];
    if (!Read(cluster_id * sizeof(cluster_data), sizeof(cluster_data), cluster_data, partition))
    {
      WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read data", cluster_id);
      return false;
    }

    for (u32 hash_id = 0; hash_id < 31; ++hash_id)
    {
      u8 hash[20];

      mbedtls_sha1(cluster_data + hash_id * sizeof(cluster_metadata), sizeof(cluster_metadata),
                   hash);

      // Note that we do not use strncmp here
      if (memcmp(hash, cluster_metadata + hash_id * sizeof(hash), sizeof(hash)))
      {
        WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: hash %d is invalid", cluster_id,
                 hash_id);
        return false;
      }
    }
  }

  return true;
}

}  // namespace
