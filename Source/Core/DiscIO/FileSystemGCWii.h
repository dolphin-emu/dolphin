// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Filesystem.h"

namespace DiscIO
{
class Volume;
struct Partition;

class FileInfoGCWii : public FileInfo
{
public:
  // None of the constructors take ownership of FST pointers

  // Set everything manually
  FileInfoGCWii(const u8* fst, u8 offset_shift, u32 index, u32 total_file_infos);
  // For the root object only
  FileInfoGCWii(const u8* fst, u8 offset_shift);
  // Copy another object
  FileInfoGCWii(const FileInfoGCWii& file_info) = default;
  // Copy data that is common to the whole file system
  FileInfoGCWii(const FileInfoGCWii& file_info, u32 index);

  ~FileInfoGCWii() override;

  std::unique_ptr<FileInfo> clone() const override;
  const_iterator begin() const override;
  const_iterator end() const override;

  u64 GetOffset() const override;
  u32 GetSize() const override;
  bool IsDirectory() const override;
  u32 GetTotalChildren() const override;
  std::string GetName() const override;
  std::string GetPath() const override;

  bool IsValid(u64 fst_size, const FileInfoGCWii& parent_directory) const;

protected:
  uintptr_t GetAddress() const override;
  FileInfo& operator++() override;

private:
  enum class EntryProperty
  {
    // NAME_OFFSET's lower 3 bytes are the name's offset within the name table.
    // NAME_OFFSET's upper 1 byte is 1 for directories and 0 for files.
    NAME_OFFSET = 0,
    // For files, FILE_OFFSET is the file offset in the partition,
    // and for directories, it's the FST index of the parent directory.
    // The root directory has its parent directory index set to 0.
    FILE_OFFSET = 1,
    // For files, FILE_SIZE is the file size, and for directories,
    // it's the FST index of the next entry that isn't in the directory.
    FILE_SIZE = 2
  };

  // For files, returns the index of the next item. For directories,
  // returns the index of the next item that isn't inside it.
  u32 GetNextIndex() const;
  // Returns one of the three properties of this FST entry.
  // Read the comments in EntryProperty for details.
  u32 Get(EntryProperty entry_property) const;
  // Returns the name offset, excluding the directory identification byte
  u64 GetNameOffset() const;

  const u8* m_fst;
  u8 m_offset_shift;
  u32 m_index;
  u32 m_total_file_infos;
};

class FileSystemGCWii : public FileSystem
{
public:
  FileSystemGCWii(const Volume* volume, const Partition& partition);
  ~FileSystemGCWii() override;

  bool IsValid() const override { return m_valid; }
  const FileInfo& GetRoot() const override;
  std::unique_ptr<FileInfo> FindFileInfo(const std::string& path) const override;
  std::unique_ptr<FileInfo> FindFileInfo(u64 disc_offset) const override;

private:
  bool m_valid;
  std::vector<u8> m_file_system_table;
  FileInfoGCWii m_root;
  // Maps the end offset of files to FST indexes
  mutable std::map<u64, u32> m_offset_file_info_cache;

  std::unique_ptr<FileInfo> FindFileInfo(const std::string& path, const FileInfo& file_info) const;
};

}  // namespace
