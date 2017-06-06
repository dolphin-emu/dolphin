// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
FileSystemGCWii::FileSystemGCWii(const Volume* _rVolume, const Partition& partition)
    : FileSystem(_rVolume, partition), m_Initialized(false), m_Valid(false), m_offset_shift(0)
{
  m_Valid = DetectFileSystem();
}

FileSystemGCWii::~FileSystemGCWii()
{
  m_FileInfoVector.clear();
}

u64 FileSystemGCWii::GetFileSize(const std::string& _rFullPath)
{
  if (!m_Initialized)
    InitFileSystem();

  const FileInfo* pFileInfo = FindFileInfo(_rFullPath);

  if (pFileInfo != nullptr && !pFileInfo->IsDirectory())
    return pFileInfo->m_FileSize;

  return 0;
}

std::string FileSystemGCWii::GetFileName(u64 _Address)
{
  if (!m_Initialized)
    InitFileSystem();

  for (auto& fileInfo : m_FileInfoVector)
  {
    if ((fileInfo.m_Offset <= _Address) && ((fileInfo.m_Offset + fileInfo.m_FileSize) > _Address))
    {
      return fileInfo.m_FullPath;
    }
  }

  return "";
}

u64 FileSystemGCWii::ReadFile(const std::string& _rFullPath, u8* _pBuffer, u64 _MaxBufferSize,
                              u64 _OffsetInFile)
{
  if (!m_Initialized)
    InitFileSystem();

  const FileInfo* pFileInfo = FindFileInfo(_rFullPath);
  if (pFileInfo == nullptr)
    return 0;

  if (_OffsetInFile >= pFileInfo->m_FileSize)
    return 0;

  u64 read_length = std::min(_MaxBufferSize, pFileInfo->m_FileSize - _OffsetInFile);

  DEBUG_LOG(
      DISCIO,
      "Reading %" PRIx64 " bytes at %" PRIx64 " from file %s. Offset: %" PRIx64 " Size: %" PRIx64,
      read_length, _OffsetInFile, _rFullPath.c_str(), pFileInfo->m_Offset, pFileInfo->m_FileSize);

  m_rVolume->Read(pFileInfo->m_Offset + _OffsetInFile, read_length, _pBuffer, m_partition);
  return read_length;
}

bool FileSystemGCWii::ExportFile(const std::string& _rFullPath, const std::string& _rExportFilename)
{
  if (!m_Initialized)
    InitFileSystem();

  const FileInfo* pFileInfo = FindFileInfo(_rFullPath);

  if (!pFileInfo)
    return false;

  u64 remainingSize = pFileInfo->m_FileSize;
  u64 fileOffset = pFileInfo->m_Offset;

  File::IOFile f(_rExportFilename, "wb");
  if (!f)
    return false;

  bool result = true;

  while (remainingSize)
  {
    // Limit read size to 128 MB
    size_t readSize = (size_t)std::min(remainingSize, (u64)0x08000000);

    std::vector<u8> buffer(readSize);

    result = m_rVolume->Read(fileOffset, readSize, &buffer[0], m_partition);

    if (!result)
      break;

    f.WriteBytes(&buffer[0], readSize);

    remainingSize -= readSize;
    fileOffset += readSize;
  }

  return result;
}

bool FileSystemGCWii::ExportApploader(const std::string& _rExportFolder) const
{
  std::optional<u32> apploader_size = m_rVolume->ReadSwapped<u32>(0x2440 + 0x14, m_partition);
  const std::optional<u32> trailer_size = m_rVolume->ReadSwapped<u32>(0x2440 + 0x18, m_partition);
  constexpr u32 header_size = 0x20;
  if (!apploader_size || !trailer_size)
    return false;
  *apploader_size += *trailer_size + header_size;
  DEBUG_LOG(DISCIO, "Apploader size -> %x", *apploader_size);

  std::vector<u8> buffer(*apploader_size);
  if (m_rVolume->Read(0x2440, *apploader_size, buffer.data(), m_partition))
  {
    std::string exportName(_rExportFolder + "/apploader.img");

    File::IOFile AppFile(exportName, "wb");
    if (AppFile)
    {
      AppFile.WriteBytes(buffer.data(), *apploader_size);
      return true;
    }
  }

  return false;
}

std::optional<u64> FileSystemGCWii::GetBootDOLOffset() const
{
  std::optional<u32> offset = m_rVolume->ReadSwapped<u32>(0x420, m_partition);
  return offset ? static_cast<u64>(*offset) << 2 : std::optional<u64>();
}

std::optional<u32> FileSystemGCWii::GetBootDOLSize(u64 dol_offset) const
{
  u32 dol_size = 0;

  // Iterate through the 7 code segments
  for (u8 i = 0; i < 7; i++)
  {
    const std::optional<u32> offset =
        m_rVolume->ReadSwapped<u32>(dol_offset + 0x00 + i * 4, m_partition);
    const std::optional<u32> size =
        m_rVolume->ReadSwapped<u32>(dol_offset + 0x90 + i * 4, m_partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  // Iterate through the 11 data segments
  for (u8 i = 0; i < 11; i++)
  {
    const std::optional<u32> offset =
        m_rVolume->ReadSwapped<u32>(dol_offset + 0x1c + i * 4, m_partition);
    const std::optional<u32> size =
        m_rVolume->ReadSwapped<u32>(dol_offset + 0xac + i * 4, m_partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  return dol_size;
}

bool FileSystemGCWii::ExportDOL(const std::string& _rExportFolder) const
{
  std::optional<u64> dol_offset = GetBootDOLOffset();
  if (!dol_offset)
    return false;
  std::optional<u32> dol_size = GetBootDOLSize(*dol_offset);
  if (!dol_size)
    return false;

  std::vector<u8> buffer(*dol_size);
  if (m_rVolume->Read(*dol_offset, *dol_size, &buffer[0], m_partition))
  {
    std::string exportName(_rExportFolder + "/boot.dol");

    File::IOFile DolFile(exportName, "wb");
    if (DolFile)
    {
      DolFile.WriteBytes(&buffer[0], *dol_size);
      return true;
    }
  }

  return false;
}

std::string FileSystemGCWii::GetStringFromOffset(u64 _Offset) const
{
  std::string data(255, 0x00);
  m_rVolume->Read(_Offset, data.size(), (u8*)&data[0], m_partition);
  data.erase(std::find(data.begin(), data.end(), 0x00), data.end());

  // TODO: Should we really always use SHIFT-JIS?
  // It makes some filenames in Pikmin (NTSC-U) sane, but is it correct?
  return SHIFTJISToUTF8(data);
}

const std::vector<FileInfo>& FileSystemGCWii::GetFileList()
{
  if (!m_Initialized)
    InitFileSystem();

  return m_FileInfoVector;
}

const FileInfo* FileSystemGCWii::FindFileInfo(const std::string& _rFullPath)
{
  if (!m_Initialized)
    InitFileSystem();

  for (auto& fileInfo : m_FileInfoVector)
  {
    if (!strcasecmp(fileInfo.m_FullPath.c_str(), _rFullPath.c_str()))
      return &fileInfo;
  }

  return nullptr;
}

bool FileSystemGCWii::DetectFileSystem()
{
  if (m_rVolume->ReadSwapped<u32>(0x18, m_partition) == u32(0x5D1C9EA3))
  {
    m_offset_shift = 2;  // Wii file system
    return true;
  }
  else if (m_rVolume->ReadSwapped<u32>(0x1c, m_partition) == u32(0xC2339F3D))
  {
    m_offset_shift = 0;  // GameCube file system
    return true;
  }

  return false;
}

void FileSystemGCWii::InitFileSystem()
{
  m_Initialized = true;

  // read the whole FST
  const std::optional<u32> fst_offset_unshifted = m_rVolume->ReadSwapped<u32>(0x424, m_partition);
  if (!fst_offset_unshifted)
    return;
  const u64 FSTOffset = static_cast<u64>(*fst_offset_unshifted) << m_offset_shift;

  // read all fileinfos
  const std::optional<u32> root_name_offset = m_rVolume->ReadSwapped<u32>(FSTOffset, m_partition);
  const std::optional<u32> root_offset = m_rVolume->ReadSwapped<u32>(FSTOffset + 0x4, m_partition);
  const std::optional<u32> root_size = m_rVolume->ReadSwapped<u32>(FSTOffset + 0x8, m_partition);
  if (!root_name_offset || !root_offset || !root_size)
    return;
  FileInfo root = {*root_name_offset, static_cast<u64>(*root_offset) << m_offset_shift, *root_size};

  if (!root.IsDirectory())
    return;

  // 12 bytes (the size of a file entry) times 10 * 1024 * 1024 is 120 MiB,
  // more than total RAM in a Wii. No file system should use anywhere near that much.
  static const u32 ARBITRARY_FILE_SYSTEM_SIZE_LIMIT = 10 * 1024 * 1024;
  if (root.m_FileSize > ARBITRARY_FILE_SYSTEM_SIZE_LIMIT)
  {
    // Without this check, Dolphin can crash by trying to allocate too much
    // memory when loading the file systems of certain malformed disc images.

    ERROR_LOG(DISCIO, "File system is abnormally large! Aborting loading");
    return;
  }

  if (m_FileInfoVector.size())
    PanicAlert("Wtf?");
  u64 NameTableOffset = FSTOffset;

  m_FileInfoVector.reserve((size_t)root.m_FileSize);
  for (u32 i = 0; i < root.m_FileSize; i++)
  {
    const u64 read_offset = FSTOffset + (i * 0xC);
    const std::optional<u32> name_offset = m_rVolume->ReadSwapped<u32>(read_offset, m_partition);
    const std::optional<u32> offset = m_rVolume->ReadSwapped<u32>(read_offset + 0x4, m_partition);
    const std::optional<u32> size = m_rVolume->ReadSwapped<u32>(read_offset + 0x8, m_partition);
    m_FileInfoVector.emplace_back(name_offset.value_or(0),
                                  static_cast<u64>(offset.value_or(0)) << m_offset_shift,
                                  size.value_or(0));
    NameTableOffset += 0xC;
  }

  BuildFilenames(1, m_FileInfoVector.size(), "", NameTableOffset);
}

size_t FileSystemGCWii::BuildFilenames(const size_t _FirstIndex, const size_t _LastIndex,
                                       const std::string& _szDirectory, u64 _NameTableOffset)
{
  size_t CurrentIndex = _FirstIndex;

  while (CurrentIndex < _LastIndex)
  {
    FileInfo& rFileInfo = m_FileInfoVector[CurrentIndex];
    u64 const uOffset = _NameTableOffset + (rFileInfo.m_NameOffset & 0xFFFFFF);
    std::string const offset_str{GetStringFromOffset(uOffset)};
    bool const is_dir = rFileInfo.IsDirectory();
    rFileInfo.m_FullPath.reserve(_szDirectory.size() + offset_str.size());

    rFileInfo.m_FullPath.append(_szDirectory.data(), _szDirectory.size())
        .append(offset_str.data(), offset_str.size())
        .append("/", size_t(is_dir));

    if (!is_dir)
    {
      ++CurrentIndex;
      continue;
    }

    // check next index
    CurrentIndex = BuildFilenames(CurrentIndex + 1, (size_t)rFileInfo.m_FileSize,
                                  rFileInfo.m_FullPath, _NameTableOffset);
  }

  return CurrentIndex;
}

}  // namespace
