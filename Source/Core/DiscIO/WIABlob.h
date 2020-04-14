// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <limits>
#include <map>
#include <memory>
#include <utility>

#include <bzlib.h>
#include <lzma.h>
#include <mbedtls/sha1.h>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Swap.h"
#include "DiscIO/Blob.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
class VolumeDisc;

constexpr u32 WIA_MAGIC = 0x01414957;  // "WIA\x1" (byteswapped to little endian)

class WIAFileReader : public BlobReader
{
public:
  ~WIAFileReader();

  static std::unique_ptr<WIAFileReader> Create(File::IOFile file, const std::string& path);

  BlobType GetBlobType() const override { return BlobType::WIA; }

  u64 GetRawSize() const override { return Common::swap64(m_header_1.wia_file_size); }
  u64 GetDataSize() const override { return Common::swap64(m_header_1.iso_file_size); }
  bool IsDataSizeAccurate() const override { return true; }

  u64 GetBlockSize() const override { return Common::swap32(m_header_2.chunk_size); }
  bool HasFastRandomAccessInBlock() const override { return false; }

  bool Read(u64 offset, u64 size, u8* out_ptr) override;
  bool SupportsReadWiiDecrypted() const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset) override;

  enum class ConversionResult
  {
    Success,
    Canceled,
    ReadFailed,
    WriteFailed,
    InternalError,
  };

  static ConversionResult ConvertToWIA(BlobReader* infile, const VolumeDisc* infile_volume,
                                       File::IOFile* outfile, int chunk_size, CompressCB callback,
                                       void* arg);

private:
  using SHA1 = std::array<u8, 20>;
  using WiiKey = std::array<u8, 16>;

  enum class CompressionType : u32
  {
    None = 0,
    Purge = 1,
    Bzip2 = 2,
    LZMA = 3,
    LZMA2 = 4,
  };

  // See docs/WIA.md for details about the format

#pragma pack(push, 1)
  struct WIAHeader1
  {
    u32 magic;
    u32 version;
    u32 version_compatible;
    u32 header_2_size;
    SHA1 header_2_hash;
    u64 iso_file_size;
    u64 wia_file_size;
    SHA1 header_1_hash;
  };
  static_assert(sizeof(WIAHeader1) == 0x48, "Wrong size for WIA header 1");

  struct WIAHeader2
  {
    u32 disc_type;
    u32 compression_type;
    u32 compression_level;  // Informative only
    u32 chunk_size;

    std::array<u8, 0x80> disc_header;

    u32 number_of_partition_entries;
    u32 partition_entry_size;
    u64 partition_entries_offset;
    SHA1 partition_entries_hash;

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

  struct GroupEntry
  {
    u32 data_offset;  // >> 2
    u32 data_size;
  };
  static_assert(sizeof(GroupEntry) == 0x08, "Wrong size for WIA group entry");

  struct HashExceptionEntry
  {
    u16 offset;
    SHA1 hash;
  };
  static_assert(sizeof(HashExceptionEntry) == 0x16, "Wrong size for WIA hash exception entry");

  struct PurgeSegment
  {
    u32 offset;
    u32 size;
  };
  static_assert(sizeof(PurgeSegment) == 0x08, "Wrong size for WIA purge segment");
#pragma pack(pop)

  struct DataEntry
  {
    u32 index;
    bool is_partition;
    u8 partition_data_index;

    DataEntry(size_t index_) : index(static_cast<u32>(index_)), is_partition(false) {}
    DataEntry(size_t index_, size_t partition_data_index_)
        : index(static_cast<u32>(index_)), is_partition(true),
          partition_data_index(static_cast<u8>(partition_data_index_))
    {
    }
  };

  struct DecompressionBuffer
  {
    std::vector<u8> data;
    size_t bytes_written = 0;
  };

  class Decompressor
  {
  public:
    virtual ~Decompressor();

    virtual bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                            size_t* in_bytes_read) = 0;
    virtual bool Done() const { return m_done; };

  protected:
    bool m_done = false;
  };

  class NoneDecompressor final : public Decompressor
  {
  public:
    bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                    size_t* in_bytes_read) override;
  };

  // This class assumes that more bytes won't be added to in once in.bytes_written == in.data.size()
  // and that *in_bytes_read initially will be equal to the size of the exception lists
  class PurgeDecompressor final : public Decompressor
  {
  public:
    PurgeDecompressor(u64 decompressed_size);
    bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                    size_t* in_bytes_read) override;

  private:
    const u64 m_decompressed_size;

    PurgeSegment m_segment = {};
    size_t m_bytes_read = 0;
    size_t m_segment_bytes_written = 0;
    size_t m_out_bytes_written = 0;
    bool m_started = false;

    mbedtls_sha1_context m_sha1_context;
  };

  class Bzip2Decompressor final : public Decompressor
  {
  public:
    ~Bzip2Decompressor();

    bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                    size_t* in_bytes_read) override;

  private:
    bz_stream m_stream = {};
    bool m_started = false;
  };

  class LZMADecompressor final : public Decompressor
  {
  public:
    LZMADecompressor(bool lzma2, const u8* filter_options, size_t filter_options_size);
    ~LZMADecompressor();

    bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                    size_t* in_bytes_read) override;

  private:
    lzma_stream m_stream = LZMA_STREAM_INIT;
    lzma_options_lzma m_options = {};
    lzma_filter m_filters[2];
    bool m_started = false;
    bool m_error_occurred = false;
  };

  class Chunk
  {
  public:
    Chunk();
    Chunk(File::IOFile* file, u64 offset_in_file, u64 compressed_size, u64 decompressed_size,
          u32 exception_lists, bool compressed_exception_lists,
          std::unique_ptr<Decompressor> decompressor);

    bool Read(u64 offset, u64 size, u8* out_ptr);

    // This can only be called once at least one byte of data has been read
    bool ApplyHashExceptions(VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP],
                             u64 exception_list_index) const;

    template <typename T>
    bool ReadAll(std::vector<T>* vector)
    {
      return Read(0, vector->size() * sizeof(T), reinterpret_cast<u8*>(vector->data()));
    }

  private:
    bool HandleExceptions(const u8* data, size_t bytes_allocated, size_t bytes_written,
                          size_t* bytes_used, bool align);

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
  };

  explicit WIAFileReader(File::IOFile file, const std::string& path);
  bool Initialize(const std::string& path);
  bool HasDataOverlap() const;

  bool ReadFromGroups(u64* offset, u64* size, u8** out_ptr, u64 chunk_size, u32 sector_size,
                      u64 data_offset, u64 data_size, u32 group_index, u32 number_of_groups,
                      u32 exception_lists);
  Chunk& ReadCompressedData(u64 offset_in_file, u64 compressed_size, u64 decompressed_size,
                            u32 exception_lists);

  static std::string VersionToString(u32 version);

  static bool PadTo4(File::IOFile* file, u64* bytes_written);
  static void AddRawDataEntry(u64 offset, u64 size, int chunk_size, u32* total_groups,
                              std::vector<RawDataEntry>* raw_data_entries,
                              std::vector<DataEntry>* data_entries);
  static PartitionDataEntry
  CreatePartitionDataEntry(u64 offset, u64 size, u32 index, int chunk_size, u32* total_groups,
                           const std::vector<PartitionEntry>& partition_entries,
                           std::vector<DataEntry>* data_entries);
  static ConversionResult SetUpDataEntriesForWriting(const VolumeDisc* volume, int chunk_size,
                                                     u64 iso_size, u32* total_groups,
                                                     std::vector<PartitionEntry>* partition_entries,
                                                     std::vector<RawDataEntry>* raw_data_entries,
                                                     std::vector<DataEntry>* data_entries);

  bool m_valid;
  CompressionType m_compression_type;

  File::IOFile m_file;
  Chunk m_cached_chunk;
  u64 m_cached_chunk_offset = std::numeric_limits<u64>::max();
  WiiEncryptionCache m_encryption_cache;

  WIAHeader1 m_header_1;
  WIAHeader2 m_header_2;
  std::vector<PartitionEntry> m_partition_entries;
  std::vector<RawDataEntry> m_raw_data_entries;
  std::vector<GroupEntry> m_group_entries;

  std::map<u64, DataEntry> m_data_entries;

  static constexpr u32 WIA_VERSION = 0x01000000;
  static constexpr u32 WIA_VERSION_WRITE_COMPATIBLE = 0x01000000;
  static constexpr u32 WIA_VERSION_READ_COMPATIBLE = 0x00080000;

  // Perhaps we could set WIA_VERSION_WRITE_COMPATIBLE to 0.9, but WIA version 0.9 was never in
  // any official release of wit, and interim versions (either source or binaries) are hard to find.
  // Since we've been unable to check if we're write compatible with 0.9, we set it 1.0 to be safe.
};

}  // namespace DiscIO
