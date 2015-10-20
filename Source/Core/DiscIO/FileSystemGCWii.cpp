// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

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
static const u32 FST_ENTRY_SIZE = 4 * 3;  // An FST entry consists of three 32-bit integers

// Set everything manually.
CFileInfoGCWii::CFileInfoGCWii(const u8* fst, bool wii, u32 index, u32 total_file_infos)
    : m_fst(fst), m_wii(wii), m_index(index), m_total_file_infos(total_file_infos)
{
}

// For the root object only.
// m_fst and m_index must be correctly set before GetSize() is called!
CFileInfoGCWii::CFileInfoGCWii(const u8* fst, bool wii)
    : m_fst(fst), m_wii(wii), m_index(0), m_total_file_infos(GetSize())
{
}

// Copy data that is common to the whole file system.
CFileInfoGCWii::CFileInfoGCWii(const CFileInfoGCWii& file_info, u32 index)
    : CFileInfoGCWii(file_info.m_fst, file_info.m_wii, index, file_info.m_total_file_infos)
{
}

CFileInfoGCWii::~CFileInfoGCWii()
{
}

const void* CFileInfoGCWii::GetAddress() const
{
  return m_fst + FST_ENTRY_SIZE * m_index;
}

u32 CFileInfoGCWii::GetNextIndex() const
{
  return IsDirectory() ? GetSize() : m_index + 1;
}

IFileInfo& CFileInfoGCWii::operator++()
{
  m_index = GetNextIndex();
  return *this;
}

std::unique_ptr<IFileInfo> CFileInfoGCWii::clone() const
{
  return std::make_unique<CFileInfoGCWii>(*this);
}

IFileInfo::const_iterator CFileInfoGCWii::begin() const
{
  return const_iterator(std::make_unique<CFileInfoGCWii>(*this, m_index + 1));
}

IFileInfo::const_iterator CFileInfoGCWii::end() const
{
  return const_iterator(std::make_unique<CFileInfoGCWii>(*this, GetNextIndex()));
}

// Reads the first u32 of this entry for Get(0), reads the u32 after that for Get(1), and so on
u32 CFileInfoGCWii::Get(int n) const
{
  return Common::swap32(m_fst + FST_ENTRY_SIZE * m_index + sizeof(u32) * n);
}

u32 CFileInfoGCWii::GetRawNameOffset() const
{
  return Get(0);
}

u32 CFileInfoGCWii::GetRawOffset() const
{
  return Get(1);
}

u32 CFileInfoGCWii::GetSize() const
{
  return Get(2);
}

u64 CFileInfoGCWii::GetOffset() const
{
  return (u64)GetRawOffset() << (m_wii ? 2 : 0);
}

bool CFileInfoGCWii::IsDirectory() const
{
  return (GetRawNameOffset() & 0xFF000000) != 0;
}

u32 CFileInfoGCWii::GetTotalChildren() const
{
  return GetSize() - (m_index + 1);
}

u64 CFileInfoGCWii::GetNameOffset() const
{
  return static_cast<u64>(FST_ENTRY_SIZE) * m_total_file_infos + (GetRawNameOffset() & 0xFFFFFF);
}

std::string CFileInfoGCWii::GetName() const
{
  // TODO: Should we really always use SHIFT-JIS?
  // Some names in Pikmin (NTSC-U) don't make sense without it, but is it correct?
  return SHIFTJISToUTF8(reinterpret_cast<const char*>(m_fst + GetNameOffset()));
}

std::string CFileInfoGCWii::GetPath() const
{
  // The root entry doesn't have a name
  if (m_index == 0)
    return "";

  if (IsDirectory())
  {
    // The index of the parent directory is available through GetRawOffset().
    return CFileInfoGCWii(*this, GetRawOffset()).GetPath() + GetName() + "/";
  }
  else
  {
    // The parent directory can be found by searching backwards
    // for a directory that contains this file. The search cannot fail,
    // because the root directory at index 0 contains all files.
    CFileInfoGCWii potential_parent(*this, m_index - 1);
    while (!(potential_parent.IsDirectory() && potential_parent.GetSize() > m_index))
      potential_parent = CFileInfoGCWii(*this, potential_parent.m_index - 1);
    return potential_parent.GetPath() + GetName();
  }
}

bool CFileInfoGCWii::IsValid(u64 fst_size, const CFileInfoGCWii& parent_directory) const
{
  if (GetNameOffset() >= fst_size)
  {
    ERROR_LOG(DISCIO, "Impossibly large name offset in file system");
    return false;
  }

  if (IsDirectory())
  {
    if (GetRawOffset() != parent_directory.m_index)
    {
      ERROR_LOG(DISCIO, "Incorrect parent offset in file system");
      return false;
    }

    u32 size = GetSize();

    if (size <= m_index)
    {
      ERROR_LOG(DISCIO, "Impossibly small directory size in file system");
      return false;
    }

    if (size > parent_directory.GetSize())
    {
      ERROR_LOG(DISCIO, "Impossibly large directory size in file system");
      return false;
    }

    for (const IFileInfo& child : *this)
    {
      if (!reinterpret_cast<const CFileInfoGCWii&>(child).IsValid(fst_size, *this))
        return false;
    }
  }

  return true;
}

CFileSystemGCWii::CFileSystemGCWii(const IVolume* _rVolume)
    : IFileSystem(_rVolume), m_Valid(false), m_Wii(false), m_root(nullptr, false, 0, 0)
{
  // Check if this is a GameCube or Wii disc
  u32 magic_bytes;
  if (m_rVolume->ReadSwapped(0x18, &magic_bytes, false) && magic_bytes == 0x5D1C9EA3)
    m_Wii = true;
  else if (m_rVolume->ReadSwapped(0x1c, &magic_bytes, false) && magic_bytes == 0xC2339F3D)
    m_Wii = false;
  else
    return;

  u32 fst_offset_unshifted, fst_size_unshifted;
  if (!m_rVolume->ReadSwapped(0x424, &fst_offset_unshifted, m_Wii) ||
      !m_rVolume->ReadSwapped(0x428, &fst_size_unshifted, m_Wii))
    return;
  u64 fst_offset = static_cast<u64>(fst_offset_unshifted) << GetOffsetShift();
  u64 fst_size = static_cast<u64>(fst_size_unshifted) << GetOffsetShift();
  if (fst_size < FST_ENTRY_SIZE)
  {
    ERROR_LOG(DISCIO, "File system is too small");
    return;
  }

  // 128 MiB is more than the total amount of RAM in a Wii.
  // No file system should use anywhere near that much.
  static const u32 ARBITRARY_FILE_SYSTEM_SIZE_LIMIT = 128 * 1024 * 1024;
  if (fst_size > ARBITRARY_FILE_SYSTEM_SIZE_LIMIT)
  {
    // Without this check, Dolphin can crash by trying to allocate too much
    // memory when loading a disc image with an incorrect FST size.

    ERROR_LOG(DISCIO, "File system is abnormally large! Aborting loading");
    return;
  }

  // Read the whole FST
  m_file_system_table.resize(fst_size);
  if (!m_rVolume->Read(fst_offset, fst_size, m_file_system_table.data(), m_Wii))
  {
    ERROR_LOG(DISCIO, "Couldn't read file system table");
    return;
  }

  // Create the root object
  m_root = CFileInfoGCWii(m_file_system_table.data(), m_Wii);
  if (!m_root.IsDirectory())
  {
    ERROR_LOG(DISCIO, "File system root is not a directory");
    return;
  }

  if (FST_ENTRY_SIZE * m_root.GetSize() > fst_size)
  {
    ERROR_LOG(DISCIO, "File system has too many entries for its size");
    return;
  }

  // If the FST's final byte isn't 0, CFileInfoGCWii::GetName() can read past the end
  if (m_file_system_table[fst_size - 1] != 0)
  {
    ERROR_LOG(DISCIO, "File system does not end with a null byte");
    return;
  }

  m_Valid = m_root.IsValid(fst_size, m_root);
}

CFileSystemGCWii::~CFileSystemGCWii()
{
}

const IFileInfo& CFileSystemGCWii::GetRoot() const
{
  return m_root;
}

std::unique_ptr<IFileInfo> CFileSystemGCWii::FindFileInfo(const std::string& path) const
{
  if (!IsValid())
    return nullptr;

  return FindFileInfo(path, m_root);
}

std::unique_ptr<IFileInfo> CFileSystemGCWii::FindFileInfo(const std::string& path, const IFileInfo& file_info) const
{
  // Given a path like "directory1/directory2/fileA.bin", this method will
  // find directory1 and then call itself to search for "directory2/fileA.bin".

  // If there's nothing left to search for, return the current entry
  if (path.empty())
    return file_info.clone();

  // It's only possible to search in directories. Searching in a file is an error
  if (!file_info.IsDirectory())
    return nullptr;

  std::string searching_for;
  std::string rest_of_path;
  size_t first_dir_separator = path.find('/');
  if (first_dir_separator != std::string::npos)
  {
    searching_for = path.substr(0, first_dir_separator);
    rest_of_path = path.substr(first_dir_separator + 1);
  }
  else
  {
    searching_for = path;
    rest_of_path = "";
  }

  for (const IFileInfo& child : file_info)
  {
    if (child.GetName() == searching_for)
    {
      // A match is found. The rest of the path is passed on to finish the search.
      std::unique_ptr<IFileInfo> result = FindFileInfo(rest_of_path, child);

      // If the search wasn't sucessful, the loop continues. It's possible
      // but unlikely that there's a second file info that matches searching_for.
      if (result)
        return result;
    }
  }

  return nullptr;
}

std::unique_ptr<IFileInfo> CFileSystemGCWii::FindFileInfo(u64 disc_offset) const
{
  if (!IsValid())
    return nullptr;

  // Build a cache (unless there already is one)
  if (m_offset_file_info_cache.empty())
  {
    u32 fst_entries = m_root.GetSize();
    for (u32 i = 0; i < fst_entries; i++)
    {
      CFileInfoGCWii file_info(m_root, i);
      if (!file_info.IsDirectory())
        m_offset_file_info_cache.emplace(file_info.GetOffset() + file_info.GetSize(), i);
    }
  }

  // Get the first file that ends after disc_offset
  auto it = m_offset_file_info_cache.upper_bound(disc_offset);
  if (it == m_offset_file_info_cache.end())
    return nullptr;
  std::unique_ptr<IFileInfo> result(std::make_unique<CFileInfoGCWii>(m_root, it->second));

  // If the file's start isn't after disc_offset, success
  if (result->GetOffset() <= disc_offset)
    return result;

  return nullptr;
}

u64 CFileSystemGCWii::ReadFile(const IFileInfo* file_info, u8* _pBuffer, u64 _MaxBufferSize,
                               u64 _OffsetInFile) const
{
  if (!file_info || file_info->IsDirectory())
    return 0;

  if (_OffsetInFile >= file_info->GetSize())
    return 0;

  u64 read_length = std::min(_MaxBufferSize, file_info->GetSize() - _OffsetInFile);

  DEBUG_LOG(DISCIO, "Reading %" PRIx64 " bytes at %" PRIx64 " from file %s. Offset: %" PRIx64
                    " Size: %" PRIx64,
            read_length, _OffsetInFile, file_info->GetPath().c_str(), file_info->GetOffset(),
            file_info->GetSize());

  m_rVolume->Read(file_info->GetOffset() + _OffsetInFile, read_length, _pBuffer, m_Wii);
  return read_length;
}

bool CFileSystemGCWii::ExportFile(const IFileInfo* file_info, const std::string& _rExportFilename) const
{
  if (!file_info || file_info->IsDirectory())
    return false;

  u64 remainingSize = file_info->GetSize();
  u64 fileOffset = file_info->GetOffset();

  File::IOFile f(_rExportFilename, "wb");
  if (!f)
    return false;

  bool result = true;

  while (remainingSize)
  {
    // Limit read size to 128 MB
    size_t readSize = (size_t)std::min(remainingSize, (u64)0x08000000);

    std::vector<u8> buffer(readSize);

    result = m_rVolume->Read(fileOffset, readSize, &buffer[0], m_Wii);

    if (!result)
      break;

    f.WriteBytes(&buffer[0], readSize);

    remainingSize -= readSize;
    fileOffset += readSize;
  }

  return result;
}

bool CFileSystemGCWii::ExportApploader(const std::string& _rExportFolder) const
{
  u32 apploader_size;
  u32 trailer_size;
  const u32 header_size = 0x20;
  if (!m_rVolume->ReadSwapped(0x2440 + 0x14, &apploader_size, m_Wii) ||
      !m_rVolume->ReadSwapped(0x2440 + 0x18, &trailer_size, m_Wii))
    return false;
  apploader_size += trailer_size + header_size;
  DEBUG_LOG(DISCIO, "Apploader size -> %x", apploader_size);

  std::vector<u8> buffer(apploader_size);
  if (m_rVolume->Read(0x2440, apploader_size, buffer.data(), m_Wii))
  {
    std::string exportName(_rExportFolder + "/apploader.img");

    File::IOFile AppFile(exportName, "wb");
    if (AppFile)
    {
      AppFile.WriteBytes(buffer.data(), apploader_size);
      return true;
    }
  }

  return false;
}

u64 CFileSystemGCWii::GetBootDOLOffset() const
{
  u32 offset = 0;
  m_rVolume->ReadSwapped(0x420, &offset, m_Wii);
  return static_cast<u64>(offset) << GetOffsetShift();
}

u32 CFileSystemGCWii::GetBootDOLSize(u64 dol_offset) const
{
  // The dol_offset value is usually obtained by calling GetBootDOLOffset.
  // If GetBootDOLOffset fails by returning 0, GetBootDOLSize should also fail.
  if (dol_offset == 0)
    return 0;

  u32 dol_size = 0;
  u32 offset = 0;
  u32 size = 0;

  // Iterate through the 7 code segments
  for (u8 i = 0; i < 7; i++)
  {
    if (!m_rVolume->ReadSwapped(dol_offset + 0x00 + i * 4, &offset, m_Wii) ||
        !m_rVolume->ReadSwapped(dol_offset + 0x90 + i * 4, &size, m_Wii))
      return 0;
    dol_size = std::max(offset + size, dol_size);
  }

  // Iterate through the 11 data segments
  for (u8 i = 0; i < 11; i++)
  {
    if (!m_rVolume->ReadSwapped(dol_offset + 0x1c + i * 4, &offset, m_Wii) ||
        !m_rVolume->ReadSwapped(dol_offset + 0xac + i * 4, &size, m_Wii))
      return 0;
    dol_size = std::max(offset + size, dol_size);
  }

  return dol_size;
}

bool CFileSystemGCWii::ExportDOL(const std::string& _rExportFolder) const
{
  u64 DolOffset = GetBootDOLOffset();
  u32 DolSize = GetBootDOLSize(DolOffset);

  if (DolOffset == 0 || DolSize == 0)
    return false;

  std::vector<u8> buffer(DolSize);
  if (m_rVolume->Read(DolOffset, DolSize, &buffer[0], m_Wii))
  {
    std::string exportName(_rExportFolder + "/boot.dol");

    File::IOFile DolFile(exportName, "wb");
    if (DolFile)
    {
      DolFile.WriteBytes(&buffer[0], DolSize);
      return true;
    }
  }

  return false;
}

u32 CFileSystemGCWii::GetOffsetShift() const
{
  return m_Wii ? 2 : 0;
}

}  // namespace
