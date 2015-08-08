// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Filesystem.h"

namespace DiscIO
{
class IVolume;

class CFileInfoGCWii : public IFileInfo
{
public:
  // None of the constructors take ownership of FST pointers

  // Set everything manually
  CFileInfoGCWii(const u8* fst, bool wii, u32 index, u32 total_file_infos);
  // For the root object only
  CFileInfoGCWii(const u8* fst, bool wii);
  // Copy another object
  CFileInfoGCWii(const CFileInfoGCWii& file_info) = default;
  // Copy data that is common to the whole file system
  CFileInfoGCWii(const CFileInfoGCWii& file_info, u32 index);

  ~CFileInfoGCWii() override;

  std::unique_ptr<IFileInfo> clone() const override;
  const_iterator begin() const override;
  const_iterator end() const override;

  u64 GetOffset() const override;
  u32 GetSize() const override;
  bool IsDirectory() const override;
  u32 GetTotalChildren() const override;
  std::string GetName() const override;
  std::string GetPath() const override;

protected:
  uintptr_t GetAddress() const override;
  IFileInfo& operator++() override;

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

  const u8* m_fst;
  bool m_wii;
  u32 m_index;
  u32 m_total_file_infos;
};

class CFileSystemGCWii : public IFileSystem
{
public:
  CFileSystemGCWii(const IVolume* _rVolume);
  ~CFileSystemGCWii() override;

  bool IsValid() const override { return m_Valid; }
  const IFileInfo& GetRoot() const override;
  std::unique_ptr<IFileInfo> FindFileInfo(const std::string& path) const override;
  std::unique_ptr<IFileInfo> FindFileInfo(u64 disc_offset) const override;

  u64 ReadFile(const IFileInfo* file_info, u8* _pBuffer, u64 _MaxBufferSize,
               u64 _OffsetInFile) const override;
  bool ExportFile(const IFileInfo* file_info, const std::string& _rExportFilename) const override;
  bool ExportApploader(const std::string& _rExportFolder) const override;
  bool ExportDOL(const std::string& _rExportFolder) const override;
  u64 GetBootDOLOffset() const override;
  u32 GetBootDOLSize(u64 dol_offset) const override;

private:
  bool m_Valid;
  bool m_Wii;
  std::vector<u8> m_file_system_table;
  CFileInfoGCWii m_root;

  std::unique_ptr<IFileInfo> FindFileInfo(const std::string& path,
                                          const IFileInfo& file_info) const;
  std::unique_ptr<IFileInfo> FindFileInfo(u64 disc_offset, const IFileInfo& file_info) const;
  u32 GetOffsetShift() const;
};

}  // namespace
