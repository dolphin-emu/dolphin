// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace File
{
struct FSTEntry;
class IOFile;
}  // namespace File

namespace DiscIO
{
enum class PartitionType : u32;

class DirectoryBlobReader;
class VolumeDisc;

// Returns true if the path is inside a DirectoryBlob and doesn't represent the DirectoryBlob itself
bool ShouldHideFromGameList(const std::string& volume_path);

// Content chunk that is loaded from a file in the host file system.
struct ContentFile
{
  // Path where the file can be found.
  std::string m_filename;

  // Offset from the start of the file where the first byte of this content chunk is.
  u64 m_offset = 0;
};

// Content chunk that's just a direct block of memory
typedef std::shared_ptr<std::vector<u8>> ContentMemory;

// Content chunk that loads data from a DirectoryBlobReader.
// Intented for representing a partition within a disc.
struct ContentPartition
{
  // Offset from the start of the partition for the first byte represented by this chunk.
  u64 m_offset = 0;

  // The value passed as partition_data_offset to EncryptPartitionData().
  u64 m_partition_data_offset = 0;
};

// Content chunk that loads data from a Volume.
struct ContentVolume
{
  // Offset from the start of the volume for the first byte represented by this chunk.
  u64 m_offset = 0;

  // The partition passed to the Volume's Read() method.
  Partition m_partition;
};

// Content chunk representing a run of identical bytes.
// Useful for padding between chunks within a file.
struct ContentFixedByte
{
  u8 m_byte = 0;
};

using ContentSource = std::variant<ContentFile,       // File
                                   ContentMemory,     // Memory/Byte Sequence
                                   ContentPartition,  // Partition
                                   ContentVolume,     // Volume
                                   ContentFixedByte   // Fixed value padding
                                   >;

struct BuilderContentSource
{
  u64 m_offset = 0;
  u64 m_size = 0;
  ContentSource m_source;
};

struct FSTBuilderNode
{
  std::string m_filename;
  u64 m_size = 0;
  std::variant<std::vector<BuilderContentSource>, std::vector<FSTBuilderNode>> m_content;
  void* m_user_data = nullptr;

  bool IsFile() const
  {
    return std::holds_alternative<std::vector<BuilderContentSource>>(m_content);
  }

  std::vector<BuilderContentSource>& GetFileContent()
  {
    return std::get<std::vector<BuilderContentSource>>(m_content);
  }

  const std::vector<BuilderContentSource>& GetFileContent() const
  {
    return std::get<std::vector<BuilderContentSource>>(m_content);
  }

  bool IsFolder() const { return std::holds_alternative<std::vector<FSTBuilderNode>>(m_content); }

  std::vector<FSTBuilderNode>& GetFolderContent()
  {
    return std::get<std::vector<FSTBuilderNode>>(m_content);
  }

  const std::vector<FSTBuilderNode>& GetFolderContent() const
  {
    return std::get<std::vector<FSTBuilderNode>>(m_content);
  }
};

class DiscContent
{
public:
  DiscContent(u64 offset, u64 size, ContentSource source);

  // Provided because it's convenient when searching for DiscContent in an std::set
  explicit DiscContent(u64 offset);

  u64 GetOffset() const;
  u64 GetEndOffset() const;
  u64 GetSize() const;
  bool Read(u64* offset, u64* length, u8** buffer, DirectoryBlobReader* blob) const;

  bool operator==(const DiscContent& other) const { return GetEndOffset() == other.GetEndOffset(); }
  bool operator!=(const DiscContent& other) const { return !(*this == other); }
  bool operator<(const DiscContent& other) const { return GetEndOffset() < other.GetEndOffset(); }
  bool operator>(const DiscContent& other) const { return other < *this; }
  bool operator<=(const DiscContent& other) const { return !(*this > other); }
  bool operator>=(const DiscContent& other) const { return !(*this < other); }

private:
  // Position of this content chunk within its parent DiscContentContainer.
  u64 m_offset;

  // Number of bytes this content chunk takes up.
  u64 m_size = 0;

  // Where and how to find the data for this content chunk.
  ContentSource m_content_source;
};

class DiscContentContainer
{
public:
  void Add(u64 offset, std::vector<u8> vector)
  {
    size_t vector_size = vector.size();
    return Add(offset, vector_size, std::make_shared<std::vector<u8>>(std::move(vector)));
  }
  void Add(u64 offset, u64 size, ContentSource source);
  u64 CheckSizeAndAdd(u64 offset, const std::string& path);
  u64 CheckSizeAndAdd(u64 offset, u64 max_size, const std::string& path);

  bool Read(u64 offset, u64 length, u8* buffer, DirectoryBlobReader* blob) const;

private:
  std::set<DiscContent> m_contents;
};

class DirectoryBlobPartition
{
public:
  DirectoryBlobPartition() = default;
  DirectoryBlobPartition(const std::string& root_directory, std::optional<bool> is_wii);
  DirectoryBlobPartition(
      DiscIO::VolumeDisc* volume, const DiscIO::Partition& partition, std::optional<bool> is_wii,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
          fst_callback,
      DirectoryBlobReader* blob);

  DirectoryBlobPartition(const DirectoryBlobPartition&) = default;
  DirectoryBlobPartition& operator=(const DirectoryBlobPartition&) = default;
  DirectoryBlobPartition(DirectoryBlobPartition&&) = default;
  DirectoryBlobPartition& operator=(DirectoryBlobPartition&&) = default;

  bool IsWii() const { return m_is_wii; }
  u64 GetDataSize() const { return m_data_size; }
  void SetDataSize(u64 size) { m_data_size = size; }
  const std::string& GetRootDirectory() const { return m_root_directory; }
  const DiscContentContainer& GetContents() const { return m_contents; }
  const std::optional<DiscIO::Partition>& GetWrappedPartition() const
  {
    return m_wrapped_partition;
  }

  const std::array<u8, VolumeWii::AES_KEY_SIZE>& GetKey() const { return m_key; }
  void SetKey(std::array<u8, VolumeWii::AES_KEY_SIZE> key) { m_key = key; }

private:
  void SetDiscType(std::optional<bool> is_wii, const std::vector<u8>& disc_header);
  void SetBI2FromFile(const std::string& bi2_path);
  void SetBI2(std::vector<u8> bi2);

  // Returns DOL address
  u64 SetApploaderFromFile(const std::string& path);
  u64 SetApploader(std::vector<u8> apploader, const std::string& log_path);
  // Returns FST address
  u64 SetDOLFromFile(const std::string& path, u64 dol_address, std::vector<u8>* disc_header);
  u64 SetDOL(FSTBuilderNode dol_node, u64 dol_address, std::vector<u8>* disc_header);

  void BuildFSTFromFolder(const std::string& fst_root_path, u64 fst_address,
                          std::vector<u8>* disc_header);
  void BuildFST(std::vector<FSTBuilderNode> root_nodes, u64 fst_address,
                std::vector<u8>* disc_header);

  // FST creation
  void WriteEntryData(std::vector<u8>* fst_data, u32* entry_offset, u8 type, u32 name_offset,
                      u64 data_offset, u64 length, u32 address_shift);
  void WriteEntryName(std::vector<u8>* fst_data, u32* name_offset, const std::string& name,
                      u64 name_table_offset);
  void WriteDirectory(std::vector<u8>* fst_data, std::vector<FSTBuilderNode>* parent_entries,
                      u32* fst_offset, u32* name_offset, u64* data_offset, u32 parent_entry_index,
                      u64 name_table_offset);

  DiscContentContainer m_contents;

  std::array<u8, VolumeWii::AES_KEY_SIZE> m_key{};

  std::string m_root_directory;
  bool m_is_wii = false;
  // GameCube has no shift, Wii has 2 bit shift
  u32 m_address_shift = 0;

  u64 m_data_size = 0;

  std::optional<DiscIO::Partition> m_wrapped_partition = std::nullopt;
};

class DirectoryBlobReader : public BlobReader
{
  friend DiscContent;

public:
  static std::unique_ptr<DirectoryBlobReader> Create(const std::string& dol_path);
  static std::unique_ptr<DirectoryBlobReader> Create(
      std::unique_ptr<DiscIO::VolumeDisc> volume,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
          fst_callback);

  DirectoryBlobReader(DirectoryBlobReader&&) = default;
  DirectoryBlobReader& operator=(DirectoryBlobReader&&) = default;

  bool Read(u64 offset, u64 length, u8* buffer) override;
  bool SupportsReadWiiDecrypted(u64 offset, u64 size, u64 partition_data_offset) const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_data_offset) override;

  BlobType GetBlobType() const override;
  std::unique_ptr<BlobReader> CopyReader() const override;

  u64 GetRawSize() const override;
  u64 GetDataSize() const override;
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return 0; }
  bool HasFastRandomAccessInBlock() const override { return true; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

private:
  struct PartitionWithType
  {
    PartitionWithType(DirectoryBlobPartition&& partition_, PartitionType type_)
        : partition(std::move(partition_)), type(type_)
    {
    }

    DirectoryBlobPartition partition;
    PartitionType type;
  };

  explicit DirectoryBlobReader(const std::string& game_partition_root,
                               const std::string& true_root);
  explicit DirectoryBlobReader(
      std::unique_ptr<DiscIO::VolumeDisc> volume,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
      const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
          fst_callback);
  explicit DirectoryBlobReader(const DirectoryBlobReader& rhs);

  const DirectoryBlobPartition* GetPartition(u64 offset, u64 size, u64 partition_data_offset) const;

  bool EncryptPartitionData(u64 offset, u64 size, u8* buffer, u64 partition_data_offset,
                            u64 partition_data_decrypted_size);

  void SetNonpartitionDiscHeaderFromFile(const std::vector<u8>& partition_header,
                                         const std::string& game_partition_root);
  void SetNonpartitionDiscHeader(const std::vector<u8>& partition_header,
                                 std::vector<u8> header_bin);
  void SetWiiRegionDataFromFile(const std::string& game_partition_root);
  void SetWiiRegionData(const std::vector<u8>& wii_region_data, const std::string& log_path);
  void SetPartitions(std::vector<PartitionWithType>&& partitions);
  void SetPartitionHeader(DirectoryBlobPartition* partition, u64 partition_address);

  DiscIO::VolumeDisc* GetWrappedVolume() { return m_wrapped_volume.get(); }

  // For GameCube:
  DirectoryBlobPartition m_gamecube_pseudopartition;

  // For Wii:
  DiscContentContainer m_nonpartition_contents;
  std::map<u64, DirectoryBlobPartition> m_partitions;
  WiiEncryptionCache m_encryption_cache;

  bool m_is_wii;
  bool m_encrypted;

  u64 m_data_size;

  std::unique_ptr<DiscIO::VolumeDisc> m_wrapped_volume;
};

}  // namespace DiscIO
