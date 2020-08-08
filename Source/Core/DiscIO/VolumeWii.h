// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Common/Lazy.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDisc.h"

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
  static constexpr size_t AES_KEY_SIZE = 16;
  static constexpr size_t SHA1_SIZE = 20;

  static constexpr u32 H3_TABLE_SIZE = 0x18000;
  static constexpr u32 BLOCKS_PER_GROUP = 0x40;

  static constexpr u64 BLOCK_HEADER_SIZE = 0x0400;
  static constexpr u64 BLOCK_DATA_SIZE = 0x7C00;
  static constexpr u64 BLOCK_TOTAL_SIZE = BLOCK_HEADER_SIZE + BLOCK_DATA_SIZE;

  static constexpr u64 GROUP_HEADER_SIZE = BLOCK_HEADER_SIZE * BLOCKS_PER_GROUP;
  static constexpr u64 GROUP_DATA_SIZE = BLOCK_DATA_SIZE * BLOCKS_PER_GROUP;
  static constexpr u64 GROUP_TOTAL_SIZE = GROUP_HEADER_SIZE + GROUP_DATA_SIZE;

  struct HashBlock
  {
    u8 h0[31][SHA1_SIZE];
    u8 padding_0[20];
    u8 h1[8][SHA1_SIZE];
    u8 padding_1[32];
    u8 h2[8][SHA1_SIZE];
    u8 padding_2[32];
  };
  static_assert(sizeof(HashBlock) == BLOCK_HEADER_SIZE);

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
  std::string GetGameTDBID(const Partition& partition = PARTITION_NONE) const override;
  std::map<Language, std::string> GetLongNames() const override;
  std::vector<u32> GetBanner(u32* width, u32* height) const override;

  Platform GetVolumeType() const override;
  bool IsDatelDisc() const override;
  bool SupportsIntegrityCheck() const override { return m_encrypted; }
  bool CheckH3TableIntegrity(const Partition& partition) const override;
  bool CheckBlockIntegrity(u64 block_index, const std::vector<u8>& encrypted_data,
                           const Partition& partition) const override;
  bool CheckBlockIntegrity(u64 block_index, const Partition& partition) const override;

  Region GetRegion() const override;
  BlobType GetBlobType() const override;
  u64 GetSize() const override;
  bool IsSizeAccurate() const override;
  u64 GetRawSize() const override;
  const BlobReader& GetBlobReader() const override;

  // The in parameter can either contain all the data to begin with,
  // or read_function can write data into the in parameter when called.
  // The latter lets reading run in parallel with hashing.
  // This function returns false iff read_function returns false.
  static bool HashGroup(const std::array<u8, BLOCK_DATA_SIZE> in[BLOCKS_PER_GROUP],
                        HashBlock out[BLOCKS_PER_GROUP],
                        const std::function<bool(size_t block)>& read_function = {});

  static bool EncryptGroup(u64 offset, u64 partition_data_offset, u64 partition_data_decrypted_size,
                           const std::array<u8, AES_KEY_SIZE>& key, BlobReader* blob,
                           std::array<u8, GROUP_TOTAL_SIZE>* out,
                           const std::function<void(HashBlock hash_blocks[BLOCKS_PER_GROUP])>&
                               hash_exception_callback = {});

  static void DecryptBlockHashes(const u8* in, HashBlock* out, mbedtls_aes_context* aes_context);
  static void DecryptBlockData(const u8* in, u8* out, mbedtls_aes_context* aes_context);

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
