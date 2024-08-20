// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/FileSystemGCWii.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeDisc.h"

namespace DiscIO
{
constexpr u32 FST_ENTRY_SIZE = 4 * 3;  // An FST entry consists of three 32-bit integers

// Set everything manually.
FileInfoGCWii::FileInfoGCWii(const u8* fst, u8 offset_shift, u32 index, u32 total_file_infos)
    : m_fst(fst), m_offset_shift(offset_shift), m_index(index), m_total_file_infos(total_file_infos)
{
}

// For the root object only.
// m_fst and m_index must be correctly set before GetSize() is called!
FileInfoGCWii::FileInfoGCWii(const u8* fst, u8 offset_shift)
    : m_fst(fst), m_offset_shift(offset_shift), m_index(0), m_total_file_infos(GetSize())
{
}

// Copy data that is common to the whole file system.
FileInfoGCWii::FileInfoGCWii(const FileInfoGCWii& file_info, u32 index)
    : FileInfoGCWii(file_info.m_fst, file_info.m_offset_shift, index, file_info.m_total_file_infos)
{
}

FileInfoGCWii::~FileInfoGCWii() = default;

uintptr_t FileInfoGCWii::GetAddress() const
{
  return reinterpret_cast<uintptr_t>(m_fst + FST_ENTRY_SIZE * m_index);
}

u32 FileInfoGCWii::GetNextIndex() const
{
  return IsDirectory() ? GetSize() : m_index + 1;
}

FileInfo& FileInfoGCWii::operator++()
{
  m_index = GetNextIndex();
  return *this;
}

std::unique_ptr<FileInfo> FileInfoGCWii::clone() const
{
  return std::make_unique<FileInfoGCWii>(*this);
}

FileInfo::const_iterator FileInfoGCWii::begin() const
{
  return const_iterator(std::make_unique<FileInfoGCWii>(*this, m_index + 1));
}

FileInfo::const_iterator FileInfoGCWii::end() const
{
  return const_iterator(std::make_unique<FileInfoGCWii>(*this, GetNextIndex()));
}

u32 FileInfoGCWii::Get(EntryProperty entry_property) const
{
  return Common::swap32(m_fst + FST_ENTRY_SIZE * m_index +
                        sizeof(u32) * static_cast<int>(entry_property));
}

u32 FileInfoGCWii::GetSize() const
{
  return Get(EntryProperty::FILE_SIZE);
}

u64 FileInfoGCWii::GetOffset() const
{
  return static_cast<u64>(Get(EntryProperty::FILE_OFFSET)) << m_offset_shift;
}

bool FileInfoGCWii::IsRoot() const
{
  return m_index == 0;
}

bool FileInfoGCWii::IsDirectory() const
{
  return (Get(EntryProperty::NAME_OFFSET) & 0xFF000000) != 0;
}

u32 FileInfoGCWii::GetTotalChildren() const
{
  return Get(EntryProperty::FILE_SIZE) - (m_index + 1);
}

u64 FileInfoGCWii::GetNameOffset() const
{
  return static_cast<u64>(FST_ENTRY_SIZE) * m_total_file_infos +
         (Get(EntryProperty::NAME_OFFSET) & 0xFFFFFF);
}

std::string FileInfoGCWii::GetName() const
{
  // TODO: Should we really always use SHIFT-JIS?
  // Some names in Pikmin (NTSC-U) don't make sense without it, but is it correct?
  return SHIFTJISToUTF8(reinterpret_cast<const char*>(m_fst + GetNameOffset()));
}

bool FileInfoGCWii::NameCaseInsensitiveEquals(std::string_view other) const
{
  // For speed, this function avoids allocating new strings, except when we are comparing
  // non-ASCII characters with non-ASCII characters, which is a rare case.

  const char* this_ptr = reinterpret_cast<const char*>(m_fst + GetNameOffset());
  const char* other_ptr = other.data();

  for (size_t i = 0; i < other.size(); ++i, ++this_ptr, ++other_ptr)
  {
    if (*this_ptr == '\0')
    {
      // A null byte in this is always a terminator and a null byte in other is never a terminator,
      // so if we reach this case, this is shorter than other
      return false;
    }
    else if (static_cast<unsigned char>(*this_ptr) >= 0x80 &&
             static_cast<unsigned char>(*other_ptr) >= 0x80)
    {
      // other is in UTF-8 and this is in Shift-JIS, so we convert so that we can compare correctly
      const std::string this_utf8 = SHIFTJISToUTF8(this_ptr);
      return std::equal(this_utf8.cbegin(), this_utf8.cend(), other.cbegin() + i, other.cend(),
                        [](char a, char b) { return Common::ToLower(a) == Common::ToLower(b); });
    }
    else if (Common::ToLower(*this_ptr) != Common::ToLower(*other_ptr))
    {
      return false;
    }
  }

  return *this_ptr == '\0';  // If we're not at a null byte, this is longer than other
}

std::string FileInfoGCWii::GetPath() const
{
  // The root entry doesn't have a name
  if (m_index == 0)
    return "";

  if (IsDirectory())
  {
    u32 parent_directory_index = Get(EntryProperty::FILE_OFFSET);
    return FileInfoGCWii(*this, parent_directory_index).GetPath() + GetName() + "/";
  }
  else
  {
    // The parent directory can be found by searching backwards
    // for a directory that contains this file. The search cannot fail,
    // because the root directory at index 0 contains all files.
    FileInfoGCWii potential_parent(*this, m_index - 1);
    while (!(potential_parent.IsDirectory() &&
             potential_parent.Get(EntryProperty::FILE_SIZE) > m_index))
    {
      potential_parent = FileInfoGCWii(*this, potential_parent.m_index - 1);
    }
    return potential_parent.GetPath() + GetName();
  }
}

bool FileInfoGCWii::IsValid(u64 fst_size, const FileInfoGCWii& parent_directory) const
{
  if (GetNameOffset() >= fst_size)
  {
    ERROR_LOG_FMT(DISCIO, "Impossibly large name offset in file system");
    return false;
  }

  if (IsDirectory())
  {
    if (Get(EntryProperty::FILE_OFFSET) != parent_directory.m_index)
    {
      ERROR_LOG_FMT(DISCIO, "Incorrect parent offset in file system");
      return false;
    }

    const u32 size = Get(EntryProperty::FILE_SIZE);

    if (size <= m_index)
    {
      ERROR_LOG_FMT(DISCIO, "Impossibly small directory size in file system");
      return false;
    }

    if (size > parent_directory.Get(EntryProperty::FILE_SIZE))
    {
      ERROR_LOG_FMT(DISCIO, "Impossibly large directory size in file system");
      return false;
    }

    for (const FileInfo& child : *this)
    {
      if (!static_cast<const FileInfoGCWii&>(child).IsValid(fst_size, *this))
        return false;
    }
  }

  return true;
}

FileSystemGCWii::FileSystemGCWii(const VolumeDisc* volume, const Partition& partition)
    : m_valid(false), m_root(nullptr, 0, 0, 0)
{
  u8 offset_shift;
  // Check if this is a GameCube or Wii disc
  if (volume->ReadSwapped<u32>(0x18, partition) == WII_DISC_MAGIC)
    offset_shift = 2;  // Wii file system
  else if (volume->ReadSwapped<u32>(0x1c, partition) == GAMECUBE_DISC_MAGIC)
    offset_shift = 0;  // GameCube file system
  else
    return;  // Invalid partition (maybe someone removed its data but not its partition table entry)

  const std::optional<u64> fst_offset = GetFSTOffset(*volume, partition);
  const std::optional<u64> fst_size = GetFSTSize(*volume, partition);
  if (!fst_offset || !fst_size)
    return;
  if (*fst_size < FST_ENTRY_SIZE)
  {
    ERROR_LOG_FMT(DISCIO, "File system is too small");
    return;
  }

  // 128 MiB is more than the total amount of RAM in a Wii.
  // No file system should use anywhere near that much.
  static const u32 ARBITRARY_FILE_SYSTEM_SIZE_LIMIT = 128 * 1024 * 1024;
  if (*fst_size > ARBITRARY_FILE_SYSTEM_SIZE_LIMIT)
  {
    // Without this check, Dolphin can crash by trying to allocate too much
    // memory when loading a disc image with an incorrect FST size.

    ERROR_LOG_FMT(DISCIO, "File system is abnormally large! Aborting loading");
    return;
  }

  // Read the whole FST
  m_file_system_table.resize(*fst_size);
  if (!volume->Read(*fst_offset, *fst_size, m_file_system_table.data(), partition))
  {
    ERROR_LOG_FMT(DISCIO, "Couldn't read file system table");
    return;
  }

  // Create the root object
  m_root = FileInfoGCWii(m_file_system_table.data(), offset_shift);
  if (!m_root.IsDirectory())
  {
    ERROR_LOG_FMT(DISCIO, "File system root is not a directory");
    return;
  }

  if (FST_ENTRY_SIZE * m_root.GetSize() > *fst_size)
  {
    ERROR_LOG_FMT(DISCIO, "File system has too many entries for its size");
    return;
  }

  // If the FST's final byte isn't 0, FileInfoGCWii::GetName() can read past the end
  if (m_file_system_table[*fst_size - 1] != 0)
  {
    ERROR_LOG_FMT(DISCIO, "File system does not end with a null byte");
    return;
  }

  m_valid = m_root.IsValid(*fst_size, m_root);
}

FileSystemGCWii::~FileSystemGCWii() = default;

const FileInfo& FileSystemGCWii::GetRoot() const
{
  return m_root;
}

std::unique_ptr<FileInfo> FileSystemGCWii::FindFileInfo(std::string_view path) const
{
  if (!IsValid())
    return nullptr;

  return FindFileInfo(path, m_root);
}

std::unique_ptr<FileInfo> FileSystemGCWii::FindFileInfo(std::string_view path,
                                                        const FileInfo& file_info) const
{
  // Given a path like "directory1/directory2/fileA.bin", this function will
  // find directory1 and then call itself to search for "directory2/fileA.bin".

  const size_t name_start = path.find_first_not_of('/');
  if (name_start == std::string::npos)
    return file_info.clone();  // We're done

  const size_t name_end = path.find('/', name_start);
  const std::string_view name = path.substr(name_start, name_end - name_start);
  const std::string_view rest_of_path =
      (name_end != std::string::npos) ? path.substr(name_end + 1) : "";

  for (const FileInfo& child : file_info)
  {
    // We need case insensitive comparison since some games have OPENING.BNR instead of opening.bnr
    if (child.NameCaseInsensitiveEquals(name))
    {
      // A match is found. The rest of the path is passed on to finish the search.
      if (std::unique_ptr<FileInfo> result = FindFileInfo(rest_of_path, child))
        return result;

      // The search wasn't successful, the loop continues, just in case there's a second
      // file info that matches searching_for (which probably won't happen in practice)
    }
  }

  return nullptr;
}

std::unique_ptr<FileInfo> FileSystemGCWii::FindFileInfo(u64 disc_offset) const
{
  if (!IsValid())
    return nullptr;

  // Build a cache (unless there already is one)
  if (m_offset_file_info_cache.empty())
  {
    u32 fst_entries = m_root.GetSize();
    for (u32 i = 0; i < fst_entries; i++)
    {
      FileInfoGCWii file_info(m_root, i);
      if (!file_info.IsDirectory())
      {
        const u32 size = file_info.GetSize();
        if (size != 0)
          m_offset_file_info_cache.emplace(file_info.GetOffset() + size, i);
      }
    }
  }

  // Get the first file that ends after disc_offset
  const auto it = m_offset_file_info_cache.upper_bound(disc_offset);
  if (it == m_offset_file_info_cache.end())
    return nullptr;
  std::unique_ptr<FileInfo> result(std::make_unique<FileInfoGCWii>(m_root, it->second));

  // If the file's start isn't after disc_offset, success
  if (result->GetOffset() <= disc_offset)
    return result;

  return nullptr;
}

}  // namespace DiscIO
