// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for Wad files ---
// Some of this code might look redundant with the CNANDContentLoader class, however,
// We do not do any decryption here, we do raw read, so things are -Faster-

namespace DiscIO
{
class IBlobReader;
enum class BlobType;
enum class Country;
enum class Language;
enum class Region;
enum class Platform;

class CVolumeWAD : public IVolume
{
public:
  CVolumeWAD(std::unique_ptr<IBlobReader> reader);
  ~CVolumeWAD();
  bool Read(u64 offset, u64 length, u8* buffer,
            const Partition& partition = PARTITION_NONE) const override;
  std::optional<u64> GetTitleID(const Partition& partition = PARTITION_NONE) const override;
  const IOS::ES::TMDReader& GetTMD(const Partition& partition = PARTITION_NONE) const override;
  std::string GetGameID(const Partition& partition = PARTITION_NONE) const override;
  std::string GetMakerID(const Partition& partition = PARTITION_NONE) const override;
  u16 GetRevision(const Partition& partition = PARTITION_NONE) const override;
  std::string GetInternalName(const Partition& partition = PARTITION_NONE) const override
  {
    return "";
  }
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  std::string GetApploaderDate(const Partition& partition = PARTITION_NONE) const override
  {
    return "";
  }
  Platform GetVolumeType() const override;
  Region GetRegion() const override;
  Country GetCountry(const Partition& partition = PARTITION_NONE) const override;

  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  u64 GetRawSize() const override;

private:
  std::unique_ptr<IBlobReader> m_reader;
  IOS::ES::TMDReader m_tmd;
  u32 m_offset = 0;
  u32 m_tmd_offset = 0;
  u32 m_opening_bnr_offset = 0;
  u32 m_hdr_size = 0;
  u32 m_cert_size = 0;
  u32 m_tick_size = 0;
  u32 m_tmd_size = 0;
  u32 m_data_size = 0;
};

}  // namespace
