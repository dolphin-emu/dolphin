// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/VolumeWiiCrypted.h"

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
CVolumeWiiCrypted::CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader, u64 _VolumeOffset,
                                     const unsigned char* _pVolumeKey)
    : m_pReader(std::move(reader)), m_AES_ctx(std::make_unique<mbedtls_aes_context>()),
      m_VolumeOffset(_VolumeOffset), m_dataOffset(0x20000), m_LastDecryptedBlockOffset(-1)
{
  _assert_(m_pReader);

  mbedtls_aes_setkey_dec(m_AES_ctx.get(), _pVolumeKey, 128);
}

bool CVolumeWiiCrypted::ChangePartition(u64 offset)
{
  m_VolumeOffset = offset;
  m_LastDecryptedBlockOffset = -1;

  u8 volume_key[16];
  DiscIO::VolumeKeyForPartition(*m_pReader, offset, volume_key);
  mbedtls_aes_setkey_dec(m_AES_ctx.get(), volume_key, 128);
  return true;
}

CVolumeWiiCrypted::~CVolumeWiiCrypted()
{
}

bool CVolumeWiiCrypted::Read(u64 _ReadOffset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
  if (!decrypt)
    return m_pReader->Read(_ReadOffset, _Length, _pBuffer);

  std::vector<u8> read_buffer(BLOCK_TOTAL_SIZE);
  while (_Length > 0)
  {
    // Calculate block offset
    u64 Block = _ReadOffset / BLOCK_DATA_SIZE;
    u64 Offset = _ReadOffset % BLOCK_DATA_SIZE;

    if (m_LastDecryptedBlockOffset != Block)
    {
      // Read the current block
      if (!m_pReader->Read(m_VolumeOffset + m_dataOffset + Block * BLOCK_TOTAL_SIZE,
                           BLOCK_TOTAL_SIZE, read_buffer.data()))
        return false;

      // Decrypt the block's data.
      // 0x3D0 - 0x3DF in m_pBuffer will be overwritten,
      // but that won't affect anything, because we won't
      // use the content of m_pBuffer anymore after this
      mbedtls_aes_crypt_cbc(m_AES_ctx.get(), MBEDTLS_AES_DECRYPT, BLOCK_DATA_SIZE,
                            &read_buffer[0x3D0], &read_buffer[BLOCK_HEADER_SIZE],
                            m_LastDecryptedBlock);
      m_LastDecryptedBlockOffset = Block;

      // The only thing we currently use from the 0x000 - 0x3FF part
      // of the block is the IV (at 0x3D0), but it also contains SHA-1
      // hashes that IOS uses to check that discs aren't tampered with.
      // http://wiibrew.org/wiki/Wii_Disc#Encrypted
    }

    // Copy the decrypted data
    u64 MaxSizeToCopy = BLOCK_DATA_SIZE - Offset;
    u64 CopySize = (_Length > MaxSizeToCopy) ? MaxSizeToCopy : _Length;
    memcpy(_pBuffer, &m_LastDecryptedBlock[Offset], (size_t)CopySize);

    // Update offsets
    _Length -= CopySize;
    _pBuffer += CopySize;
    _ReadOffset += CopySize;
  }

  return true;
}

bool CVolumeWiiCrypted::GetTitleID(u64* buffer) const
{
  return ReadSwapped(m_VolumeOffset + 0x1DC, buffer, false);
}

IOS::ES::TicketReader CVolumeWiiCrypted::GetTicket() const
{
  std::vector<u8> buffer(0x2a4);
  Read(m_VolumeOffset, buffer.size(), buffer.data(), false);
  return IOS::ES::TicketReader{std::move(buffer)};
}

IOS::ES::TMDReader CVolumeWiiCrypted::GetTMD() const
{
  u32 tmd_size = 0;
  u32 tmd_address = 0;

  if (!ReadSwapped(m_VolumeOffset + 0x2a4, &tmd_size, false))
    return {};
  if (!ReadSwapped(m_VolumeOffset + 0x2a8, &tmd_address, false))
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
  if (!Read(m_VolumeOffset + tmd_address, tmd_size, buffer.data(), false))
    return {};

  return IOS::ES::TMDReader{std::move(buffer)};
}

u64 CVolumeWiiCrypted::PartitionOffsetToRawOffset(u64 offset) const
{
  return m_VolumeOffset + m_dataOffset + (offset / BLOCK_DATA_SIZE * BLOCK_TOTAL_SIZE) +
         (offset % BLOCK_DATA_SIZE);
}

std::string CVolumeWiiCrypted::GetGameID() const
{
  char ID[6];

  if (!Read(0, 6, (u8*)ID, true))
    return std::string();

  return DecodeString(ID);
}

Region CVolumeWiiCrypted::GetRegion() const
{
  u32 region_code;
  if (!ReadSwapped(0x4E000, &region_code, false))
    return Region::UNKNOWN_REGION;

  return static_cast<Region>(region_code);
}

Country CVolumeWiiCrypted::GetCountry() const
{
  u8 country_byte;
  if (!ReadSwapped(3, &country_byte, true))
    return Country::COUNTRY_UNKNOWN;

  const Region region = GetRegion();

  if (RegionSwitchWii(country_byte) != region)
    return TypicalCountryForRegion(region);

  return CountrySwitch(country_byte);
}

std::string CVolumeWiiCrypted::GetMakerID() const
{
  char makerID[2];

  if (!Read(0x4, 0x2, (u8*)&makerID, true))
    return std::string();

  return DecodeString(makerID);
}

u16 CVolumeWiiCrypted::GetRevision() const
{
  u8 revision;
  if (!ReadSwapped(7, &revision, true))
    return 0;

  return revision;
}

std::string CVolumeWiiCrypted::GetInternalName() const
{
  char name_buffer[0x60];
  if (Read(0x20, 0x60, (u8*)&name_buffer, true))
    return DecodeString(name_buffer);

  return "";
}

std::map<Language, std::string> CVolumeWiiCrypted::GetLongNames() const
{
  std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this));
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
  u32 size;

  if (!Read(0x428, 0x4, (u8*)&size, true))
    return 0;

  return (u64)Common::swap32(size) << 2;
}

std::string CVolumeWiiCrypted::GetApploaderDate() const
{
  char date[16];

  if (!Read(0x2440, 0x10, (u8*)&date, true))
    return std::string();

  return DecodeString(date);
}

Platform CVolumeWiiCrypted::GetVolumeType() const
{
  return Platform::WII_DISC;
}

u8 CVolumeWiiCrypted::GetDiscNumber() const
{
  u8 disc_number = 0;
  ReadSwapped(6, &disc_number, true);
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

bool CVolumeWiiCrypted::CheckIntegrity() const
{
  // Get partition data size
  u32 partSizeDiv4;
  Read(m_VolumeOffset + 0x2BC, 4, (u8*)&partSizeDiv4, false);
  u64 partDataSize = (u64)Common::swap32(partSizeDiv4) * 4;

  u32 nClusters = (u32)(partDataSize / 0x8000);
  for (u32 clusterID = 0; clusterID < nClusters; ++clusterID)
  {
    u64 clusterOff = m_VolumeOffset + m_dataOffset + (u64)clusterID * 0x8000;

    // Read and decrypt the cluster metadata
    u8 clusterMDCrypted[0x400];
    u8 clusterMD[0x400];
    u8 IV[16] = {0};
    if (!Read(clusterOff, 0x400, clusterMDCrypted, false))
    {
      WARN_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", clusterID);
      return false;
    }
    mbedtls_aes_crypt_cbc(m_AES_ctx.get(), MBEDTLS_AES_DECRYPT, 0x400, IV, clusterMDCrypted,
                          clusterMD);

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
    if (!Read((u64)clusterID * 0x7C00, 0x7C00, clusterData, true))
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
