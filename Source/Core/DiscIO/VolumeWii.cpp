// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeWii.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/Crypto/SHA1.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/DiscExtractor.h"
#include "DiscIO/DiscUtils.h"
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

  m_has_hashes = m_reader->ReadSwapped<u8>(0x60) == u8(0);
  m_has_encryption = m_reader->ReadSwapped<u8>(0x61) == u8(0);

  if (m_has_encryption && !m_has_hashes)
    ERROR_LOG_FMT(DISCIO, "Wii disc has encryption but no hashes! This probably won't work well");

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
        const std::optional<u32> tmd_size =
            m_reader->ReadSwapped<u32>(partition.offset + WII_PARTITION_TMD_SIZE_ADDRESS);
        const std::optional<u64> tmd_address = ReadSwappedAndShifted(
            partition.offset + WII_PARTITION_TMD_OFFSET_ADDRESS, PARTITION_NONE);
        if (!tmd_size || !tmd_address)
          return INVALID_TMD;
        if (!IOS::ES::IsValidTMDSize(*tmd_size))
        {
          // This check is normally done by ES in ES_DiVerify, but that would happen too late
          // (after allocating the buffer), so we do the check here.
          ERROR_LOG_FMT(DISCIO, "Invalid TMD size");
          return INVALID_TMD;
        }
        std::vector<u8> tmd_buffer(*tmd_size);
        if (!m_reader->Read(partition.offset + *tmd_address, *tmd_size, tmd_buffer.data()))
          return INVALID_TMD;
        return IOS::ES::TMDReader{std::move(tmd_buffer)};
      };

      auto get_cert_chain = [this, partition]() -> std::vector<u8> {
        const std::optional<u32> size =
            m_reader->ReadSwapped<u32>(partition.offset + WII_PARTITION_CERT_CHAIN_SIZE_ADDRESS);
        const std::optional<u64> address = ReadSwappedAndShifted(
            partition.offset + WII_PARTITION_CERT_CHAIN_OFFSET_ADDRESS, PARTITION_NONE);
        if (!size || !address)
          return {};
        std::vector<u8> cert_chain(*size);
        if (!m_reader->Read(partition.offset + *address, *size, cert_chain.data()))
          return {};
        return cert_chain;
      };

      auto get_h3_table = [this, partition]() -> std::vector<u8> {
        if (!m_has_hashes)
          return {};
        const std::optional<u64> h3_table_offset = ReadSwappedAndShifted(
            partition.offset + WII_PARTITION_H3_OFFSET_ADDRESS, PARTITION_NONE);
        if (!h3_table_offset)
          return {};
        std::vector<u8> h3_table(WII_PARTITION_H3_SIZE);
        if (!m_reader->Read(partition.offset + *h3_table_offset, WII_PARTITION_H3_SIZE,
                            h3_table.data()))
          return {};
        return h3_table;
      };

      auto get_key = [this, partition]() -> std::unique_ptr<Common::AES::Context> {
        const IOS::ES::TicketReader& ticket = *m_partitions[partition].ticket;
        if (!ticket.IsValid())
          return nullptr;
        return Common::AES::CreateContextDecrypt(ticket.GetTitleKey().data());
      };

      auto get_file_system = [this, partition]() -> std::unique_ptr<FileSystem> {
        auto file_system = std::make_unique<FileSystemGCWii>(this, partition);
        return file_system->IsValid() ? std::move(file_system) : nullptr;
      };

      auto get_data_offset = [this, partition]() -> u64 {
        return ReadSwappedAndShifted(partition.offset + 0x2b8, PARTITION_NONE).value_or(0);
      };

      m_partitions.emplace(
          partition, PartitionDetails{Common::Lazy<std::unique_ptr<Common::AES::Context>>(get_key),
                                      Common::Lazy<IOS::ES::TicketReader>(get_ticket),
                                      Common::Lazy<IOS::ES::TMDReader>(get_tmd),
                                      Common::Lazy<std::vector<u8>>(get_cert_chain),
                                      Common::Lazy<std::vector<u8>>(get_h3_table),
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

  const u64 partition_data_offset = partition.offset + *partition_details.data_offset;
  if (m_has_hashes && m_has_encryption &&
      m_reader->SupportsReadWiiDecrypted(offset, length, partition_data_offset))
  {
    return m_reader->ReadWiiDecrypted(offset, length, buffer, partition_data_offset);
  }

  if (!m_has_hashes)
  {
    return m_reader->Read(partition_data_offset + offset, length, buffer);
  }

  Common::AES::Context* aes_context = nullptr;
  std::unique_ptr<u8[]> read_buffer = nullptr;
  if (m_has_encryption)
  {
    aes_context = partition_details.key->get();
    if (!aes_context)
      return false;

    read_buffer = std::make_unique<u8[]>(BLOCK_TOTAL_SIZE);
  }

  while (length > 0)
  {
    // Calculate offsets
    u64 block_offset_on_disc = partition_data_offset + offset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE;
    u64 data_offset_in_block = offset % BLOCK_DATA_SIZE;

    if (m_last_decrypted_block != block_offset_on_disc)
    {
      if (m_has_encryption)
      {
        // Read the current block
        if (!m_reader->Read(block_offset_on_disc, BLOCK_TOTAL_SIZE, read_buffer.get()))
          return false;

        // Decrypt the block's data
        DecryptBlockData(read_buffer.get(), m_last_decrypted_block_data, aes_context);
      }
      else
      {
        // Read the current block
        if (!m_reader->Read(block_offset_on_disc + BLOCK_HEADER_SIZE, BLOCK_DATA_SIZE,
                            m_last_decrypted_block_data))
        {
          return false;
        }
      }

      m_last_decrypted_block = block_offset_on_disc;
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

bool VolumeWii::HasWiiHashes() const
{
  return m_has_hashes;
}

bool VolumeWii::HasWiiEncryption() const
{
  return m_has_encryption;
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

const std::vector<u8>& VolumeWii::GetCertificateChain(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? *it->second.cert_chain : INVALID_CERT_CHAIN;
}

const FileSystem* VolumeWii::GetFileSystem(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  return it != m_partitions.end() ? it->second.file_system->get() : nullptr;
}

u64 VolumeWii::OffsetInHashedPartitionToRawOffset(u64 offset, const Partition& partition,
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

  if (!m_has_hashes)
    return partition.offset + data_offset + offset;

  return OffsetInHashedPartitionToRawOffset(offset, partition, data_offset);
}

std::string VolumeWii::GetGameTDBID(const Partition& partition) const
{
  return GetGameID(partition);
}

Region VolumeWii::GetRegion() const
{
  return RegionCodeToRegion(m_reader->ReadSwapped<u32>(0x4E000));
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

Platform VolumeWii::GetVolumeType() const
{
  return Platform::WiiDisc;
}

bool VolumeWii::IsDatelDisc() const
{
  return m_game_partition == PARTITION_NONE;
}

BlobType VolumeWii::GetBlobType() const
{
  return m_reader->GetBlobType();
}

u64 VolumeWii::GetDataSize() const
{
  return m_reader->GetDataSize();
}

DataSizeType VolumeWii::GetDataSizeType() const
{
  return m_reader->GetDataSizeType();
}

u64 VolumeWii::GetRawSize() const
{
  return m_reader->GetRawSize();
}

const BlobReader& VolumeWii::GetBlobReader() const
{
  return *m_reader;
}

std::array<u8, 20> VolumeWii::GetSyncHash() const
{
  auto context = Common::SHA1::CreateContext();

  // Disc header
  ReadAndAddToSyncHash(context.get(), 0, 0x80, PARTITION_NONE);

  // Region code
  ReadAndAddToSyncHash(context.get(), 0x4E000, 4, PARTITION_NONE);

  // The data offset of the game partition - an important factor for disc drive timings
  const u64 data_offset = PartitionOffsetToRawOffset(0, GetGamePartition());
  context->Update(reinterpret_cast<const u8*>(&data_offset), sizeof(data_offset));

  // TMD
  AddTMDToSyncHash(context.get(), GetGamePartition());

  // Game partition contents
  AddGamePartitionToSyncHash(context.get());

  return context->Finish();
}

bool VolumeWii::CheckH3TableIntegrity(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;

  const std::vector<u8>& h3_table = *partition_details.h3_table;
  if (h3_table.size() != WII_PARTITION_H3_SIZE)
    return false;

  const IOS::ES::TMDReader& tmd = *partition_details.tmd;
  if (!tmd.IsValid())
    return false;

  const std::vector<IOS::ES::Content> contents = tmd.GetContents();
  if (contents.size() != 1)
    return false;

  return Common::SHA1::CalculateDigest(h3_table) == contents[0].sha1;
}

bool VolumeWii::CheckBlockIntegrity(u64 block_index, const u8* encrypted_data,
                                    const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;

  if (block_index / BLOCKS_PER_GROUP * Common::SHA1::DIGEST_LEN >=
      partition_details.h3_table->size())
  {
    return false;
  }

  HashBlock hashes;
  u8 cluster_data_buffer[BLOCK_DATA_SIZE];
  const u8* cluster_data;

  if (m_has_encryption)
  {
    Common::AES::Context* aes_context = partition_details.key->get();
    if (!aes_context)
      return false;

    DecryptBlockHashes(encrypted_data, &hashes, aes_context);
    DecryptBlockData(encrypted_data, cluster_data_buffer, aes_context);
    cluster_data = cluster_data_buffer;
  }
  else
  {
    std::memcpy(&hashes, encrypted_data, BLOCK_HEADER_SIZE);
    cluster_data = encrypted_data + BLOCK_HEADER_SIZE;
  }

  for (u32 hash_index = 0; hash_index < 31; ++hash_index)
  {
    if (Common::SHA1::CalculateDigest(&cluster_data[hash_index * 0x400], 0x400) !=
        hashes.h0[hash_index])
    {
      return false;
    }
  }

  if (Common::SHA1::CalculateDigest(hashes.h0) != hashes.h1[block_index % 8])
    return false;

  if (Common::SHA1::CalculateDigest(hashes.h1) != hashes.h2[block_index / 8 % 8])
    return false;

  Common::SHA1::Digest h3_digest;
  auto h3_digest_ptr =
      partition_details.h3_table->data() + block_index / 64 * Common::SHA1::DIGEST_LEN;
  memcpy(h3_digest.data(), h3_digest_ptr, sizeof(h3_digest));
  if (Common::SHA1::CalculateDigest(hashes.h2) != h3_digest)
    return false;

  return true;
}

bool VolumeWii::CheckBlockIntegrity(u64 block_index, const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;
  const u64 cluster_offset =
      partition.offset + *partition_details.data_offset + block_index * BLOCK_TOTAL_SIZE;

  std::vector<u8> cluster(BLOCK_TOTAL_SIZE);
  if (!m_reader->Read(cluster_offset, cluster.size(), cluster.data()))
    return false;
  return CheckBlockIntegrity(block_index, cluster.data(), partition);
}

bool VolumeWii::HashGroup(const std::array<u8, BLOCK_DATA_SIZE> in[BLOCKS_PER_GROUP],
                          HashBlock out[BLOCKS_PER_GROUP],
                          const std::function<bool(size_t block)>& read_function)
{
  std::array<std::future<void>, BLOCKS_PER_GROUP> hash_futures;
  bool success = true;

  for (size_t i = 0; i < BLOCKS_PER_GROUP; ++i)
  {
    if (read_function && success)
      success = read_function(i);

    hash_futures[i] = std::async(std::launch::async, [&in, &out, &hash_futures, success, i]() {
      const size_t h1_base = Common::AlignDown(i, 8);

      if (success)
      {
        // H0 hashes
        for (size_t j = 0; j < 31; ++j)
          out[i].h0[j] = Common::SHA1::CalculateDigest(in[i].data() + j * 0x400, 0x400);

        // H0 padding
        out[i].padding_0 = {};

        // H1 hash
        out[h1_base].h1[i - h1_base] = Common::SHA1::CalculateDigest(out[i].h0);
      }

      if (i % 8 == 7)
      {
        for (size_t j = 0; j < 7; ++j)
          hash_futures[h1_base + j].get();

        if (success)
        {
          // H1 padding
          out[h1_base].padding_1 = {};

          // H1 copies
          for (size_t j = 1; j < 8; ++j)
            out[h1_base + j].h1 = out[h1_base].h1;

          // H2 hash
          out[0].h2[h1_base / 8] = Common::SHA1::CalculateDigest(out[i].h1);
        }

        if (i == BLOCKS_PER_GROUP - 1)
        {
          for (size_t j = 0; j < 7; ++j)
            hash_futures[j * 8 + 7].get();

          if (success)
          {
            // H2 padding
            out[0].padding_2 = {};

            // H2 copies
            for (size_t j = 1; j < BLOCKS_PER_GROUP; ++j)
              out[j].h2 = out[0].h2;
          }
        }
      }
    });
  }

  // Wait for all the async tasks to finish
  hash_futures.back().get();

  return success;
}

bool VolumeWii::EncryptGroup(
    u64 offset, u64 partition_data_offset, u64 partition_data_decrypted_size,
    const std::array<u8, AES_KEY_SIZE>& key, BlobReader* blob,
    std::array<u8, GROUP_TOTAL_SIZE>* out,
    const std::function<void(HashBlock hash_blocks[BLOCKS_PER_GROUP])>& hash_exception_callback)
{
  std::vector<std::array<u8, BLOCK_DATA_SIZE>> unencrypted_data(BLOCKS_PER_GROUP);
  std::vector<HashBlock> unencrypted_hashes(BLOCKS_PER_GROUP);

  const bool success =
      HashGroup(unencrypted_data.data(), unencrypted_hashes.data(), [&](size_t block) {
        if (offset + (block + 1) * BLOCK_DATA_SIZE <= partition_data_decrypted_size)
        {
          if (!blob->ReadWiiDecrypted(offset + block * BLOCK_DATA_SIZE, BLOCK_DATA_SIZE,
                                      unencrypted_data[block].data(), partition_data_offset))
          {
            return false;
          }
        }
        else
        {
          unencrypted_data[block].fill(0);
        }
        return true;
      });

  if (!success)
    return false;

  if (hash_exception_callback)
    hash_exception_callback(unencrypted_hashes.data());

  const unsigned int threads =
      std::min(BLOCKS_PER_GROUP, std::max<unsigned int>(1, std::thread::hardware_concurrency()));

  std::vector<std::future<void>> encryption_futures(threads);

  auto aes_context = Common::AES::CreateContextEncrypt(key.data());

  for (size_t i = 0; i < threads; ++i)
  {
    encryption_futures[i] = std::async(
        std::launch::async,
        [&unencrypted_data, &unencrypted_hashes, &aes_context, &out](size_t start, size_t end) {
          for (size_t j = start; j < end; ++j)
          {
            u8* out_ptr = out->data() + j * BLOCK_TOTAL_SIZE;

            aes_context->CryptIvZero(reinterpret_cast<u8*>(&unencrypted_hashes[j]), out_ptr,
                                     BLOCK_HEADER_SIZE);

            aes_context->Crypt(out_ptr + 0x3D0, unencrypted_data[j].data(),
                               out_ptr + BLOCK_HEADER_SIZE, BLOCK_DATA_SIZE);
          }
        },
        i * BLOCKS_PER_GROUP / threads, (i + 1) * BLOCKS_PER_GROUP / threads);
  }

  for (std::future<void>& future : encryption_futures)
    future.get();

  return true;
}

void VolumeWii::DecryptBlockHashes(const u8* in, HashBlock* out, Common::AES::Context* aes_context)
{
  aes_context->CryptIvZero(in, reinterpret_cast<u8*>(out), sizeof(HashBlock));
}

void VolumeWii::DecryptBlockData(const u8* in, u8* out, Common::AES::Context* aes_context)
{
  aes_context->Crypt(&in[0x3d0], &in[sizeof(HashBlock)], out, BLOCK_DATA_SIZE);
}

}  // namespace DiscIO
