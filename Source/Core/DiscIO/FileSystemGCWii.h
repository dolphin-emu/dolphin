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
  CFileInfoGCWii(u64 name_offset, u64 offset, u64 file_size, std::string name);

  u64 GetOffset() const override { return m_Offset; }
  u64 GetSize() const override { return m_FileSize; }
  bool IsDirectory() const override { return (m_NameOffset & 0xFF000000) != 0; }
  const std::string& GetName() const override { return m_Name; }
  // TODO: These shouldn't be public
  std::string m_Name;
  const u64 m_NameOffset = 0u;

private:
  const u64 m_Offset = 0u;
  const u64 m_FileSize = 0u;
};

class CFileSystemGCWii : public IFileSystem
{
public:
  CFileSystemGCWii(const IVolume* _rVolume);
  ~CFileSystemGCWii() override;
  bool IsValid() const override { return m_Valid; }
  const std::vector<CFileInfoGCWii>& GetFileList() override;
  const IFileInfo* FindFileInfo(const std::string& path) override;
  const IFileInfo* FindFileInfo(u64 disc_offset) override;
  std::string GetPath(u64 _Address) override;
  std::string GetPathFromFSTOffset(size_t file_info_offset) override;
  u64 ReadFile(const IFileInfo* file_info, u8* _pBuffer, u64 _MaxBufferSize,
               u64 _OffsetInFile) override;
  bool ExportFile(const IFileInfo* file_info, const std::string& _rExportFilename) override;
  bool ExportApploader(const std::string& _rExportFolder) const override;
  bool ExportDOL(const std::string& _rExportFolder) const override;
  u64 GetBootDOLOffset() const override;
  u32 GetBootDOLSize(u64 dol_offset) const override;

private:
  bool m_Initialized;
  bool m_Valid;
  bool m_Wii;
  std::vector<CFileInfoGCWii> m_FileInfoVector;

  const IFileInfo* FindFileInfo(const std::string& path, size_t search_start_offset) const;
  std::string GetStringFromOffset(u64 _Offset) const;
  bool DetectFileSystem();
  void InitFileSystem();
  u32 GetOffsetShift() const;
};

}  // namespace
