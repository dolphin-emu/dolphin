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
class DirectoryBlobReader : public BlobReader
{
public:
  static bool IsValidDirectoryBlob(std::string dol_path);
  static std::unique_ptr<DirectoryBlobReader> Create(File::IOFile dol, const std::string& dol_path);

  bool Read(u64 offset, u64 length, u8* buffer) override;
  bool SupportsReadWiiDecrypted() const override;
  bool ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_offset) override;

  BlobType GetBlobType() const override;
  u64 GetRawSize() const override;
  u64 GetDataSize() const override;

private:
  class DiscContent
  {
  public:
    // Either a pointer to memory or a path to a file can be used as a content source
    using ContentSource = std::variant<std::string, const u8*>;

    DiscContent(u64 offset, u64 size, ContentSource content_source);

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
    u64 m_size;
    ContentSource m_content_source;
  };

  DirectoryBlobReader(File::IOFile dol_file, const std::string& root_directory);

  bool ReadInternal(u64 offset, u64 length, u8* buffer, const std::set<DiscContent>& contents);

  void SetDiskTypeWii();
  void SetDiskTypeGC();

  void SetGameID(const std::string& id);
  void SetName(const std::string&);

  bool SetApploader(const std::string& apploader);
  void SetDOLAndDiskType(File::IOFile dol_file);

  void BuildFST();

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
  std::set<DiscContent> m_nonpartition_contents;

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

  static constexpr u8 ENTRY_SIZE = 0x0c;
  static constexpr u8 FILE_ENTRY = 0;
  static constexpr u8 DIRECTORY_ENTRY = 1;
  static constexpr u64 DISKHEADER_ADDRESS = 0;
  static constexpr u64 DISKHEADERINFO_ADDRESS = 0x440;
  static constexpr u64 APPLOADER_ADDRESS = 0x2440;
  static const size_t MAX_NAME_LENGTH = 0x3df;
  static const size_t MAX_ID_LENGTH = 6;
};

}  // namespace
