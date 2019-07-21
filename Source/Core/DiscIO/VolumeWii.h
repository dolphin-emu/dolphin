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
#include "Common/Lazy.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
class BlobReader;
enum class BlobType;
enum class Country;
class FileSystem;
enum class Language;
enum class Region;
enum class Platform;

class VolumeWii : public VolumeDisc
{
public:
  VolumeWii(std::unique_ptr<BlobReader> reader);
  ~VolumeWii();
  bool Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const override;
  bool IsEncryptedAndHashed() const override;
  std::vector<Partition> GetPartitions() const override;
  Partition GetGamePartition() const override;
  std::optional<u32> GetPartitionType(const Partition& partition) const override;
  std::optional<u64> GetTitleID(const Partition& partition) const override;
  const IOS::ES::TicketReader& GetTicket(const Partition& partition) const override;
  const IOS::ES::TMDReader& GetTMD(const Partition& partition) const override;
  const std::vector<u8>& GetCertificateChain(const Partition& partition) const override;
  const FileSystem* GetFileSystem(const Partition& partition) const override;
  static u64 EncryptedPartitionOffsetToRawOffset(u64 offset, const Partition& partition,
                                                 u64 partition_data_offset);
  u64 PartitionOffsetToRawOffset(u64 offset, const Partition& partition) const override;
  std::string GetGameID(const Partition& partition = PARTITION_NONE) const override;
  std::string GetGameTDBID(const Partition& partition = PARTITION_NONE) const override;
  std::string GetMakerID(const Partition& partition = PARTITION_NONE) const override;
  std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const override;
  std::string GetInternalName(const Partition& partition = PARTITION_NONE) const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(u32* width, u32* height) const override;
  std::string GetApploaderDate(const Partition& partition) const override;
  std::optional<u8> GetDiscNumber(const Partition& partition = PARTITION_NONE) const override;

  Platform GetVolumeType() const override;
  bool SupportsIntegrityCheck() const override { return m_encrypted; }
  bool CheckH3TableIntegrity(const Partition& partition) const override;
  bool CheckBlockIntegrity(u64 block_index, const Partition& partition) const override;

  Region GetRegion() const override;
  Country GetCountry(const Partition& partition = PARTITION_NONE) const override;
  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  bool IsSizeAccurate() const override;
  u64 GetRawSize() const override;

  static constexpr unsigned int H3_TABLE_SIZE = 0x18000;

  static constexpr unsigned int BLOCK_HEADER_SIZE = 0x0400;
  static constexpr unsigned int BLOCK_DATA_SIZE = 0x7C00;
  static constexpr unsigned int BLOCK_TOTAL_SIZE = BLOCK_HEADER_SIZE + BLOCK_DATA_SIZE;

protected:
  u32 GetOffsetShift() const override { return 2; }

private:
  struct PartitionDetails
  {
    Common::Lazy<std::unique_ptr<mbedtls_aes_context>> key;
    Common::Lazy<IOS::ES::TicketReader> ticket;
    Common::Lazy<IOS::ES::TMDReader> tmd;
    Common::Lazy<std::vector<u8>> cert_chain;
    Common::Lazy<std::vector<u8>> h3_table;
    Common::Lazy<std::unique_ptr<FileSystem>> file_system;
    Common::Lazy<u64> data_offset;
    u32 type;
  };

  std::unique_ptr<BlobReader> m_reader;
  std::map<Partition, PartitionDetails> m_partitions;
  Partition m_game_partition;
  bool m_encrypted;

  mutable u64 m_last_decrypted_block;
  mutable u8 m_last_decrypted_block_data[BLOCK_DATA_SIZE];
};

}  // namespace DiscIO
