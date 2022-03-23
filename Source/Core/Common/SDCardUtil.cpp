// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/SDCardUtil.h"

#include <algorithm>
// Inclusion order matters
// clang-format off
#include <ff.h>
#include <diskio.h>
// clang-format on

#include "Common/Align.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

enum : u32
{
  SECTOR_SIZE = 512,
  MAX_CLUSTER_SIZE = 64 * SECTOR_SIZE,
};

static File::IOFile s_image;
static std::vector<u8> s_buffer(MAX_CLUSTER_SIZE);

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
  return 0;
}

extern "C" DSTATUS disk_status(BYTE pdrv)
{
  return 0;
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE;
  if (!s_image.Seek(offset, File::SeekOrigin::Begin))
  {
    ERROR_LOG_FMT(COMMON, "SD image seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  if (!s_image.ReadBytes(buff, size))
  {
    ERROR_LOG_FMT(COMMON, "SD image read failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE;
  if (!s_image.Seek(offset, File::SeekOrigin::Begin))
  {
    ERROR_LOG_FMT(COMMON, "SD image seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  if (!s_image.WriteBytes(buff, size))
  {
    ERROR_LOG_FMT(COMMON, "SD image write failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  switch (cmd)
  {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    *reinterpret_cast<LBA_t*>(buff) = s_image.GetSize() / SECTOR_SIZE;
    return RES_OK;
  default:
    WARN_LOG_FMT(COMMON, "Unexpected SD image ioctl {}", cmd);
    return RES_OK;
  }
}

extern "C" DWORD get_fattime(void)
{
  const std::time_t time = std::time(nullptr);
  std::tm tm;
#ifdef _WIN32
  localtime_s(&tm, &time);
#else
  localtime_r(&time, &tm);
#endif

  DWORD fattime = 0;
  fattime |= (tm.tm_year - 80) << 25;
  fattime |= (tm.tm_mon + 1) << 21;
  fattime |= tm.tm_mday << 16;
  fattime |= tm.tm_hour << 11;
  fattime |= tm.tm_min << 5;
  fattime |= std::min(tm.tm_sec, 59) >> 1;
  return fattime;
}

namespace Common
{
static u64 MebibytesToBytes(u64 mebibytes)
{
  return mebibytes * 1024 * 1024;
}

static u64 GibibytesToBytes(u64 gibibytes)
{
  return gibibytes * 1024 * 1024 * 1024;
}

static bool Check(const File::FSTEntry& entry)
{
  if (!entry.isDirectory)
  {
    return true;
  }

  if (entry.children.size() > 65536)
  {
    ERROR_LOG_FMT(COMMON, "Directory {} has too many entries ({})", entry.physicalName,
                  entry.children.size());
    return false;
  }

  for (const File::FSTEntry& child : entry.children)
  {
    const size_t size = UTF8ToUTF16(child.virtualName).size();
    if (size > 255)
    {
      ERROR_LOG_FMT(COMMON, "Filename {0} (in directory {1}) is too long ({2})", child.virtualName,
                    entry.physicalName, size);
      return false;
    }

    if (child.size >= GibibytesToBytes(4))
    {
      ERROR_LOG_FMT(COMMON, "File {0} (in directory {1}) is too large ({2})", child.virtualName,
                    entry.physicalName, child.size);
      return false;
    }

    if (!Check(child))
    {
      return false;
    }
  }

  return true;
}

static u64 GetSize(const File::FSTEntry& entry)
{
  if (!entry.isDirectory)
  {
    return AlignUp(entry.size, MAX_CLUSTER_SIZE);
  }

  u64 size = 0;
  for (const File::FSTEntry& child : entry.children)
  {
    size += 32;
    // For simplicity, assume that all names are LFN.
    const u64 num_lfn_entries = (UTF8ToUTF16(child.virtualName).size() + 13 - 1) / 13;
    size += num_lfn_entries * 32;
  }
  size = AlignUp(size, MAX_CLUSTER_SIZE);

  for (const File::FSTEntry& child : entry.children)
  {
    size += GetSize(child);
  }

  return size;
}

static bool Pack(const File::FSTEntry& entry, bool is_root)
{
  if (!entry.isDirectory)
  {
    File::IOFile src(entry.physicalName, "rb");
    if (!src)
    {
      return false;
    }

    FIL dst;
    if (f_open(&dst, entry.virtualName.c_str(), FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
      return false;
    }

    if (src.GetSize() != entry.size)
    {
      return false;
    }
    u32 size = src.GetSize();
    while (size > 0)
    {
      u32 chunk_size = std::min(size, static_cast<u32>(s_buffer.size()));
      if (!src.ReadBytes(s_buffer.data(), chunk_size))
      {
        return false;
      }
      u32 written_size;
      if (f_write(&dst, s_buffer.data(), chunk_size, &written_size) != FR_OK)
      {
        return false;
      }
      if (written_size != chunk_size)
      {
        return false;
      }
      size -= chunk_size;
    }

    if (f_close(&dst) != FR_OK)
    {
      return false;
    }
    if (!src.Close())
    {
      return false;
    }

    return true;
  }

  if (!is_root)
  {
    if (f_mkdir(entry.virtualName.c_str()) != FR_OK)
    {
      return false;
    }
    if (f_chdir(entry.virtualName.c_str()) != FR_OK)
    {
      return false;
    }
  }

  for (const File::FSTEntry& child : entry.children)
  {
    if (!Pack(child, false))
    {
      return false;
    }
  }

  if (!is_root)
  {
    if (f_chdir("..") != FR_OK)
    {
      return false;
    }
  }

  return true;
}

bool SDPack()
{
  const std::string root_path = File::GetUserPath(D_SDFOLDER_IDX);
  if (!File::IsDirectory(root_path))
  {
    return false;
  }
  const File::FSTEntry root = File::ScanDirectoryTree(root_path, true);

  if (!Check(root))
  {
    return false;
  }

  u64 size = GetSize(root);
  // Allocate a reasonable amount of free space
  size += std::clamp(size / 2, MebibytesToBytes(512), GibibytesToBytes(8));
  size = AlignUp(size, MAX_CLUSTER_SIZE);

  const std::string image_path = File::GetUserPath(F_WIISDIMAGE_IDX);
  const std::string temp_image_path = File::GetTempFilenameForAtomicWrite(image_path);
  if (!s_image.Open(temp_image_path, "w+b"))
  {
    ERROR_LOG_FMT(COMMON, "Failed to open SD image");
    return false;
  }

  if (!s_image.Resize(size))
  {
    ERROR_LOG_FMT(COMMON, "Failed to allocate space for SD image");
    s_image.Close();
    File::Delete(temp_image_path);
    return false;
  }

  MKFS_PARM options = {};
  options.fmt = FM_FAT32;
  options.n_fat = 0;    // Number of FATs: automatic
  options.align = 1;    // Alignment of the data region (in sectors)
  options.n_root = 0;   // Number of root directory entries: automatic (and unused for FAT32)
  options.au_size = 0;  // Cluster size: automatic
  if (f_mkfs("", &options, s_buffer.data(), static_cast<UINT>(s_buffer.size())) != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to initialize SD image filesystem");
    s_image.Close();
    File::Delete(temp_image_path);
    return false;
  }

  FATFS fs;
  f_mount(&fs, "", 0);

  if (!Pack(root, true))
  {
    ERROR_LOG_FMT(COMMON, "Failed to pack SD image");
    s_image.Close();
    File::Delete(temp_image_path);
    return false;
  }

  f_unmount("");

  if (!s_image.Close())
  {
    ERROR_LOG_FMT(COMMON, "Failed to close SD image");
    return false;
  }
  if (!File::Rename(temp_image_path, image_path))
  {
    ERROR_LOG_FMT(COMMON, "Failed to rename SD image");
    return false;
  }

  INFO_LOG_FMT(COMMON, "Successfully packed SD image");
  return true;
}

static bool Unpack(const std::string path, bool is_directory, const char* name)
{
  if (!is_directory)
  {
    FIL src;
    if (f_open(&src, name, FA_READ) != FR_OK)
    {
      return false;
    }

    File::IOFile dst(path, "wb");
    if (!dst)
    {
      return false;
    }

    u32 size = f_size(&src);
    while (size > 0)
    {
      u32 chunk_size = std::min(size, static_cast<u32>(s_buffer.size()));
      u32 read_size;
      if (f_read(&src, s_buffer.data(), chunk_size, &read_size) != FR_OK)
      {
        return false;
      }
      if (read_size != chunk_size)
      {
        return false;
      }
      if (!dst.WriteBytes(s_buffer.data(), chunk_size))
      {
        return false;
      }
      size -= chunk_size;
    }

    if (!dst.Close())
    {
      return false;
    }
    if (f_close(&src) != FR_OK)
    {
      return false;
    }

    return true;
  }

  if (!File::CreateDir(path))
  {
    return false;
  }

  if (f_chdir(name) != FR_OK)
  {
    return false;
  }

  DIR directory;
  if (f_opendir(&directory, ".") != FR_OK)
  {
    return false;
  }

  FILINFO entry;
  while (true)
  {
    if (f_readdir(&directory, &entry) != FR_OK)
    {
      return false;
    }

    if (entry.fname[0] == '\0')
    {
      break;
    }

    if (!Unpack(path + "/" + entry.fname, entry.fattrib & AM_DIR, entry.fname))
    {
      return false;
    }
  }

  if (f_closedir(&directory) != FR_OK)
  {
    return false;
  }

  if (f_chdir("..") != FR_OK)
  {
    return false;
  }

  return true;
}

bool SDUnpack()
{
  const std::string image_path = File::GetUserPath(F_WIISDIMAGE_IDX);
  if (!s_image.Open(image_path, "r+b"))
  {
    ERROR_LOG_FMT(COMMON, "Failed to open SD image");
    return false;
  }

  FATFS fs;
  f_mount(&fs, "", 0);

  // Most systems don't offer atomic directory renaming, so it's simpler to directly work on the
  // actual one and rollback if needed.
  const std::string root_path = File::GetUserPath(D_SDFOLDER_IDX);
  File::CreateDir(root_path);
  const std::string temp_root_path = File::GetTempFilenameForAtomicWrite(root_path);
  if (!File::Rename(root_path, temp_root_path))
  {
    ERROR_LOG_FMT(COMMON, "Failed to backup SD folder");
    return false;
  }

  if (!Unpack(root_path, true, ""))
  {
    ERROR_LOG_FMT(COMMON, "Failed to unpack SD image");
    File::DeleteDirRecursively(root_path);
    File::Rename(temp_root_path, root_path);
    return false;
  }

  f_unmount("");

  if (!s_image.Close())
  {
    ERROR_LOG_FMT(COMMON, "Failed to close SD image");
    File::DeleteDirRecursively(root_path);
    File::Rename(temp_root_path, root_path);
    return false;
  }

  File::DeleteDirRecursively(temp_root_path);
  INFO_LOG_FMT(COMMON, "Successfully unpacked SD image");
  return true;
}
}  // namespace Common
