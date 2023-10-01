// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/IOFile.h"
#include "Common/Swap.h"
#include "DiscIO/Blob.h"
#include "DiscIO/MultithreadedCompressor.h"
#include "DiscIO/WIACompression.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
class FileSystem;
class VolumeDisc;

enum class WIARVZCompressionType : u32
{
  None = 0,
  Purge = 1,
  Bzip2 = 2,
  LZMA = 3,
  LZMA2 = 4,
  Zstd = 5,
};

std::pair<int, int> GetAllowedCompressionLevels(WIARVZCompressionType compression_type, bool gui);

constexpr u32 WIA_MAGIC = 0x01414957;  // "WIA\x1" (byteswapped to little endian)
constexpr u32 RVZ_MAGIC = 0x015A5652;  // "RVZ\x1" (byteswapped to little endian)

template <bool RVZ>
class WIARVZFileReader : public BlobReader
{
public:
  ~WIARVZFileReader();

  static std::unique_ptr<WIARVZFileReader> Create(File::IOFile file, const std::string& path);

  BlobType GetBlobType() const override;
  std::unique_ptr<BlobReader> CopyReader() const override;

  u64 GetRawSize() const override { return Common::swap64(m_header_1.wia_file_size); }
  u64 GetDataSize() const override { return Common::swap64(m_header_1.iso_file_size); }
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return Common::swap32(m_header_2.chunk_size); }
  bool HasFastRandomAccessInBlock() const override { return false; }
  std::string GetCompressionMethod() const override;
  std::optional<int> GetCompressionLevel() const override
  {
    return static_cast<int>(static_cast<s32>(Common::swap32(m_header_2.compression_level)));
  }

  bool Read(u64 offset, u64 size, u8* out_ptr) override;
  bool SupportsReadWiiDecrypted(u64 offset, u64 size, u64 partition_data_offset) const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset) override;

  static ConversionResultCode Convert(BlobReader* infile, const VolumeDisc* infile_volume,
                                      File::IOFile* outfile, WIARVZCompressionType compression_type,
                                      int compression_level, int chunk_size, CompressCB callback);

private:
  using WiiKey = std::array<u8, 16>;

  // See docs/WiaAndRvz.md for details about the format

#pragma pack(push, 1)
  struct WIAHeader1
  {
    u32 magic;
    u32 version;
    u32 version_compatible;
    u32 header_2_size;
    Common::SHA1::Digest header_2_hash;
    u64 iso_file_size;
    u64 wia_file_size;
    Common::SHA1::Digest header_1_hash;
  };
  static_assert(sizeof(WIAHeader1) == 0x48, "Wrong size for WIA header 1");

  struct WIAHeader2
  {
    u32 disc_type;
    u32 compression_type;
    s32 compression_level;  // Informative only
    u32 chunk_size;

    std::array<u8, 0x80> disc_header;

    u32 number_of_partition_entries;
    u32 partition_entry_size;
    u64 partition_entries_offset;
    Common::SHA1::Digest partition_entries_hash;

    u32 number_of_raw_data_entries;
    u64 raw_data_entries_offset;
    u32 raw_data_entries_size;

    u32 number_of_group_entries;
    u64 group_entries_offset;
    u32 group_entries_size;

    u8 compressor_data_size;
    u8 compressor_data[7];
  };
  static_assert(sizeof(WIAHeader2) == 0xdc, "Wrong size for WIA header 2");

  struct PartitionDataEntry
  {
    u32 first_sector;
    u32 number_of_sectors;
    u32 group_index;
    u32 number_of_groups;
  };
  static_assert(sizeof(PartitionDataEntry) == 0x10, "Wrong size for WIA partition data entry");

  struct PartitionEntry
  {
    WiiKey partition_key;
    std::array<PartitionDataEntry, 2> data_entries;
  };
  static_assert(sizeof(PartitionEntry) == 0x30, "Wrong size for WIA partition entry");

  struct RawDataEntry
  {
    u64 data_offset;
    u64 data_size;
    u32 group_index;
    u32 number_of_groups;
  };
  static_assert(sizeof(RawDataEntry) == 0x18, "Wrong size for WIA raw data entry");

  struct WIAGroupEntry
  {
    u32 data_offset;  // >> 2
    u32 data_size;
  };
  static_assert(sizeof(WIAGroupEntry) == 0x08, "Wrong size for WIA group entry");

  struct RVZGroupEntry
  {
    u32 data_offset;  // >> 2
    u32 data_size;
    u32 rvz_packed_size;
  };
  static_assert(sizeof(RVZGroupEntry) == 0x0c, "Wrong size for RVZ group entry");

  using GroupEntry = std::conditional_t<RVZ, RVZGroupEntry, WIAGroupEntry>;

  struct HashExceptionEntry
  {
    u16 offset;
    Common::SHA1::Digest hash;
  };
  static_assert(sizeof(HashExceptionEntry) == 0x16, "Wrong size for WIA hash exception entry");
#pragma pack(pop)

  struct DataEntry
  {
    u32 index;
    bool is_partition;
    u8 partition_data_index;

    DataEntry(size_t index_)
        : index(static_cast<u32>(index_)), is_partition(false), partition_data_index(0)
    {
    }
    DataEntry(size_t index_, size_t partition_data_index_)
        : index(static_cast<u32>(index_)), is_partition(true),
          partition_data_index(static_cast<u8>(partition_data_index_))
    {
    }
  };

  class Chunk
  {
  public:
    Chunk();
    Chunk(File::IOFile* file, u64 offset_in_file, u64 compressed_size, u64 decompressed_size,
          u32 exception_lists, bool compressed_exception_lists, u32 rvz_packed_size,
          u64 data_offset, std::unique_ptr<Decompressor> decompressor);

    bool Read(u64 offset, u64 size, u8* out_ptr);

    // This can only be called once at least one byte of data has been read
    void GetHashExceptions(std::vector<HashExceptionEntry>* exception_list,
                           u64 exception_list_index, u16 additional_offset) const;

    template <typename T>
    bool ReadAll(std::vector<T>* vector)
    {
      return Read(0, vector->size() * sizeof(T), reinterpret_cast<u8*>(vector->data()));
    }

  private:
    bool Decompress();
    bool HandleExceptions(const u8* data, size_t bytes_allocated, size_t bytes_written,
                          size_t* bytes_used, bool align);

    size_t GetOutBytesWrittenExcludingExceptions() const;

    DecompressionBuffer m_in;
    DecompressionBuffer m_out;
    size_t m_in_bytes_read = 0;

    std::unique_ptr<Decompressor> m_decompressor = nullptr;
    File::IOFile* m_file = nullptr;
    u64 m_offset_in_file = 0;

    size_t m_out_bytes_allocated_for_exceptions = 0;
    size_t m_out_bytes_used_for_exceptions = 0;
    size_t m_in_bytes_used_for_exceptions = 0;
    u32 m_exception_lists = 0;
    bool m_compressed_exception_lists = false;
    u32 m_rvz_packed_size = 0;
    u64 m_data_offset = 0;
  };

  explicit WIARVZFileReader(File::IOFile file, const std::string& path);
  bool Initialize(const std::string& path);
  bool HasDataOverlap() const;

  const PartitionEntry* GetPartition(u64 partition_data_offset, u32* partition_first_sector) const;

  bool ReadFromGroups(u64* offset, u64* size, u8** out_ptr, u64 chunk_size, u32 sector_size,
                      u64 data_offset, u64 data_size, u32 group_index, u32 number_of_groups,
                      u32 exception_lists);
  Chunk& ReadCompressedData(u64 offset_in_file, u64 compressed_size, u64 decompressed_size,
                            WIARVZCompressionType compression_type, u32 exception_lists = 0,
                            u32 rvz_packed_size = 0, u64 data_offset = 0);

  static bool ApplyHashExceptions(const std::vector<HashExceptionEntry>& exception_list,
                                  VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP]);

  static std::string VersionToString(u32 version);

  struct ReuseID
  {
    bool operator==(const ReuseID& other) const
    {
      return std::tie(partition_key, data_size, encrypted, value) ==
             std::tie(other.partition_key, other.data_size, other.encrypted, other.value);
    }
    bool operator<(const ReuseID& other) const
    {
      return std::tie(partition_key, data_size, encrypted, value) <
             std::tie(other.partition_key, other.data_size, other.encrypted, other.value);
    }
    bool operator>(const ReuseID& other) const
    {
      return std::tie(partition_key, data_size, encrypted, value) >
             std::tie(other.partition_key, other.data_size, other.encrypted, other.value);
    }

    bool operator!=(const ReuseID& other) const { return !operator==(other); }
    bool operator>=(const ReuseID& other) const { return !operator<(other); }
    bool operator<=(const ReuseID& other) const { return !operator>(other); }

    WiiKey partition_key;
    u64 data_size;
    bool encrypted;
    u8 value;
  };

  struct CompressThreadState
  {
    using WiiBlockData = std::array<u8, VolumeWii::BLOCK_DATA_SIZE>;

    std::unique_ptr<Compressor> compressor;

    std::vector<WiiBlockData> decryption_buffer =
        std::vector<WiiBlockData>(VolumeWii::BLOCKS_PER_GROUP);

    std::vector<VolumeWii::HashBlock> hash_buffer =
        std::vector<VolumeWii::HashBlock>(VolumeWii::BLOCKS_PER_GROUP);
  };

  struct CompressParameters
  {
    std::vector<u8> data{};
    const DataEntry* data_entry = nullptr;
    u64 data_offset = 0;
    u64 bytes_read = 0;
    size_t group_index = 0;
  };

  struct WIAOutputParametersEntry
  {
    std::vector<u8> exception_lists;
    std::vector<u8> main_data;
    std::optional<ReuseID> reuse_id;
    std::optional<GroupEntry> reused_group;
  };

  struct RVZOutputParametersEntry
  {
    std::vector<u8> exception_lists;
    std::vector<u8> main_data;
    std::optional<ReuseID> reuse_id;
    std::optional<GroupEntry> reused_group;
    size_t rvz_packed_size = 0;
    bool compressed = false;
  };

  using OutputParametersEntry =
      std::conditional_t<RVZ, RVZOutputParametersEntry, WIAOutputParametersEntry>;

  struct OutputParameters
  {
    std::vector<OutputParametersEntry> entries;
    u64 bytes_read = 0;
    size_t group_index = 0;
  };

  static bool PadTo4(File::IOFile* file, u64* bytes_written);
  static void AddRawDataEntry(u64 offset, u64 size, int chunk_size, u32* total_groups,
                              std::vector<RawDataEntry>* raw_data_entries,
                              std::vector<DataEntry>* data_entries);
  static PartitionDataEntry
  CreatePartitionDataEntry(u64 offset, u64 size, u32 index, int chunk_size, u32* total_groups,
                           const std::vector<PartitionEntry>& partition_entries,
                           std::vector<DataEntry>* data_entries);
  static ConversionResultCode SetUpDataEntriesForWriting(
      const VolumeDisc* volume, int chunk_size, u64 iso_size, u32* total_groups,
      std::vector<PartitionEntry>* partition_entries, std::vector<RawDataEntry>* raw_data_entries,
      std::vector<DataEntry>* data_entries, std::vector<const FileSystem*>* partition_file_systems);
  static std::optional<std::vector<u8>> Compress(Compressor* compressor, const u8* data,
                                                 size_t size);
  static bool WriteHeader(File::IOFile* file, const u8* data, size_t size, u64 upper_bound,
                          u64* bytes_written, u64* offset_out);

  static void SetUpCompressor(std::unique_ptr<Compressor>* compressor,
                              WIARVZCompressionType compression_type, int compression_level,
                              WIAHeader2* header_2);
  static bool TryReuse(std::map<ReuseID, GroupEntry>* reusable_groups,
                       std::mutex* reusable_groups_mutex, OutputParametersEntry* entry);
  static ConversionResult<OutputParameters>
  ProcessAndCompress(CompressThreadState* state, CompressParameters parameters,
                     const std::vector<PartitionEntry>& partition_entries,
                     const std::vector<DataEntry>& data_entries, const FileSystem* file_system,
                     std::map<ReuseID, GroupEntry>* reusable_groups,
                     std::mutex* reusable_groups_mutex, u64 chunks_per_wii_group,
                     u64 exception_lists_per_chunk, bool compressed_exception_lists,
                     bool compression);
  static ConversionResultCode Output(std::vector<OutputParametersEntry>* entries,
                                     File::IOFile* outfile,
                                     std::map<ReuseID, GroupEntry>* reusable_groups,
                                     std::mutex* reusable_groups_mutex, GroupEntry* group_entry,
                                     u64* bytes_written);
  static ConversionResultCode RunCallback(size_t groups_written, u64 bytes_read, u64 bytes_written,
                                          u32 total_groups, u64 iso_size, CompressCB callback);

  bool m_valid;
  WIARVZCompressionType m_compression_type;

  File::IOFile m_file;
  std::string m_path;
  Chunk m_cached_chunk;
  u64 m_cached_chunk_offset = std::numeric_limits<u64>::max();
  WiiEncryptionCache m_encryption_cache;

  std::vector<HashExceptionEntry> m_exception_list;
  bool m_write_to_exception_list = false;
  u64 m_exception_list_last_group_index;

  WIAHeader1 m_header_1;
  WIAHeader2 m_header_2;
  std::vector<PartitionEntry> m_partition_entries;
  std::vector<RawDataEntry> m_raw_data_entries;
  std::vector<GroupEntry> m_group_entries;

  std::map<u64, DataEntry> m_data_entries;

  // Perhaps we could set WIA_VERSION_WRITE_COMPATIBLE to 0.9, but WIA version 0.9 was never in
  // any official release of wit, and interim versions (either source or binaries) are hard to find.
  // Since we've been unable to check if we're write compatible with 0.9, we set it 1.0 to be safe.

  static constexpr u32 WIA_VERSION = 0x01000000;
  static constexpr u32 WIA_VERSION_WRITE_COMPATIBLE = 0x01000000;
  static constexpr u32 WIA_VERSION_READ_COMPATIBLE = 0x00080000;

  static constexpr u32 RVZ_VERSION = 0x01000000;
  static constexpr u32 RVZ_VERSION_WRITE_COMPATIBLE = 0x00030000;
  static constexpr u32 RVZ_VERSION_READ_COMPATIBLE = 0x00030000;
};

using WIAFileReader = WIARVZFileReader<false>;
using RVZFileReader = WIARVZFileReader<true>;

}  // namespace DiscIO
