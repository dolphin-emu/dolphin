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
class DiscContent
{
public:
  using ContentSource = std::variant<std::string, const u8*>;

  DiscContent(u64 offset, u64 size, const std::string& path);
  DiscContent(u64 offset, u64 size, const u8* data);

  // Provided because it's convenient when searching for DiscContent in an std::set
  explicit DiscContent(u64 offset);

  u64 GetOffset() const;
  u64 GetSize() const;
  bool Read(u64* offset, u64* length, u8** buffer) const;

  bool operator==(const DiscContent& other) const { return m_offset == other.m_offset; }
  bool operator!=(const DiscContent& other) const { return !(*this == other); }
  bool operator<(const DiscContent& other) const { return m_offset < other.m_offset; }
  bool operator>(const DiscContent& other) const { return other < *this; }
  bool operator<=(const DiscContent& other) const { return !(*this < other); }
  bool operator>=(const DiscContent& other) const { return !(*this > other); }
private:
  u64 m_offset;
  u64 m_size = 0;
  std::string m_path;
  ContentSource m_content_source;
};

class DirectoryBlobPartition
{
public:
  DirectoryBlobPartition() = default;
  DirectoryBlobPartition(const std::string& root_directory, std::optional<bool> is_wii);

  bool IsWii() const { return m_is_wii; }
  u64 GetDataSize() const { return m_data_size; }
  const std::vector<u8>& GetHeader() const { return m_disk_header; }
  const std::set<DiscContent>& GetContents() const { return m_contents; }
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

  std::set<DiscContent> m_contents;
  std::vector<u8> m_disk_header;
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

  bool Read(u64 offset, u64 length, u8* buffer) override;
  bool SupportsReadWiiDecrypted() const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_offset) override;

  BlobType GetBlobType() const override;
  u64 GetRawSize() const override;
  u64 GetDataSize() const override;

private:
  explicit DirectoryBlobReader(const std::string& game_partition_root);

  bool ReadInternal(u64 offset, u64 length, u8* buffer, const std::set<DiscContent>& contents);

  void SetNonpartitionDiscHeader(const std::vector<u8>& partition_header,
                                 const std::string& game_partition_root);
  void SetPartitionTable();
  void SetWiiRegionData(const std::string& game_partition_root);
  void SetTMDAndTicket(const std::string& partition_root);

  // For GameCube:
  DirectoryBlobPartition m_gamecube_pseudopartition;

  // For Wii:
  std::set<DiscContent> m_nonpartition_contents;
  std::map<u64, DirectoryBlobPartition> m_partitions;

  bool m_is_wii;

  std::vector<u8> m_disk_header_nonpartition;
  std::vector<u8> m_wii_region_data;

#pragma pack(push, 1)
  struct TMDHeader
  {
    u32 tmd_size;
    u32 tmd_offset;
  } m_tmd_header;
  static_assert(sizeof(TMDHeader) == 8, "Wrong size for TMDHeader");
#pragma pack(pop)

  u64 m_data_size;
};

}  // namespace
