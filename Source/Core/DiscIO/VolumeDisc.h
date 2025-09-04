// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
class VolumeDisc : public Volume
{
public:
  struct BootID
  {
    u32 magic;  // 0x00 (Always BTID)
    uint32_t unknown0[3];
    uint32_t unknown1[4];
    char mediaboardType[4];  // 0x20 (GCAM for Triforce)
    uint32_t unknown2;
    uint16_t year;      // 0x28
    uint8_t month;      // 0x2A
    uint8_t day;        // 0x2B
    uint8_t videoMode;  // 0x2C unknown bitmask, resolutions + horizontal/vertical
    uint8_t unknown3;
    uint8_t type3Compatible;  // 0x2E (Type-3 compatible titles have this set to 1)
    uint8_t unknown4;
    char gameId[8];        // 0x30
    uint32_t regionFlags;  // 0x38
    uint32_t unknown6[9];
    char manufacturer[0x20];    // 0x60
    char gameName[0x20];        // 0x80
    char gameExecutable[0x20];  // 0xA0
    char testExecutable[0x20];  // 0xC0
    char creditTypes[8][0x20];  // 0xE0
  };

  std::string GetGameID(const Partition& partition = PARTITION_NONE) const override;
  Country GetCountry(const Partition& partition = PARTITION_NONE) const override;
  std::string GetMakerID(const Partition& partition = PARTITION_NONE) const override;
  std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const override;
  std::string GetInternalName(const Partition& partition = PARTITION_NONE) const override;
  std::string GetApploaderDate(const Partition& partition) const override;
  std::optional<u8> GetDiscNumber(const Partition& partition = PARTITION_NONE) const override;
  bool IsNKit() const override;

protected:
  Region RegionCodeToRegion(std::optional<u32> region_code) const;
  void AddGamePartitionToSyncHash(Common::SHA1::Context* context) const;
};

}  // namespace DiscIO
