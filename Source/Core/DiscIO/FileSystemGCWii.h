// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
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
  FileInfoGCWii(u64 name_offset, u64 offset, u64 file_size);
  ~FileInfoGCWii() override;

  u64 GetOffset() const override { return m_Offset; }
  u64 GetSize() const override { return m_FileSize; }
  bool IsDirectory() const override { return (m_NameOffset & 0xFF000000) != 0; }
  // TODO: These shouldn't be public
  std::string m_FullPath;
  const u64 m_NameOffset = 0u;

private:
  const u64 m_Offset = 0u;
  const u64 m_FileSize = 0u;
};

class FileSystemGCWii : public FileSystem
{
public:
  FileSystemGCWii(const Volume* _rVolume, const Partition& partition);
  ~FileSystemGCWii() override;
  bool IsValid() const override { return m_Valid; }
  u64 GetFileSize(const std::string& _rFullPath) override;
  const std::vector<FileInfoGCWii>& GetFileList() override;
  std::string GetFileName(u64 _Address) override;
  u64 ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize,
               u64 _OffsetInFile) override;
  bool ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename) override;
  bool ExportApploader(const std::string& _rExportFolder) const override;
  bool ExportDOL(const std::string& _rExportFolder) const override;
  std::optional<u64> GetBootDOLOffset() const override;
  std::optional<u32> GetBootDOLSize(u64 dol_offset) const override;

private:
  bool m_Initialized;
  bool m_Valid;
  u32 m_offset_shift;
  std::vector<FileInfoGCWii> m_FileInfoVector;

  std::string GetStringFromOffset(u64 _Offset) const;
  const FileInfoGCWii* FindFileInfo(const std::string& _rFullPath);
  bool DetectFileSystem();
  void InitFileSystem();
  size_t BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex,
                        const std::string& _szDirectory, u64 _NameTableOffset);
};

}  // namespace
