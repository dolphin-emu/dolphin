// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
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

namespace File
{
struct FSTEntry;
class IOFile;
}

namespace DiscIO
{
enum class PartitionType : u32;

// Returns true if the path is inside a DirectoryBlob and doesn't represent the DirectoryBlob itself
bool ShouldHideFromGameList(const std::string& volume_path);

class DiscContent
{
public:
  using ContentSource = std::variant<std::string, const u8*>;

  DiscContent(u64 offset, u64 size, const std::string& path);
  DiscContent(u64 offset, u64 size, const u8* data);

  // Provided because it's convenient when searching for DiscContent in an std::set
  explicit DiscContent(u64 offset);

  u64 GetOffset() const;
  u64 GetEndOffset() const;
  u64 GetSize() const;
  bool Read(u64* offset, u64* length, u8** buffer) const;

  bool operator==(const DiscContent& other) const { return GetEndOffset() == other.GetEndOffset(); }
  bool operator!=(const DiscContent& other) const { return !(*this == other); }
  bool operator<(const DiscContent& other) const { return GetEndOffset() < other.GetEndOffset(); }
  bool operator>(const DiscContent& other) const { return other < *this; }
  bool operator<=(const DiscContent& other) const { return !(*this < other); }
  bool operator>=(const DiscContent& other) const { return !(*this > other); }
private:
  u64 m_offset;
  u64 m_size = 0;
  ContentSource m_content_source;
};

class DiscContentContainer
{
public:
  template <typename T>
  void Add(u64 offset, const std::vector<T>& vector)
  {
    return Add(offset, vector.size() * sizeof(T), reinterpret_cast<const u8*>(vector.data()));
  }
  void Add(u64 offset, u64 size, const std::string& path);
  void Add(u64 offset, u64 size, const u8* data);
  u64 CheckSizeAndAdd(u64 offset, const std::string& path);
  u64 CheckSizeAndAdd(u64 offset, u64 max_size, const std::string& path);

  bool Read(u64 offset, u64 length, u8* buffer) const;

private:
  std::set<DiscContent> m_contents;
};

class DirectoryBlobPartition
{
public:
  DirectoryBlobPartition() = default;
  DirectoryBlobPartition(const std::string& root_directory, std::optional<bool> is_wii);

  // We do not allow copying, because it might mess up the pointers inside DiscContents
  DirectoryBlobPartition(const DirectoryBlobPartition&) = delete;
  DirectoryBlobPartition& operator=(const DirectoryBlobPartition&) = delete;
  DirectoryBlobPartition(DirectoryBlobPartition&&) = default;
  DirectoryBlobPartition& operator=(DirectoryBlobPartition&&) = default;

  bool IsWii() const { return m_is_wii; }
  u64 GetDataSize() const { return m_data_size; }
  const std::string& GetRootDirectory() const { return m_root_directory; }
  const std::vector<u8>& GetHeader() const { return m_disc_header; }
  const DiscContentContainer& GetContents() const { return m_contents; }
private:
  void SetDiscHeaderAndDiscType(std::optional<bool> is_wii);
  void SetBI2();

  // Returns DOL address
  u64 SetApploader();
  // Returns FST address
  u64 SetDOL(u64 dol_address);

  void BuildFST(u64 fst_address);

  // FST creation
  void WriteEntryData(u32* entry_offset, u8 type, u32 name_offset, u64 data_offset, u64 length,
                      u32 address_shift);
  void WriteEntryName(u32* name_offset, const std::string& name, u64 name_table_offset);
  void WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset, u32* name_offset,
                      u64* data_offset, u32 parent_entry_index, u64 name_table_offset);

  DiscContentContainer m_contents;
  std::vector<u8> m_disc_header;
  std::vector<u8> m_bi2;
  std::vector<u8> m_apploader;
  std::vector<u8> m_fst_data;

  std::string m_root_directory;
  bool m_is_wii = false;
  // GameCube has no shift, Wii has 2 bit shift
  u32 m_address_shift = 0;

  u64 m_data_size;
};

class DirectoryBlobReader : public BlobReader
{
public:
  static std::unique_ptr<DirectoryBlobReader> Create(const std::string& dol_path);

  // We do not allow copying, because it might mess up the pointers inside DiscContents
  DirectoryBlobReader(const DirectoryBlobReader&) = delete;
  DirectoryBlobReader& operator=(const DirectoryBlobReader&) = delete;
  DirectoryBlobReader(DirectoryBlobReader&&) = default;
  DirectoryBlobReader& operator=(DirectoryBlobReader&&) = default;

  bool Read(u64 offset, u64 length, u8* buffer) override;
  bool SupportsReadWiiDecrypted() const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_offset) override;

  BlobType GetBlobType() const override;
  u64 GetRawSize() const override;
  u64 GetDataSize() const override;

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

  void SetNonpartitionDiscHeader(const std::vector<u8>& partition_header,
                                 const std::string& game_partition_root);
  void SetWiiRegionData(const std::string& game_partition_root);
  void SetPartitions(std::vector<PartitionWithType>&& partitions);
  void SetPartitionHeader(const DirectoryBlobPartition& partition, u64 partition_address);

  // For GameCube:
  DirectoryBlobPartition m_gamecube_pseudopartition;

  // For Wii:
  DiscContentContainer m_nonpartition_contents;
  std::map<u64, DirectoryBlobPartition> m_partitions;

  bool m_is_wii;

  std::vector<u8> m_disc_header_nonpartition;
  std::vector<u8> m_partition_table;
  std::vector<u8> m_wii_region_data;
  std::vector<std::vector<u8>> m_partition_headers;

  u64 m_data_size;
};

}  // namespace
