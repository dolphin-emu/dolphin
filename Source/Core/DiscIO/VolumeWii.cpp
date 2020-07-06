// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>

#include "Common/Align.h"
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

      auto get_cert_chain = [this, partition]() -> std::vector<u8> {
        const std::optional<u32> size = m_reader->ReadSwapped<u32>(partition.offset + 0x2ac);
        const std::optional<u64> address =
            ReadSwappedAndShifted(partition.offset + 0x2b0, PARTITION_NONE);
        if (!size || !address)
          return {};
        std::vector<u8> cert_chain(*size);
        if (!m_reader->Read(partition.offset + *address, *size, cert_chain.data()))
          return {};
        return cert_chain;
      };

      auto get_h3_table = [this, partition]() -> std::vector<u8> {
        if (!m_encrypted)
          return {};
        const std::optional<u64> h3_table_offset =
            ReadSwappedAndShifted(partition.offset + 0x2b4, PARTITION_NONE);
        if (!h3_table_offset)
          return {};
        std::vector<u8> h3_table(H3_TABLE_SIZE);
        if (!m_reader->Read(partition.offset + *h3_table_offset, H3_TABLE_SIZE, h3_table.data()))
          return {};
        return h3_table;
      };

      auto get_key = [this, partition]() -> std::unique_ptr<mbedtls_aes_context> {
        const IOS::ES::TicketReader& ticket = *m_partitions[partition].ticket;
        if (!ticket.IsValid())
          return nullptr;
        const std::array<u8, AES_KEY_SIZE> key = ticket.GetTitleKey();
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

  if (m_reader->SupportsReadWiiDecrypted())
  {
    return m_reader->ReadWiiDecrypted(offset, length, buffer,
                                      partition.offset + *partition_details.data_offset);
  }

  if (!m_encrypted)
  {
    return m_reader->Read(partition.offset + *partition_details.data_offset + offset, length,
                          buffer);
  }

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

      // Decrypt the block's data
      DecryptBlockData(read_buffer.data(), m_last_decrypted_block_data, aes_context);
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

u64 VolumeWii::GetSize() const
{
  return m_reader->GetDataSize();
}

bool VolumeWii::IsSizeAccurate() const
{
  return m_reader->IsDataSizeAccurate();
}

u64 VolumeWii::GetRawSize() const
{
  return m_reader->GetRawSize();
}

const BlobReader& VolumeWii::GetBlobReader() const
{
  return *m_reader;
}

bool VolumeWii::CheckH3TableIntegrity(const Partition& partition) const
{
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;

  const std::vector<u8>& h3_table = *partition_details.h3_table;
  if (h3_table.size() != H3_TABLE_SIZE)
    return false;

  const IOS::ES::TMDReader& tmd = *partition_details.tmd;
  if (!tmd.IsValid())
    return false;

  const std::vector<IOS::ES::Content> contents = tmd.GetContents();
  if (contents.size() != 1)
    return false;

  std::array<u8, 20> h3_table_sha1;
  mbedtls_sha1_ret(h3_table.data(), h3_table.size(), h3_table_sha1.data());
  return h3_table_sha1 == contents[0].sha1;
}

bool VolumeWii::CheckBlockIntegrity(u64 block_index, const std::vector<u8>& encrypted_data,
                                    const Partition& partition) const
{
  if (encrypted_data.size() != BLOCK_TOTAL_SIZE)
    return false;

  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  const PartitionDetails& partition_details = it->second;

  if (block_index / BLOCKS_PER_GROUP * SHA1_SIZE >= partition_details.h3_table->size())
    return false;

  mbedtls_aes_context* aes_context = partition_details.key->get();
  if (!aes_context)
    return false;

  HashBlock hashes;
  DecryptBlockHashes(encrypted_data.data(), &hashes, aes_context);

  u8 cluster_data[BLOCK_DATA_SIZE];
  DecryptBlockData(encrypted_data.data(), cluster_data, aes_context);

  for (u32 hash_index = 0; hash_index < 31; ++hash_index)
  {
    u8 h0_hash[SHA1_SIZE];
    mbedtls_sha1_ret(cluster_data + hash_index * 0x400, 0x400, h0_hash);
    if (memcmp(h0_hash, hashes.h0[hash_index], SHA1_SIZE))
      return false;
  }

  u8 h1_hash[SHA1_SIZE];
  mbedtls_sha1_ret(reinterpret_cast<u8*>(hashes.h0), sizeof(hashes.h0), h1_hash);
  if (memcmp(h1_hash, hashes.h1[block_index % 8], SHA1_SIZE))
    return false;

  u8 h2_hash[SHA1_SIZE];
  mbedtls_sha1_ret(reinterpret_cast<u8*>(hashes.h1), sizeof(hashes.h1), h2_hash);
  if (memcmp(h2_hash, hashes.h2[block_index / 8 % 8], SHA1_SIZE))
    return false;

  u8 h3_hash[SHA1_SIZE];
  mbedtls_sha1_ret(reinterpret_cast<u8*>(hashes.h2), sizeof(hashes.h2), h3_hash);
  if (memcmp(h3_hash, partition_details.h3_table->data() + block_index / 64 * SHA1_SIZE, SHA1_SIZE))
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
  return CheckBlockIntegrity(block_index, cluster, partition);
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
          mbedtls_sha1_ret(in[i].data() + j * 0x400, 0x400, out[i].h0[j]);

        // H0 padding
        std::memset(out[i].padding_0, 0, sizeof(HashBlock::padding_0));

        // H1 hash
        mbedtls_sha1_ret(reinterpret_cast<u8*>(out[i].h0), sizeof(HashBlock::h0),
                         out[h1_base].h1[i - h1_base]);
      }

      if (i % 8 == 7)
      {
        for (size_t j = 0; j < 7; ++j)
          hash_futures[h1_base + j].get();

        if (success)
        {
          // H1 padding
          std::memset(out[h1_base].padding_1, 0, sizeof(HashBlock::padding_1));

          // H1 copies
          for (size_t j = 1; j < 8; ++j)
            std::memcpy(out[h1_base + j].h1, out[h1_base].h1, sizeof(HashBlock::h1));

          // H2 hash
          mbedtls_sha1_ret(reinterpret_cast<u8*>(out[i].h1), sizeof(HashBlock::h1),
                           out[0].h2[h1_base / 8]);
        }

        if (i == BLOCKS_PER_GROUP - 1)
        {
          for (size_t j = 0; j < 7; ++j)
            hash_futures[j * 8 + 7].get();

          if (success)
          {
            // H2 padding
            std::memset(out[0].padding_2, 0, sizeof(HashBlock::padding_2));

            // H2 copies
            for (size_t j = 1; j < BLOCKS_PER_GROUP; ++j)
              std::memcpy(out[j].h2, out[0].h2, sizeof(HashBlock::h2));
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

  mbedtls_aes_context aes_context;
  mbedtls_aes_setkey_enc(&aes_context, key.data(), 128);

  for (size_t i = 0; i < threads; ++i)
  {
    encryption_futures[i] = std::async(
        std::launch::async,
        [&unencrypted_data, &unencrypted_hashes, &aes_context, &out](size_t start, size_t end) {
          for (size_t i = start; i < end; ++i)
          {
            u8* out_ptr = out->data() + i * BLOCK_TOTAL_SIZE;

            u8 iv[16] = {};
            mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_ENCRYPT, BLOCK_HEADER_SIZE, iv,
                                  reinterpret_cast<u8*>(&unencrypted_hashes[i]), out_ptr);

            std::memcpy(iv, out_ptr + 0x3D0, sizeof(iv));
            mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_ENCRYPT, BLOCK_DATA_SIZE, iv,
                                  unencrypted_data[i].data(), out_ptr + BLOCK_HEADER_SIZE);
          }
        },
        i * BLOCKS_PER_GROUP / threads, (i + 1) * BLOCKS_PER_GROUP / threads);
  }

  for (std::future<void>& future : encryption_futures)
    future.get();

  return true;
}

void VolumeWii::DecryptBlockHashes(const u8* in, HashBlock* out, mbedtls_aes_context* aes_context)
{
  std::array<u8, 16> iv;
  iv.fill(0);
  mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, sizeof(HashBlock), iv.data(), in,
                        reinterpret_cast<u8*>(out));
}

void VolumeWii::DecryptBlockData(const u8* in, u8* out, mbedtls_aes_context* aes_context)
{
  std::array<u8, 16> iv;
  std::copy(&in[0x3d0], &in[0x3e0], iv.data());
  mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, BLOCK_DATA_SIZE, iv.data(),
                        &in[BLOCK_HEADER_SIZE], out);
}

}  // namespace DiscIO
