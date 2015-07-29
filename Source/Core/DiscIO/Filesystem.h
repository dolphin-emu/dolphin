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
  virtual u64 GetSize() const = 0;
  virtual bool IsDirectory() const = 0;
  virtual const std::string& GetName() const = 0;
};

class IFileSystem
{
public:
  IFileSystem(const IVolume* _rVolume);

  virtual ~IFileSystem();
  virtual bool IsValid() const = 0;
  // TODO: Should only return IFileInfo, not CFileInfoGCWii
  virtual const std::vector<CFileInfoGCWii>& GetFileList() = 0;
  virtual u64 GetFileSize(const std::string& _rFullPath) = 0;
  virtual u64 ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize,
                       u64 _OffsetInFile = 0) = 0;
  virtual bool ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename) = 0;
  virtual bool ExportApploader(const std::string& _rExportFolder) const = 0;
  virtual bool ExportDOL(const std::string& _rExportFolder) const = 0;
  virtual std::string GetPath(u64 _Address) = 0;
  virtual std::string GetPathFromFSTOffset(size_t file_info_offset) = 0;
  virtual u64 GetBootDOLOffset() const = 0;
  virtual u32 GetBootDOLSize(u64 dol_offset) const = 0;

protected:
  const IVolume* m_rVolume;
};

std::unique_ptr<IFileSystem> CreateFileSystem(const IVolume* volume);

}  // namespace
