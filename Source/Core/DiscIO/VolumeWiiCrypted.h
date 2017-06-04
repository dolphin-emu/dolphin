// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <mbedtls/aes.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/ES/Formats.h"
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
  CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader);
  ~CVolumeWiiCrypted();
  bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, const Partition& partition) const override;
  std::vector<Partition> GetPartitions() const override;
  Partition GetGamePartition() const override;
  std::optional<u64> GetTitleID(const Partition& partition) const override;
  const IOS::ES::TicketReader& GetTicket(const Partition& partition) const override;
  const IOS::ES::TMDReader& GetTMD(const Partition& partition) const override;
  std::string GetGameID(const Partition& partition) const override;
  std::string GetMakerID(const Partition& partition) const override;
  std::optional<u16> GetRevision(const Partition& partition) const override;
  std::string GetInternalName(const Partition& partition) const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(int* width, int* height) const override;
  std::string GetApploaderDate(const Partition& partition) const override;
  std::optional<u8> GetDiscNumber(const Partition& partition) const override;

  Platform GetVolumeType() const override;
  bool SupportsIntegrityCheck() const override { return true; }
  bool CheckIntegrity(const Partition& partition) const override;

  Region GetRegion() const override;
  Country GetCountry(const Partition& partition) const override;
  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  u64 GetRawSize() const override;

  static u64 PartitionOffsetToRawOffset(u64 offset, const Partition& partition);

  static constexpr unsigned int BLOCK_HEADER_SIZE = 0x0400;
  static constexpr unsigned int BLOCK_DATA_SIZE = 0x7C00;
  static constexpr unsigned int BLOCK_TOTAL_SIZE = BLOCK_HEADER_SIZE + BLOCK_DATA_SIZE;

private:
  std::unique_ptr<IBlobReader> m_pReader;
  std::map<Partition, std::unique_ptr<mbedtls_aes_context>> m_partition_keys;
  std::map<Partition, IOS::ES::TicketReader> m_partition_tickets;
  std::map<Partition, IOS::ES::TMDReader> m_partition_tmds;
  Partition m_game_partition;

  mutable u64 m_last_decrypted_block;
  mutable u8 m_last_decrypted_block_data[BLOCK_DATA_SIZE];
};

}  // namespace
