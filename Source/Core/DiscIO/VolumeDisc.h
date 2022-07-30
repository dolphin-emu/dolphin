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
