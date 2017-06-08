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
  static bool IsValidDirectoryBlob(std::string dol_path);
  static std::unique_ptr<DirectoryBlobReader> Create(File::IOFile dol, const std::string& dol_path);

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
  DirectoryBlobReader(File::IOFile dol_file, const std::string& root_directory);

  bool ReadPartition(u64 offset, u64 length, u8* buffer);
  bool ReadNonPartition(u64 offset, u64 length, u8* buffer);

  void SetDiskTypeWii();
  void SetDiskTypeGC();

  void SetGameID(const std::string& id);
  void SetName(const std::string&);

  bool SetApploader(const std::string& apploader);
  void SetDOLAndDiskType(File::IOFile dol_file);

  void BuildFST();

  // writing to read buffer
  void WriteToBuffer(u64 source_start_address, u64 source_length, const u8* source, u64* address,
                     u64* length, u8** buffer) const;

  void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer) const;

  void Write32(u32 data, u32 offset, std::vector<u8>* const buffer);

  // FST creation
  void WriteEntryData(u32* entry_offset, u8 type, u32 name_offset, u64 data_offset, u64 length,
                      u32 address_shift);
  void WriteEntryName(u32* name_offset, const std::string& name);
  void WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset, u32* name_offset,
                      u64* data_offset, u32 parent_entry_index);

  std::string m_root_directory;

  std::set<DiscContent> m_virtual_disc;

  bool m_is_wii = false;

  // GameCube has no shift, Wii has 2 bit shift
  u32 m_address_shift = 0;

  // first address on disk containing file data
  u64 m_data_start_address;

  u64 m_fst_name_offset;
  std::vector<u8> m_fst_data;

  std::vector<u8> m_disk_header;

#pragma pack(push, 1)
  struct SDiskHeaderInfo
  {
    u32 debug_monitor_size;
    u32 simulated_mem_size;
    u32 arg_offset;
    u32 debug_flag;
    u32 track_location;
    u32 track_size;
    u32 country_code;
    u32 unknown;
    u32 unknown2;

    // All the data is byteswapped
    SDiskHeaderInfo()
    {
      debug_monitor_size = 0;
      simulated_mem_size = 0;
      arg_offset = 0;
      debug_flag = 0;
      track_location = 0;
      track_size = 0;
      country_code = 0;
      unknown = 0;
      unknown2 = 0;
    }
  };
#pragma pack(pop)
  std::unique_ptr<SDiskHeaderInfo> m_disk_header_info;

  std::vector<u8> m_apploader;
  std::vector<u8> m_dol;

  u64 m_fst_address;
  u64 m_dol_address;
};

}  // namespace
