// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{
CVolumeWiiCrypted::CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader)
    : m_pReader(std::move(reader)), m_game_partition(PARTITION_NONE), m_last_decrypted_block(-1)
{
  // Get decryption keys for all partitions
  CBlobBigEndianReader big_endian_reader(*m_pReader.get());
  for (unsigned int partition_group = 0; partition_group < 4; ++partition_group)
  {
    u32 number_of_partitions;
    if (!big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8), &number_of_partitions))
      continue;

    u32 read_buffer;
    big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8) + 4, &read_buffer);
    u64 partition_table_offset = (u64)read_buffer << 2;

    for (u32 i = 0; i < number_of_partitions; i++)
    {
      big_endian_reader.ReadSwapped(partition_table_offset + (i * 8), &read_buffer);
      u64 partition_offset = (u64)read_buffer << 2;

      if (m_game_partition == PARTITION_NONE)
      {
        u32 partition_type;
        if (big_endian_reader.ReadSwapped(partition_table_offset + (i * 8) + 4, &partition_type))
          if (partition_type == 0)
            m_game_partition = Partition(partition_offset);
      }

      u8 sub_key[16];
      m_pReader->Read(partition_offset + 0x1bf, 16, sub_key);

      u8 IV[16];
      memset(IV, 0, 16);
      m_pReader->Read(partition_offset + 0x44c, 8, IV);

      static const u8 master_key[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
                                        0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};

      static const u8 master_key_korean[16] = {0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e,
                                               0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e};

      u8 using_korean_key = 0;
      big_endian_reader.ReadSwapped(partition_offset + 0x1f1, &using_korean_key);

      mbedtls_aes_context aes_context;
      mbedtls_aes_setkey_dec(&aes_context, (using_korean_key ? master_key_korean : master_key),
                             128);

      u8 volume_key[16];
      mbedtls_aes_crypt_cbc(&aes_context, MBEDTLS_AES_DECRYPT, 16, IV, sub_key, volume_key);

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
  if (m_pReader == nullptr)
    return false;

  if (partition == PARTITION_NONE)
    return m_pReader->Read(_ReadOffset, _Length, _pBuffer);

  // Get the decryption key for the partition
  auto it = m_partitions.find(partition);
  if (it == m_partitions.end())
    return false;
  mbedtls_aes_context* aes_context = it->second.get();

  FileMon::FindFilename(_ReadOffset, partition);

  std::vector<u8> read_buffer(s_block_total_size);
  while (_Length > 0)
  {
    // Calculate offsets
    u64 block_offset_on_disc = partition.offset + s_partition_data_offset +
                               _ReadOffset / s_block_data_size * s_block_total_size;
    u64 data_offset_in_block = _ReadOffset % s_block_data_size;

    if (m_last_decrypted_block != block_offset_on_disc)
    {
      // Read the current block
      if (!m_pReader->Read(block_offset_on_disc, s_block_total_size, read_buffer.data()))
        return false;

      // Decrypt the block's data.
      // 0x3D0 - 0x3DF in read_buffer will be overwritten,
      // but that won't affect anything, because we won't
      // use the content of read_buffer anymore after this
      mbedtls_aes_crypt_cbc(aes_context, MBEDTLS_AES_DECRYPT, s_block_data_size,
                            &read_buffer[0x3D0], &read_buffer[s_block_header_size],
                            m_last_decrypted_block_data);
      m_last_decrypted_block = block_offset_on_disc;

      // The only thing we currently use from the 0x000 - 0x3FF part
      // of the block is the IV (at 0x3D0), but it also contains SHA-1
      // hashes that IOS uses to check that discs aren't tampered with.
      // http://wiibrew.org/wiki/Wii_Disc#Encrypted
    }

    // Copy the decrypted data
    u64 copy_size = std::min(_Length, s_block_data_size - data_offset_in_block);
    memcpy(_pBuffer, &m_last_decrypted_block_data[data_offset_in_block], (size_t)copy_size);

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
  for (auto it = m_partitions.begin(); it != m_partitions.end(); ++it)
    partitions.push_back(it->first);
  return partitions;
}

Partition CVolumeWiiCrypted::GetGamePartition() const
{
  return m_game_partition;
}

bool CVolumeWiiCrypted::GetTitleID(u64* buffer) const
{
  // Tik is at m_VolumeOffset size 0x2A4
  // TitleID offset in tik is 0x1DC
  if (!Read(GetGamePartition().offset + 0x1DC, sizeof(u64), reinterpret_cast<u8*>(buffer),
            PARTITION_NONE))
    return false;

  *buffer = Common::swap64(*buffer);
  return true;
}

std::vector<u8> CVolumeWiiCrypted::GetTMD(const Partition& partition) const
{
  u32 tmd_size;
  u32 tmd_address;

  Read(partition.offset + 0x2a4, sizeof(u32), (u8*)&tmd_size, PARTITION_NONE);
  Read(partition.offset + 0x2a8, sizeof(u32), (u8*)&tmd_address, PARTITION_NONE);
  tmd_size = Common::swap32(tmd_size);
  tmd_address = Common::swap32(tmd_address) << 2;

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
  Read(partition.offset + tmd_address, tmd_size, buffer.data(), PARTITION_NONE);

  return buffer;
}

std::string CVolumeWiiCrypted::GetUniqueID() const
{
  if (m_pReader == nullptr)
    return std::string();

  char ID[6];

  if (!Read(0, 6, (u8*)ID, PARTITION_NONE))
    return std::string();

  return DecodeString(ID);
}

Country CVolumeWiiCrypted::GetCountry() const
{
  if (!m_pReader)
    return Country::COUNTRY_UNKNOWN;

  u8 country_byte;
  if (!m_pReader->Read(3, 1, &country_byte))
    return Country::COUNTRY_UNKNOWN;

  Country country_value = CountrySwitch(country_byte);

  u32 region_code;
  if (!ReadSwapped(0x4E000, &region_code, DiscIO::PARTITION_NONE))
    return country_value;

  switch (region_code)
  {
  case 0:
    switch (country_value)
    {
    case Country::COUNTRY_TAIWAN:
      return Country::COUNTRY_TAIWAN;
    default:
      return Country::COUNTRY_JAPAN;
    }
  case 1:
    return Country::COUNTRY_USA;
  case 2:
    switch (country_value)
    {
    case Country::COUNTRY_FRANCE:
    case Country::COUNTRY_GERMANY:
    case Country::COUNTRY_ITALY:
    case Country::COUNTRY_NETHERLANDS:
    case Country::COUNTRY_RUSSIA:
    case Country::COUNTRY_SPAIN:
    case Country::COUNTRY_AUSTRALIA:
      return country_value;
    default:
      return Country::COUNTRY_EUROPE;
    }
  case 4:
    return Country::COUNTRY_KOREA;
  default:
    return country_value;
  }
}

std::string CVolumeWiiCrypted::GetMakerID() const
{
  if (m_pReader == nullptr)
    return std::string();

  char makerID[2];

  if (!Read(0x4, 0x2, (u8*)&makerID, PARTITION_NONE))
    return std::string();

  return DecodeString(makerID);
}

u16 CVolumeWiiCrypted::GetRevision() const
{
  if (!m_pReader)
    return 0;

  u8 revision;
  if (!m_pReader->Read(7, 1, &revision))
    return 0;

  return revision;
}

std::string CVolumeWiiCrypted::GetInternalName() const
{
  char name_buffer[0x60];
  if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)&name_buffer, PARTITION_NONE))
    return DecodeString(name_buffer);

  return "";
}

std::map<Language, std::string> CVolumeWiiCrypted::GetLongNames() const
{
  std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this, GetGamePartition()));
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
  if (!GetTitleID(&title_id))
    return std::vector<u32>();

  return GetWiiBanner(width, height, title_id);
}

u64 CVolumeWiiCrypted::GetFSTSize() const
{
  if (m_pReader == nullptr)
    return 0;

  u32 size;

  if (!Read(0x428, 0x4, (u8*)&size, GetGamePartition()))
    return 0;

  return (u64)Common::swap32(size) << 2;
}

std::string CVolumeWiiCrypted::GetApploaderDate() const
{
  if (m_pReader == nullptr)
    return std::string();

  char date[16];

  if (!Read(0x2440, 0x10, (u8*)&date, GetGamePartition()))
    return std::string();

  return DecodeString(date);
}

Platform CVolumeWiiCrypted::GetVolumeType() const
{
  return Platform::WII_DISC;
}

u8 CVolumeWiiCrypted::GetDiscNumber() const
{
  u8 disc_number;
  m_pReader->Read(6, 1, &disc_number);
  return disc_number;
}

BlobType CVolumeWiiCrypted::GetBlobType() const
{
  return m_pReader ? m_pReader->GetBlobType() : BlobType::PLAIN;
}

u64 CVolumeWiiCrypted::GetSize() const
{
  if (m_pReader)
    return m_pReader->GetDataSize();
  else
    return 0;
}

u64 CVolumeWiiCrypted::GetRawSize() const
{
  if (m_pReader)
    return m_pReader->GetRawSize();
  else
    return 0;
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
    u64 clusterOff = partition.offset + s_partition_data_offset + (u64)clusterID * 0x8000;

    // Read and decrypt the cluster metadata
    u8 clusterMDCrypted[0x400];
    u8 clusterMD[0x400];
    u8 IV[16] = {0};
    if (!m_pReader->Read(clusterOff, 0x400, clusterMDCrypted))
    {
      NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", clusterID);
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
      NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read data", clusterID);
      return false;
    }

    for (u32 hashID = 0; hashID < 31; ++hashID)
    {
      u8 hash[20];

      mbedtls_sha1(clusterData + hashID * 0x400, 0x400, hash);

      // Note that we do not use strncmp here
      if (memcmp(hash, clusterMD + hashID * 20, 20))
      {
        NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: hash %d is invalid", clusterID,
                   hashID);
        return false;
      }
    }
  }

  return true;
}

}  // namespace
