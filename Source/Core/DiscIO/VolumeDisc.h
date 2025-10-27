// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
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
    std::array<uint32_t, 3> unknown0;
    std::array<uint32_t, 4> unknown1;
    std::array<char, 4> mediaboard_type;  // 0x20 (GCAM for Triforce)
    uint32_t unknown2;
    uint16_t year;       // 0x28
    uint8_t month;       // 0x2A
    uint8_t day;         // 0x2B
    uint8_t video_mode;  // 0x2C unknown bitmask, resolutions + horizontal/vertical
    uint8_t unknown3;
    uint8_t type_3_compatible;  // 0x2E (Type-3 compatible titles have this set to 1)
    uint8_t unknown4;
    std::array<char, 8> game_id;  // 0x30
    uint32_t region_flags;        // 0x38
    std::array<uint32_t, 9> unknown6;
    std::array<char, 0x20> manufacturer;                 // 0x60
    std::array<char, 0x20> game_name;                    // 0x80
    std::array<char, 0x20> game_executable;              // 0xA0
    std::array<char, 0x20> test_executable;              // 0xC0
    std::array<std::array<char, 0x20>, 8> credit_types;  // 0xE0
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

static_assert(sizeof(VolumeDisc::BootID) == 480);

}  // namespace DiscIO
