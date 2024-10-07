// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FatFsUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

// Does not compile if diskio.h is included first.
// clang-format off
#include "ff.h"
#include "diskio.h"
// clang-format on

#include "Common/Align.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"

enum : u32
{
  SECTOR_SIZE = 512,
  MAX_CLUSTER_SIZE = 64 * SECTOR_SIZE,
};

static std::mutex s_fatfs_mutex;
static Common::FatFsCallbacks* s_callbacks;

namespace
{
int SDCardDiskRead(File::IOFile* image, u8 pdrv, u8* buff, u32 sector, unsigned int count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE;
  if (!image->Seek(offset, File::SeekOrigin::Begin))
  {
    ERROR_LOG_FMT(COMMON, "SD image seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  if (!image->ReadBytes(buff, size))
  {
    ERROR_LOG_FMT(COMMON, "SD image read failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

int SDCardDiskWrite(File::IOFile* image, u8 pdrv, const u8* buff, u32 sector, unsigned int count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE;
  if (!image->Seek(offset, File::SeekOrigin::Begin))
  {
    ERROR_LOG_FMT(COMMON, "SD image seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  if (!image->WriteBytes(buff, size))
  {
    ERROR_LOG_FMT(COMMON, "SD image write failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

int SDCardDiskIOCtl(File::IOFile* image, u8 pdrv, u8 cmd, void* buff)
{
  switch (cmd)
  {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    *static_cast<LBA_t*>(buff) = image->GetSize() / SECTOR_SIZE;
    return RES_OK;
  default:
    WARN_LOG_FMT(COMMON, "Unexpected SD image ioctl {}", cmd);
    return RES_OK;
  }
}

u32 GetSystemTimeFAT()
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
}  // namespace

namespace Common
{
FatFsCallbacks::FatFsCallbacks() = default;
FatFsCallbacks::~FatFsCallbacks() = default;

u8 FatFsCallbacks::DiskInitialize(u8 pdrv)
{
  return 0;
}

u8 FatFsCallbacks::DiskStatus(u8 pdrv)
{
  return 0;
}

u32 FatFsCallbacks::GetCurrentTimeFAT()
{
  return GetSystemTimeFAT();
}
}  // namespace Common

namespace
{
class SDCardFatFsCallbacks : public Common::FatFsCallbacks
{
public:
  int DiskRead(u8 pdrv, u8* buff, u32 sector, unsigned int count) override
  {
    return SDCardDiskRead(m_image, pdrv, buff, sector, count);
  }

  int DiskWrite(u8 pdrv, const u8* buff, u32 sector, unsigned int count) override
  {
    return SDCardDiskWrite(m_image, pdrv, buff, sector, count);
  }

  int DiskIOCtl(u8 pdrv, u8 cmd, void* buff) override
  {
    return SDCardDiskIOCtl(m_image, pdrv, cmd, buff);
  }

  u32 GetCurrentTimeFAT() override
  {
    if (m_deterministic)
      return 0;

    return GetSystemTimeFAT();
  }

  File::IOFile* m_image = nullptr;
  bool m_deterministic = false;
};
}  // namespace

extern "C" DSTATUS disk_status(BYTE pdrv)
{
  return static_cast<DSTATUS>(s_callbacks->DiskStatus(pdrv));
}

extern "C" DSTATUS disk_initialize(BYTE pdrv)
{
  return static_cast<DSTATUS>(s_callbacks->DiskInitialize(pdrv));
}

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
  return static_cast<DRESULT>(s_callbacks->DiskRead(pdrv, buff, sector, count));
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
  return static_cast<DRESULT>(s_callbacks->DiskWrite(pdrv, buff, sector, count));
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  return static_cast<DRESULT>(s_callbacks->DiskIOCtl(pdrv, cmd, buff));
}

extern "C" DWORD get_fattime(void)
{
  return static_cast<DWORD>(s_callbacks->GetCurrentTimeFAT());
}

#if FF_USE_LFN == 3  // match ff.h; currently unused by Dolphin
extern "C" void* ff_memalloc(UINT msize)
{
  return std::malloc(msize);
}

extern "C" void ff_memfree(void* mblock)
{
  return std::free(mblock);
}
#endif

#if FF_FS_REENTRANT
extern "C" int ff_cre_syncobj(BYTE vol, FF_SYNC_t* sobj)
{
  *sobj = new std::recursive_mutex();
  return *sobj != nullptr;
}

extern "C" int ff_req_grant(FF_SYNC_t sobj)
{
  std::recursive_mutex* m = reinterpret_cast<std::recursive_mutex*>(sobj);
  m->lock();
  return 1;
}

extern "C" void ff_rel_grant(FF_SYNC_t sobj)
{
  std::recursive_mutex* m = reinterpret_cast<std::recursive_mutex*>(sobj);
  m->unlock();
}

extern "C" int ff_del_syncobj(FF_SYNC_t sobj)
{
  delete reinterpret_cast<std::recursive_mutex*>(sobj);
  return 1;
}
#endif

static const char* FatFsErrorToString(FRESULT error_code)
{
  // These are taken from the comment next to each value in ff.h
  switch (error_code)
  {
  case FR_OK:
    return "Succeeded";
  case FR_DISK_ERR:
    return "A hard error occurred in the low level disk I/O layer";
  case FR_INT_ERR:
    return "Assertion failed";
  case FR_NOT_READY:
    return "The physical drive cannot work";
  case FR_NO_FILE:
    return "Could not find the file";
  case FR_NO_PATH:
    return "Could not find the path";
  case FR_INVALID_NAME:
    return "The path name format is invalid";
  case FR_DENIED:
    return "Access denied due to prohibited access or directory full";
  case FR_EXIST:
    return "Access denied due to prohibited access";
  case FR_INVALID_OBJECT:
    return "The file/directory object is invalid";
  case FR_WRITE_PROTECTED:
    return "The physical drive is write protected";
  case FR_INVALID_DRIVE:
    return "The logical drive number is invalid";
  case FR_NOT_ENABLED:
    return "The volume has no work area";
  case FR_NO_FILESYSTEM:
    return "There is no valid FAT volume";
  case FR_MKFS_ABORTED:
    return "The f_mkfs() aborted due to any problem";
  case FR_TIMEOUT:
    return "Could not get a grant to access the volume within defined period";
  case FR_LOCKED:
    return "The operation is rejected according to the file sharing policy";
  case FR_NOT_ENOUGH_CORE:
    return "LFN working buffer could not be allocated";
  case FR_TOO_MANY_OPEN_FILES:
    return "Number of open files > FF_FS_LOCK";
  case FR_INVALID_PARAMETER:
    return "Given parameter is invalid";
  default:
    return "Unknown error";
  }
}

namespace Common
{
static constexpr u64 MebibytesToBytes(u64 mebibytes)
{
  return mebibytes * 1024 * 1024;
}

static constexpr u64 GibibytesToBytes(u64 gibibytes)
{
  return gibibytes * 1024 * 1024 * 1024;
}

static bool CheckIfFATCompatible(const File::FSTEntry& entry)
{
  if (!entry.isDirectory)
    return true;

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

    if (!CheckIfFATCompatible(child))
      return false;
  }

  return true;
}

static u64 GetSize(const File::FSTEntry& entry)
{
  if (!entry.isDirectory)
    return AlignUp(entry.size, MAX_CLUSTER_SIZE);

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
    size += GetSize(child);

  return size;
}

static bool Pack(const std::function<bool()>& cancelled, const File::FSTEntry& entry, bool is_root,
                 std::vector<u8>& tmp_buffer)
{
  if (cancelled())
    return false;

  if (!entry.isDirectory)
  {
    File::IOFile src(entry.physicalName, "rb");
    if (!src)
    {
      ERROR_LOG_FMT(COMMON, "Failed to open file {}", entry.physicalName);
      return false;
    }

    FIL dst{};
    const auto open_error_code =
        f_open(&dst, entry.virtualName.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
    if (open_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to open file {} in SD image: {}", entry.physicalName,
                    FatFsErrorToString(open_error_code));
      return false;
    }

    const size_t src_size = src.GetSize();
    if (src.GetSize() != entry.size)
    {
      ERROR_LOG_FMT(COMMON, "File at {} does not match previously read filesize ({} != {})",
                    entry.physicalName, entry.size, src_size);
      return false;
    }

    if (entry.size >= GibibytesToBytes(4))
    {
      ERROR_LOG_FMT(COMMON, "File at {} is too large to fit into FAT ({} >= 4GiB)",
                    entry.physicalName, entry.size);
      return false;
    }

    u64 size = entry.size;
    while (size > 0)
    {
      if (cancelled())
        return false;

      u32 chunk_size = static_cast<u32>(std::min(size, static_cast<u64>(tmp_buffer.size())));
      if (!src.ReadBytes(tmp_buffer.data(), chunk_size))
      {
        ERROR_LOG_FMT(COMMON, "Failed to read data from file at {}", entry.physicalName);
        return false;
      }

      u32 written_size;
      const auto write_error_code = f_write(&dst, tmp_buffer.data(), chunk_size, &written_size);
      if (write_error_code != FR_OK)
      {
        ERROR_LOG_FMT(COMMON, "Failed to write file {} to SD image: {}", entry.physicalName,
                      FatFsErrorToString(write_error_code));
        return false;
      }

      if (written_size != chunk_size)
      {
        ERROR_LOG_FMT(COMMON, "Failed to write bytes of file {} to SD image ({} != {})",
                      entry.physicalName, written_size, chunk_size);
        return false;
      }

      size -= chunk_size;
    }

    const auto close_error_code = f_close(&dst);
    if (close_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to close file {} in SD image: {}", entry.physicalName,
                    FatFsErrorToString(close_error_code));
      return false;
    }

    if (!src.Close())
    {
      ERROR_LOG_FMT(COMMON, "Failed to close file {}", entry.physicalName);
      return false;
    }

    return true;
  }

  if (!is_root)
  {
    const auto mkdir_error_code = f_mkdir(entry.virtualName.c_str());
    if (mkdir_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to make directory {} in SD image: {}", entry.physicalName,
                    FatFsErrorToString(mkdir_error_code));
      return false;
    }

    const auto chdir_error_code = f_chdir(entry.virtualName.c_str());
    if (chdir_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to entry directory {} in SD image: {}", entry.physicalName,
                    FatFsErrorToString(chdir_error_code));
      return false;
    }
  }

  for (const File::FSTEntry& child : entry.children)
  {
    if (!Pack(cancelled, child, false, tmp_buffer))
      return false;
  }

  if (!is_root)
  {
    const auto chdir_up_error_code = f_chdir("..");
    if (chdir_up_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to leave directory {} in SD image: {}", entry.physicalName,
                    FatFsErrorToString(chdir_up_error_code));
      return false;
    }
  }

  return true;
}

static void SortFST(File::FSTEntry* root)
{
  std::sort(root->children.begin(), root->children.end(),
            [](const File::FSTEntry& lhs, const File::FSTEntry& rhs) {
              return lhs.virtualName < rhs.virtualName;
            });
  for (auto& child : root->children)
    SortFST(&child);
}

bool SyncSDFolderToSDImage(const std::function<bool()>& cancelled, bool deterministic)
{
  const std::string source_dir = File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX);
  const std::string image_path = File::GetUserPath(F_WIISDCARDIMAGE_IDX);
  if (source_dir.empty() || image_path.empty())
    return false;

  INFO_LOG_FMT(COMMON, "Starting SD card conversion from folder {} to file {}", source_dir,
               image_path);

  if (!File::IsDirectory(source_dir))
  {
    ERROR_LOG_FMT(COMMON, "{} is not a directory, not converting", source_dir);
    return false;
  }

  File::FSTEntry root = File::ScanDirectoryTree(source_dir, true);
  if (deterministic)
    SortFST(&root);
  if (!CheckIfFATCompatible(root))
    return false;

  u64 size = Config::Get(Config::MAIN_WII_SD_CARD_FILESIZE);
  if (size == 0)
  {
    size = GetSize(root);
    // Allocate a reasonable amount of free space
    size += std::clamp(size / 2, MebibytesToBytes(512), GibibytesToBytes(8));
  }
  size = AlignUp(size, MAX_CLUSTER_SIZE);

  std::lock_guard lk(s_fatfs_mutex);
  SDCardFatFsCallbacks callbacks;
  s_callbacks = &callbacks;
  Common::ScopeGuard callbacks_guard{[] { s_callbacks = nullptr; }};

  File::IOFile image;
  callbacks.m_image = &image;
  callbacks.m_deterministic = deterministic;

  const std::string temp_image_path = File::GetTempFilenameForAtomicWrite(image_path);
  if (!image.Open(temp_image_path, "w+b"))
  {
    ERROR_LOG_FMT(COMMON, "Failed to create or overwrite SD image at {}", image_path);
    return false;
  }

  // delete temp file in failure case
  Common::ScopeGuard image_delete_guard{[&] {
    image.Close();
    File::Delete(temp_image_path);
  }};

  if (!image.Resize(size))
  {
    ERROR_LOG_FMT(COMMON, "Failed to allocate {} bytes for SD image at {}", size, image_path);
    return false;
  }

  MKFS_PARM options = {};
  options.fmt = FM_FAT32 | FM_SFD;
  options.n_fat = 0;    // Number of FATs: automatic
  options.align = 1;    // Alignment of the data region (in sectors)
  options.n_root = 0;   // Number of root directory entries: automatic (and unused for FAT32)
  options.au_size = 0;  // Cluster size: automatic

  std::vector<u8> tmp_buffer(MAX_CLUSTER_SIZE);
  const auto mkfs_error_code =
      f_mkfs("", &options, tmp_buffer.data(), static_cast<UINT>(tmp_buffer.size()));
  if (mkfs_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to initialize SD image filesystem: {}",
                  FatFsErrorToString(mkfs_error_code));
    return false;
  }

  FATFS fs{};
  const auto mount_error_code = f_mount(&fs, "", 0);
  if (mount_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to mount SD image filesystem: {}",
                  FatFsErrorToString(mount_error_code));
    return false;
  }
  Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

  if (!Pack(cancelled, root, true, tmp_buffer))
  {
    ERROR_LOG_FMT(COMMON, "Failed to pack folder {} to SD image at {}", source_dir,
                  temp_image_path);
    return false;
  }

  unmount_guard.Exit();  // unmount before closing the image

  if (!image.Close())
  {
    ERROR_LOG_FMT(COMMON, "Failed to close SD image at {}", temp_image_path);
    return false;
  }

  if (!File::Rename(temp_image_path, image_path))
  {
    ERROR_LOG_FMT(COMMON, "Failed to rename SD image from {} to {}", temp_image_path, image_path);
    return false;
  }

  image_delete_guard.Dismiss();  // no need to delete the temp file anymore after the rename

  INFO_LOG_FMT(COMMON, "Successfully packed folder {} to SD image at {}", source_dir, image_path);
  return true;
}

static bool Unpack(const std::function<bool()>& cancelled, const std::string path,
                   bool is_directory, const char* name, std::vector<u8>& tmp_buffer)
{
  if (cancelled())
    return false;

  if (!is_directory)
  {
    FIL src{};
    const auto open_error_code = f_open(&src, name, FA_READ);
    if (open_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to open file {} in SD image: {}", path,
                    FatFsErrorToString(open_error_code));
      return false;
    }

    File::IOFile dst(path, "wb");
    if (!dst)
    {
      ERROR_LOG_FMT(COMMON, "Failed to open file {}", path);
      return false;
    }

    u32 size = f_size(&src);
    while (size > 0)
    {
      if (cancelled())
        return false;

      u32 chunk_size = std::min(size, static_cast<u32>(tmp_buffer.size()));
      u32 read_size;
      const auto read_error_code = f_read(&src, tmp_buffer.data(), chunk_size, &read_size);
      if (read_error_code != FR_OK)
      {
        ERROR_LOG_FMT(COMMON, "Failed to read from file {} in SD image: {}", path,
                      FatFsErrorToString(read_error_code));
        return false;
      }

      if (read_size != chunk_size)
      {
        ERROR_LOG_FMT(COMMON, "Failed to read bytes of file {} in SD image ({} != {})", path,
                      read_size, chunk_size);
        return false;
      }

      if (!dst.WriteBytes(tmp_buffer.data(), chunk_size))
      {
        ERROR_LOG_FMT(COMMON, "Failed to write to file {}", path);
        return false;
      }

      size -= chunk_size;
    }

    if (!dst.Close())
    {
      ERROR_LOG_FMT(COMMON, "Failed to close file {}", path);
      return false;
    }

    const auto close_error_code = f_close(&src);
    if (close_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to close file {} in SD image: {}", path,
                    FatFsErrorToString(close_error_code));
      return false;
    }

    return true;
  }

  if (!File::CreateDir(path))
  {
    ERROR_LOG_FMT(COMMON, "Failed to create directory {}", path);
    return false;
  }

  const auto chdir_error_code = f_chdir(name);
  if (chdir_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to enter directory {} in SD image: {}", path,
                  FatFsErrorToString(chdir_error_code));
    return false;
  }

  DIR directory{};
  const auto opendir_error_code = f_opendir(&directory, ".");
  if (opendir_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to open directory {} in SD image: {}", path,
                  FatFsErrorToString(opendir_error_code));
    return false;
  }

  FILINFO entry{};
  while (true)
  {
    const auto readdir_error_code = f_readdir(&directory, &entry);
    if (readdir_error_code != FR_OK)
    {
      ERROR_LOG_FMT(COMMON, "Failed to read directory {} in SD image: {}", path,
                    FatFsErrorToString(readdir_error_code));
      return false;
    }

    if (entry.fname[0] == '\0')
      break;

    if (entry.fname[0] == '?' && entry.fname[1] == '\0' && entry.altname[0] == '\0')
    {
      // FATFS indicates entries that have neither a short nor a long filename this way.
      // These are likely corrupted file entries so just skip them.
      continue;
    }

    const std::string_view childname = entry.fname;

    // Check for path traversal attacks.
    const bool is_path_traversal_attack =
        (childname.find("\\") != std::string_view::npos) ||
        (childname.find('/') != std::string_view::npos) ||
        std::all_of(childname.begin(), childname.end(), [](char c) { return c == '.'; });
    if (is_path_traversal_attack)
    {
      ERROR_LOG_FMT(
          COMMON,
          "Path traversal attack detected in directory {} in SD image, child filename is {}", path,
          childname);
      return false;
    }

    if (!Unpack(cancelled, fmt::format("{}/{}", path, childname), entry.fattrib & AM_DIR,
                entry.fname, tmp_buffer))
    {
      return false;
    }
  }

  const auto closedir_error_code = f_closedir(&directory);
  if (closedir_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to close directory {} in SD image: {}", path,
                  FatFsErrorToString(closedir_error_code));
    return false;
  }

  const auto chdir_up_error_code = f_chdir("..");
  if (chdir_up_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to leave directory {} in SD image: {}", path,
                  FatFsErrorToString(chdir_up_error_code));
    return false;
  }

  return true;
}

bool SyncSDImageToSDFolder(const std::function<bool()>& cancelled)
{
  const std::string image_path = File::GetUserPath(F_WIISDCARDIMAGE_IDX);
  const std::string target_dir = File::GetUserPath(D_WIISDCARDSYNCFOLDER_IDX);
  if (image_path.empty() || target_dir.empty())
    return false;

  std::lock_guard lk(s_fatfs_mutex);
  SDCardFatFsCallbacks callbacks;
  s_callbacks = &callbacks;
  Common::ScopeGuard callbacks_guard{[] { s_callbacks = nullptr; }};

  INFO_LOG_FMT(COMMON, "Starting SD card conversion from file {} to folder {}", image_path,
               target_dir);

  File::IOFile image;
  callbacks.m_image = &image;

  // this shouldn't matter since we're not modifying the SD image here, but initialize it to
  // something consistent just in case
  callbacks.m_deterministic = true;

  if (!image.Open(image_path, "r+b"))
  {
    ERROR_LOG_FMT(COMMON, "Failed to open SD image at {}", image_path);
    return false;
  }

  FATFS fs{};
  const auto mount_error_code = f_mount(&fs, "", 0);
  if (mount_error_code != FR_OK)
  {
    ERROR_LOG_FMT(COMMON, "Failed to mount SD image file system: {}",
                  FatFsErrorToString(mount_error_code));
    return false;
  }
  Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

  // Unpack() and GetTempFilenameForAtomicWrite() don't want the trailing separator.
  const std::string target_dir_without_slash = target_dir.substr(0, target_dir.length() - 1);

  // Most systems don't offer atomic directory renaming, so it's simpler to directly work on the
  // actual one and rollback if needed.
  const bool target_dir_exists = File::IsDirectory(target_dir);
  const std::string backup_target_dir_without_slash =
      File::GetTempFilenameForAtomicWrite(target_dir_without_slash);

  if (target_dir_exists)
  {
    if (!File::Rename(target_dir_without_slash, backup_target_dir_without_slash))
    {
      ERROR_LOG_FMT(COMMON, "Failed to move old SD folder to {}", backup_target_dir_without_slash);
      return false;
    }
  }

  std::vector<u8> tmp_buffer(MAX_CLUSTER_SIZE);
  if (!Unpack(cancelled, target_dir_without_slash, true, "", tmp_buffer))
  {
    ERROR_LOG_FMT(COMMON, "Failed to unpack SD image {} to {}", image_path, target_dir);
    File::DeleteDirRecursively(target_dir_without_slash);
    if (target_dir_exists)
      File::Rename(backup_target_dir_without_slash, target_dir_without_slash);
    return false;
  }

  unmount_guard.Exit();  // unmount before closing the image

  if (target_dir_exists)
    File::DeleteDirRecursively(backup_target_dir_without_slash);

  // even if this fails the conversion has already succeeded, so we still return true
  if (!image.Close())
    ERROR_LOG_FMT(COMMON, "Failed to close SD image {}", image_path);

  INFO_LOG_FMT(COMMON, "Successfully unpacked SD image {} to {}", image_path, target_dir);
  return true;
}

void RunInFatFsContext(FatFsCallbacks& callbacks, const std::function<void()>& function)
{
  std::lock_guard lk(s_fatfs_mutex);
  s_callbacks = &callbacks;
  Common::ScopeGuard callbacks_guard{[] { s_callbacks = nullptr; }};
  function();
}
}  // namespace Common
