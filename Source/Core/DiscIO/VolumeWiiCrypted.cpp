// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/VolumeWiiCrypted.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/Swap.h"

#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

namespace DiscIO
{
constexpr u64 PARTITION_DATA_OFFSET = 0x20000;

CVolumeWiiCrypted::CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader)
    : m_pReader(std::move(reader)), m_game_partition(PARTITION_NONE), m_last_decrypted_block(-1)
{
  _assert_(m_pReader);

  // Get decryption keys for all partitions
  CBlobBigEndianReader big_endian_reader(*m_pReader.get());
  for (u32 partition_group = 0; partition_group < 4; ++partition_group)
  {
    u32 number_of_partitions;
    if (!big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8), &number_of_partitions))
      continue;

    u32 read_buffer;
    if (!big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8) + 4, &read_buffer))
      continue;
    const u64 partition_table_offset = (u64)read_buffer << 2;

    for (u32 i = 0; i < number_of_partitions; i++)
    {
      if (!big_endian_reader.ReadSwapped(partition_table_offset + (i * 8), &read_buffer))
        continue;
      const u64 partition_offset = (u64)read_buffer << 2;

      if (m_game_partition == PARTITION_NONE)
      {
        u32 partition_type;
        if (!big_endian_reader.ReadSwapped(partition_table_offset + (i * 8) + 4, &partition_type))
          continue;

        if (partition_type == 0)
          m_game_partition = Partition(partition_offset);
      }

      u8 sub_key[16];
      if (!m_pReader->Read(partition_offset + 0x1bf, 16, sub_key))
        continue;

      u8 iv[16];
      memset(iv, 0, 16);
      if (!m_pReader->Read(partition_offset + 0x44c, 8, iv))
        continue;

      static const u8 common_key_standard[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
                                                 0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};
      static const u8 common_key_korean[16] = {0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e,
                                               0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e};
      static const u8 common_key_rvt[16] = {0xa1, 0x60, 0x4a, 0x6a, 0x71, 0x23, 0xb5, 0x29,
                                            0xae, 0x8b, 0xec, 0x32, 0xc8, 0x16, 0xfc, 0xaa};
      static const char issuer_rvt[] = "Root-CA00000002-XS00000006";

      const u8* common_key;

      u8 issuer[sizeof(issuer_rvt)];
      if (!m_pReader->Read(partition_offset + 0x140, sizeof(issuer), issuer))
        continue;

      if (!memcmp(issuer, issuer_rvt, sizeof(issuer_rvt)))
      {
        // RVT issuer. Use the RVT (debug) master key.
        common_key = common_key_rvt;
      }
      else
      {
        u8 key_number = 0;
        if (!big_endian_reader.ReadSwapped(partition_offset + 0x1f1, &key_number))
          continue;
        common_key = (key_number == 1) ? common_key_korean : common_key_standard;
      }

      mbedtls_aes_context aes_context;
      mbedtls_aes_setkey_dec(&aes_context, common_key, 128);

      u8 volume_key[16];
      mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_DECRYPT, 16, iv, sub_key, volume_key);

      std::unique_ptr<mbedtls_aes_context> partition_AES_context =
          std::make_unique<mbedtls_aes_context>();
      mbedtls_aes_setkey_dec(partition_AES_context.get(), volume_key, 128);
      m_partitions[Partition(partition_offset)] = std::move(partition_AES_context);
    }
  }
}

CVolumeWiiCrypted::~CVolumeWiiCrypted()
{
}

bool CVolumeWiiCrypted::Read(u64 _ReadOffset, u64 _Length, u8* _pBuffer,
                             const Partition& partition) const
{
  if (partition == PARTITION_NONE)
    return m_pReader->Read(_ReadOffset, _Length, _pBuffer);

  // Get the decryption key for the partition
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  mbedtls_aes_context* aes_context = it->second.get();

  std::vector<u8> read_buffer(BLOCK_TOTAL_SIZE);
  while (_Length > 0)
  {
    // Calculate offsets
    u64 block_offset_on_disc =
        partition.offset + PARTITION_DATA_OFFSET + _ReadOffset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE;
    u64 data_offset_in_block = _ReadOffset % BLOCK_DATA_SIZE;

    if (m_last_decrypted_block != block_offset_on_disc)
    {
      // Read the current block
      if (!m_pReader->Read(block_offset_on_disc, BLOCK_TOTAL_SIZE, read_buffer.data()))
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
    u64 copy_size = std::min(_Length, BLOCK_DATA_SIZE - data_offset_in_block);
    memcpy(_pBuffer, &m_last_decrypted_block_data[data_offset_in_block],
           static_cast<size_t>(copy_size));

    // Update offsets
    _Length -= copy_size;
    _pBuffer += copy_size;
    _ReadOffset += copy_size;
  }

  return true;
}

std::vector<Partition> CVolumeWiiCrypted::GetPartitions() const
{
  std::vector<Partition> partitions;
  for (const auto& pair : m_partitions)
    partitions.push_back(pair.first);
  return partitions;
}

Partition CVolumeWiiCrypted::GetGamePartition() const
{
  return m_game_partition;
}

bool CVolumeWiiCrypted::GetTitleID(u64* buffer, const Partition& partition) const
{
  return ReadSwapped(partition.offset + 0x1DC, buffer, PARTITION_NONE);
}

IOS::ES::TicketReader CVolumeWiiCrypted::GetTicket(const Partition& partition) const
{
  std::vector<u8> buffer(0x2a4);
  Read(partition.offset, buffer.size(), buffer.data(), PARTITION_NONE);
  return IOS::ES::TicketReader{std::move(buffer)};
}

IOS::ES::TMDReader CVolumeWiiCrypted::GetTMD(const Partition& partition) const
{
  u32 tmd_size = 0;
  u32 tmd_address = 0;

  if (!ReadSwapped(partition.offset + 0x2a4, &tmd_size, PARTITION_NONE))
    return {};
  if (!ReadSwapped(partition.offset + 0x2a8, &tmd_address, PARTITION_NONE))
    return {};
  tmd_address <<= 2;

  if (tmd_size > 1024 * 1024 * 4)
  {
    // The size is checked so that a malicious or corrupt ISO
    // can't force Dolphin to allocate up to 4 GiB of memory.
    // 4 MiB should be much bigger than the size of TMDs and much smaller
    // than the amount of RAM in a computer that can run Dolphin.
    PanicAlert("TMD > 4 MiB");
    tmd_size = 1024 * 1024 * 4;
  }

  std::vector<u8> buffer(tmd_size);
  if (!Read(partition.offset + tmd_address, tmd_size, buffer.data(), PARTITION_NONE))
    return {};

  return IOS::ES::TMDReader{std::move(buffer)};
}

u64 CVolumeWiiCrypted::PartitionOffsetToRawOffset(u64 offset, const Partition& partition)
{
  if (partition == PARTITION_NONE)
    return offset;

  return partition.offset + PARTITION_DATA_OFFSET + (offset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE) +
         (offset % BLOCK_DATA_SIZE);
}

std::string CVolumeWiiCrypted::GetGameID(const Partition& partition) const
{
  char ID[6];

  if (!Read(0, 6, (u8*)ID, partition))
    return std::string();

  return DecodeString(ID);
}

Region CVolumeWiiCrypted::GetRegion() const
{
  u32 region_code;
  if (!ReadSwapped(0x4E000, &region_code, PARTITION_NONE))
    return Region::UNKNOWN_REGION;

  return static_cast<Region>(region_code);
}

Country CVolumeWiiCrypted::GetCountry(const Partition& partition) const
{
  u8 country_byte;
  if (!ReadSwapped(3, &country_byte, partition))
    return Country::COUNTRY_UNKNOWN;

  const Region region = GetRegion();

  if (RegionSwitchWii(country_byte) != region)
    return TypicalCountryForRegion(region);

  return CountrySwitch(country_byte);
}

std::string CVolumeWiiCrypted::GetMakerID(const Partition& partition) const
{
  char makerID[2];

  if (!Read(0x4, 0x2, (u8*)&makerID, partition))
    return std::string();

  return DecodeString(makerID);
}

u16 CVolumeWiiCrypted::GetRevision(const Partition& partition) const
{
  u8 revision;
  if (!ReadSwapped(7, &revision, partition))
    return 0;

  return revision;
}

std::string CVolumeWiiCrypted::GetInternalName(const Partition& partition) const
{
  char name_buffer[0x60];
  if (Read(0x20, 0x60, (u8*)&name_buffer, partition))
    return DecodeString(name_buffer);

  return "";
}

std::map<Language, std::string> CVolumeWiiCrypted::GetLongNames() const
{
  std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this, GetGamePartition()));
  if (!file_system)
    return {{}};

  std::vector<u8> opening_bnr(NAMES_TOTAL_BYTES);
  size_t size = file_system->ReadFile("opening.bnr", opening_bnr.data(), opening_bnr.size(), 0x5C);
  opening_bnr.resize(size);
  return ReadWiiNames(opening_bnr);
}

std::vector<u32> CVolumeWiiCrypted::GetBanner(int* width, int* height) const
{
  *width = 0;
  *height = 0;

  u64 title_id;
  if (!GetTitleID(&title_id, GetGamePartition()))
    return std::vector<u32>();

  return GetWiiBanner(width, height, title_id);
}

u64 CVolumeWiiCrypted::GetFSTSize(const Partition& partition) const
{
  u32 size;

  if (!Read(0x428, 0x4, (u8*)&size, partition))
    return 0;

  return (u64)Common::swap32(size) << 2;
}

std::string CVolumeWiiCrypted::GetApploaderDate(const Partition& partition) const
{
  char date[16];

  if (!Read(0x2440, 0x10, (u8*)&date, partition))
    return std::string();

  return DecodeString(date);
}

Platform CVolumeWiiCrypted::GetVolumeType() const
{
  return Platform::WII_DISC;
}

u8 CVolumeWiiCrypted::GetDiscNumber(const Partition& partition) const
{
  u8 disc_number = 0;
  ReadSwapped(6, &disc_number, partition);
  return disc_number;
}

BlobType CVolumeWiiCrypted::GetBlobType() const
{
  return m_pReader->GetBlobType();
}

u64 CVolumeWiiCrypted::GetSize() const
{
  return m_pReader->GetDataSize();
}

u64 CVolumeWiiCrypted::GetRawSize() const
{
  return m_pReader->GetRawSize();
}

bool CVolumeWiiCrypted::CheckIntegrity(const Partition& partition) const
{
  // Get the decryption key for the partition
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  mbedtls_aes_context* aes_context = it->second.get();

  // Get partition data size
  u32 partSizeDiv4;
  Read(partition.offset + 0x2BC, 4, (u8*)&partSizeDiv4, PARTITION_NONE);
  u64 partDataSize = (u64)Common::swap32(partSizeDiv4) * 4;

  u32 nClusters = (u32)(partDataSize / 0x8000);
  for (u32 clusterID = 0; clusterID < nClusters; ++clusterID)
  {
    u64 clusterOff = partition.offset + PARTITION_DATA_OFFSET + (u64)clusterID * 0x8000;

    // Read and decrypt the cluster metadata
    u8 clusterMDCrypted[0x400];
    u8 clusterMD[0x400];
    u8 IV[16] = {0};
    if (!Read(clusterOff, 0x400, clusterMDCrypted, PARTITION_NONE))
    {
      WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", clusterID);
      return false;
    }
    mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, 0x400, IV, clusterMDCrypted, clusterMD);

    // Some clusters have invalid data and metadata because they aren't
    // meant to be read by the game (for example, holes between files). To
    // try to avoid reporting errors because of these clusters, we check
    // the 0x00 paddings in the metadata.
    //
    // This may cause some false negatives though: some bad clusters may be
    // skipped because they are *too* bad and are not even recognized as
    // valid clusters. To be improved.
    bool meaningless = false;
    for (u32 idx = 0x26C; idx < 0x280; ++idx)
      if (clusterMD[idx] != 0)
        meaningless = true;

    if (meaningless)
      continue;

    u8 clusterData[0x7C00];
    if (!Read((u64)clusterID * 0x7C00, 0x7C00, clusterData, partition))
    {
      WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read data", clusterID);
      return false;
    }

    for (u32 hashID = 0; hashID < 31; ++hashID)
    {
      u8 hash[20];

      mbedtls_sha1(clusterData + hashID * 0x400, 0x400, hash);

      // Note that we do not use strncmp here
      if (memcmp(hash, clusterMD + hashID * 20, 20))
      {
        WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: hash %d is invalid", clusterID,
                 hashID);
        return false;
      }
    }
  }

  return true;
}

}  // namespace
