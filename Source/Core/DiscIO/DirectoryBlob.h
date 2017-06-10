// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
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
  explicit DirectoryBlobReader(const std::string& root_directory);

  bool ReadInternal(u64 offset, u64 length, u8* buffer, const std::set<DiscContent>& contents);

  void SetDiscHeaderAndDiscType();
  void SetPartitionTable();
  void SetWiiRegionData();
  void SetTMDAndTicket();

  // Returns DOL address
  u64 SetApploader();
  // Returns FST address
  u64 SetDOL(u64 dol_address);

  void BuildFST(u64 fst_address);

  void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer) const;

  void Write32(u32 data, u32 offset, std::vector<u8>* const buffer);

  // FST creation
  void WriteEntryData(u32* entry_offset, u8 type, u32 name_offset, u64 data_offset, u64 length,
                      u32 address_shift);
  void WriteEntryName(u32* name_offset, const std::string& name, u64 name_table_offset);
  void WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset, u32* name_offset,
                      u64* data_offset, u32 parent_entry_index, u64 name_table_offset);

  std::string m_root_directory;

  std::set<DiscContent> m_virtual_disc;
  std::set<DiscContent> m_nonpartition_contents;

  bool m_is_wii;

  // GameCube has no shift, Wii has 2 bit shift
  u32 m_address_shift;

  std::vector<u8> m_fst_data;

  std::vector<u8> m_disk_header;
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

  std::vector<u8> m_apploader;
};

}  // namespace
