// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class IVolume;

// TODO: eww
class CFileInfoGCWii;

// file info of an FST entry
class IFileInfo
{
public:
  // Not guaranteed to return a meaningful value for directories
  virtual u64 GetOffset() const = 0;
  // Not guaranteed to return a meaningful value for directories
  virtual u32 GetSize() const = 0;
  virtual bool IsDirectory() const = 0;
  virtual std::string GetName() const = 0;
};

class IFileSystem
{
public:
  IFileSystem(const IVolume* _rVolume);

  virtual ~IFileSystem();
  virtual bool IsValid() const = 0;
  // TODO: Should only return IFileInfo, not CFileInfoGCWii
  virtual const std::vector<CFileInfoGCWii>& GetFileList() const = 0;
  virtual const IFileInfo* FindFileInfo(const std::string& path) const = 0;
  virtual const IFileInfo* FindFileInfo(u64 disc_offset) const = 0;
  virtual u64 ReadFile(const IFileInfo* file_info, u8* _pBuffer, u64 _MaxBufferSize,
                       u64 _OffsetInFile = 0) const = 0;
  virtual bool ExportFile(const IFileInfo* file_info,
                          const std::string& _rExportFilename) const = 0;
  virtual bool ExportApploader(const std::string& _rExportFolder) const = 0;
  virtual bool ExportDOL(const std::string& _rExportFolder) const = 0;
  virtual std::string GetPath(u64 _Address) const = 0;
  virtual std::string GetPathFromFSTOffset(size_t file_info_offset) const = 0;
  virtual u64 GetBootDOLOffset() const = 0;
  virtual u32 GetBootDOLSize(u64 dol_offset) const = 0;

protected:
  const IVolume* m_rVolume;
};

std::unique_ptr<IFileSystem> CreateFileSystem(const IVolume* volume);

}  // namespace
