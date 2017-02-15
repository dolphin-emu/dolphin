// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeDirectory.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{
enum EDiscType
{
  DISC_TYPE_UNK,
  DISC_TYPE_WII,
  DISC_TYPE_WII_CONTAINER,
  DISC_TYPE_GC,
  DISC_TYPE_WAD
};

static const unsigned char s_master_key[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
                                               0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};

static const unsigned char s_master_key_korean[16] = {
    0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13, 0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e};

static const unsigned char s_master_key_rvt[16] = {0xa1, 0x60, 0x4a, 0x6a, 0x71, 0x23, 0xb5, 0x29,
                                                   0xae, 0x8b, 0xec, 0x32, 0xc8, 0x16, 0xfc, 0xaa};

static const char s_issuer_rvt[] = "Root-CA00000002-XS00000006";

static std::unique_ptr<IVolume> CreateVolumeFromCryptedWiiImage(std::unique_ptr<IBlobReader> reader,
                                                                u32 partition_group,
                                                                u32 volume_type, u32 volume_number);
EDiscType GetDiscType(IBlobReader& _rReader);

std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename, u32 partition_group,
                                                  u32 volume_number)
{
  std::unique_ptr<IBlobReader> reader(CreateBlobReader(filename));
  if (reader == nullptr)
    return nullptr;

  switch (GetDiscType(*reader))
  {
  case DISC_TYPE_WII:
  case DISC_TYPE_GC:
    return std::make_unique<CVolumeGC>(std::move(reader));

  case DISC_TYPE_WAD:
    return std::make_unique<CVolumeWAD>(std::move(reader));

  case DISC_TYPE_WII_CONTAINER:
    return CreateVolumeFromCryptedWiiImage(std::move(reader), partition_group, 0, volume_number);

  case DISC_TYPE_UNK:
    return nullptr;
  }

  return nullptr;
}

std::unique_ptr<IVolume> CreateVolumeFromDirectory(const std::string& directory, bool is_wii,
                                                   const std::string& apploader,
                                                   const std::string& dol)
{
  if (CVolumeDirectory::IsValidDirectory(directory))
    return std::make_unique<CVolumeDirectory>(directory, is_wii, apploader, dol);

  return nullptr;
}

void VolumeKeyForPartition(IBlobReader& _rReader, u64 offset, u8* VolumeKey)
{
  u8 SubKey[16];
  _rReader.Read(offset + 0x1bf, 16, SubKey);

  u8 IV[16];
  memset(IV, 0, 16);
  _rReader.Read(offset + 0x44c, 8, IV);

  mbedtls_aes_context AES_ctx;

  u8 issuer[sizeof(s_issuer_rvt)];
  _rReader.Read(offset + 0x140, sizeof(issuer), issuer);
  if (!memcmp(issuer, s_issuer_rvt, sizeof(s_issuer_rvt)))
  {
    // RVT issuer. Use the RVT (debug) master key.
    mbedtls_aes_setkey_dec(&AES_ctx, s_master_key_rvt, 128);
  }
  else
  {
    // Issue: 6813
    // Magic value is at partition's offset + 0x1f1 (1byte)
    // If encrypted with the Korean key, the magic value would be 1
    // Otherwise it is zero
    u8 using_korean_key = 0;
    _rReader.Read(offset + 0x1f1, sizeof(u8), &using_korean_key);
    u8 region = 0;
    _rReader.Read(0x3, sizeof(u8), &region);

    mbedtls_aes_setkey_dec(
        &AES_ctx, (using_korean_key == 1 && region == 'K' ? s_master_key_korean : s_master_key),
        128);
  }

  mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_DECRYPT, 16, IV, SubKey, VolumeKey);
}

static std::unique_ptr<IVolume> CreateVolumeFromCryptedWiiImage(std::unique_ptr<IBlobReader> reader,
                                                                u32 partition_group,
                                                                u32 volume_type, u32 volume_number)
{
  CBlobBigEndianReader big_endian_reader(*reader);

  u32 num_partitions;
  if (!big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8), &num_partitions))
    return nullptr;

  // Check if we're looking for a valid partition
  if ((int)volume_number != -1 && volume_number > num_partitions)
    return nullptr;

  u32 partitions_offset_unshifted;
  if (!big_endian_reader.ReadSwapped(0x40000 + (partition_group * 8) + 4,
                                     &partitions_offset_unshifted))
    return nullptr;
  u64 partitions_offset = (u64)partitions_offset_unshifted << 2;

  struct SPartition
  {
    SPartition(u64 offset_, u32 type_) : offset(offset_), type(type_) {}
    u64 offset;
    u32 type;
  };

  struct SPartitionGroup
  {
    u32 num_partitions;
    u64 partitions_offset;
    std::vector<SPartition> partitions;
  };
  SPartitionGroup partition_groups[4];

  // Read all partitions
  for (SPartitionGroup& group : partition_groups)
  {
    for (u32 i = 0; i < num_partitions; i++)
    {
      u32 partition_offset, partition_type;
      if (big_endian_reader.ReadSwapped(partitions_offset + (i * 8) + 0, &partition_offset) &&
          big_endian_reader.ReadSwapped(partitions_offset + (i * 8) + 4, &partition_type))
      {
        group.partitions.emplace_back((u64)partition_offset << 2, partition_type);
      }
    }
  }

  // Return the partition type specified or number
  // types: 0 = game, 1 = firmware update, 2 = channel installer
  //  some partitions on SSBB use the ASCII title id of the demo VC game they hold...
  for (size_t i = 0; i < partition_groups[partition_group].partitions.size(); i++)
  {
    const SPartition& partition = partition_groups[partition_group].partitions.at(i);

    if ((partition.type == volume_type && (int)volume_number == -1) || i == volume_number)
    {
      u8 volume_key[16];
      VolumeKeyForPartition(*reader, partition.offset, volume_key);
      return std::make_unique<CVolumeWiiCrypted>(std::move(reader), partition.offset, volume_key);
    }
  }

  return nullptr;
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
  CBlobBigEndianReader Reader(_rReader);

  // Check for Wii
  u32 WiiMagic = 0;
  Reader.ReadSwapped(0x18, &WiiMagic);
  u32 WiiContainerMagic = 0;
  Reader.ReadSwapped(0x60, &WiiContainerMagic);
  if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic != 0)
    return DISC_TYPE_WII;
  if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic == 0)
    return DISC_TYPE_WII_CONTAINER;

  // Check for WAD
  // 0x206962 for boot2 wads
  u32 WADMagic = 0;
  Reader.ReadSwapped(0x02, &WADMagic);
  if (WADMagic == 0x00204973 || WADMagic == 0x00206962)
    return DISC_TYPE_WAD;

  // Check for GC
  u32 GCMagic = 0;
  Reader.ReadSwapped(0x1C, &GCMagic);
  if (GCMagic == 0xC2339F3D)
    return DISC_TYPE_GC;

  // No known magic words found
  return DISC_TYPE_UNK;
}

}  // namespace
