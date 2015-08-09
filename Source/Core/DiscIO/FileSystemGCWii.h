// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
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

  IFileInfo& operator++() override;
  std::unique_ptr<IFileInfo> clone() const override;
  const_iterator begin() const override;
  const_iterator end() const override;

  // For directories, GetRawOffset should be used instead. This one does a special correction for
  // Wii files
  u64 GetOffset() const override;
  // For directories, returns the index of the next entry that is not in this directory
  u32 GetSize() const override;
  bool IsDirectory() const override;
  u32 GetTotalChildren() const override;
  std::string GetName() const override;
  std::string GetPath() const override;

protected:
  const void* GetAddress() const override;

private:
  u32 GetNextIndex() const;

  u32 Get(int n) const;
  u32 GetRawNameOffset() const;
  // For directories, returns the index of the parent directory (or 0 if root)
  u32 GetRawOffset() const;

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
  // Maps the end offset of files to FST indexes
  mutable std::map<u64, u32> m_offset_file_info_cache;

  std::unique_ptr<IFileInfo> FindFileInfo(const std::string& path,
                                          const IFileInfo& file_info) const;
  u32 GetOffsetShift() const;
};

}  // namespace
