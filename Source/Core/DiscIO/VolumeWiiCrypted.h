// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <mbedtls/aes.h>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for encrypted Wii images ---

namespace DiscIO
{
class IBlobReader;
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;

class CVolumeWiiCrypted : public IVolume
{
public:
  CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader, u64 _VolumeOffset,
                    const unsigned char* _pVolumeKey);
  ~CVolumeWiiCrypted();
  bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const override;
  bool GetTitleID(u64* buffer) const override;
  std::vector<u8> GetTMD() const override;
  u64 PartitionOffsetToRawOffset(u64 offset) const override;
  std::string GetGameID() const override;
  std::string GetMakerID() const override;
  u16 GetRevision() const override;
  std::string GetInternalName() const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  u64 GetFSTSize() const override;
  std::string GetApploaderDate() const override;
  u8 GetDiscNumber() const override;

  Platform GetVolumeType() const override;
  bool SupportsIntegrityCheck() const override { return true; }
  bool CheckIntegrity() const override;
  bool ChangePartition(u64 offset) override;

  Region GetRegion() const override;
  Country GetCountry() const override;
  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  u64 GetRawSize() const override;

  static constexpr unsigned int BLOCK_HEADER_SIZE = 0x0400;
  static constexpr unsigned int BLOCK_DATA_SIZE = 0x7C00;
  static constexpr unsigned int BLOCK_TOTAL_SIZE = BLOCK_HEADER_SIZE + BLOCK_DATA_SIZE;

private:
  std::unique_ptr<IBlobReader> m_pReader;
  std::unique_ptr<mbedtls_aes_context> m_AES_ctx;

  u64 m_VolumeOffset;
  u64 m_dataOffset;

  mutable u64 m_LastDecryptedBlockOffset;
  mutable unsigned char m_LastDecryptedBlock[BLOCK_DATA_SIZE];
};

}  // namespace
