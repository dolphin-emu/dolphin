// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/VFF/VFFUtil.h"

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
#include "Common/FatFsUtil.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

#include "Core/IOS/Uids.h"

static IOS::HLE::FS::FileHandle* s_vff;

static DRESULT vff_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count);
static DRESULT vff_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count);
static DRESULT vff_ioctl(BYTE pdrv, BYTE cmd, void* buff);
static DRESULT read_vff_header(FATFS* fs);

enum : u16
{
  SECTOR_SIZE = 512,
  VF_LITTLE_ENDIAN = 0xFFFE,
  VF_BIG_ENDIAN = 0xFEFF
};

static FRESULT vff_mount(FATFS* fs)
{
  fs->fs_type = 0;     // Clear the filesystem object
  fs->pdrv = (BYTE)0;  // Volume hosting physical drive

  DRESULT ret = read_vff_header(fs);
  if (ret != RES_OK)
    return FR_DISK_ERR;

  return FR_OK;
}

DRESULT read_vff_header(FATFS* fs)
{
  struct Header header;
  if (!s_vff->Read(&header, 1))
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read VFF header.");
    return RES_ERROR;
  }

  u16 cluster_size = 0;
  u16 cluster_count = 0;

  switch (Common::swap16(header.endianness))
  {
  case VF_BIG_ENDIAN:
    cluster_size = Common::swap16(header.clusterSize) * 16;
    cluster_count = Common::swap32(header.volumeSize) / cluster_size;
    break;
  case VF_LITTLE_ENDIAN:
    // TODO: Actually implement.
    // RiiConnect24's VFF are the only seen instance of Little Endian as it uses an SDK tool.
    // Another option is to just delete these VFF's and let the current channel create a Big Endian
    // one.
    return RES_ERROR;
  }

  if (cluster_count < 4085)
  {
    fs->fs_type = FS_FAT12;

    u32 table_size = std::floor((cluster_count + 1) / 2) * 3;
    table_size = (table_size + cluster_size - 1) & ~(cluster_size - 1);

    // Fsize is the full table size divided by 512 (Cluster size).
    fs->fsize = table_size / 512;
  }
  else if (cluster_count < 65525)
  {
    fs->fs_type = FS_FAT16;

    u32 table_size = cluster_count * 2;
    table_size = (table_size + cluster_size - 1) & ~(cluster_size - 1);
    fs->fsize = table_size / 512;
  }
  else
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF not FAT12 or 16! Cluster size: {}", cluster_size);
    return RES_ERROR;
  }

  fs->n_fats = 2;
  fs->csize = 1;

  // Root directory entry is 4096 bytes long, with each entry being 32 bytes. 4096 / 32 = 128
  fs->n_rootdir = 128;

  u32 sysect = 1 + (fs->fsize * 2) + fs->n_rootdir / (512 / 32);

  // cluster_count is the total count whereas this is the actual amount of clusters we can use
  u32 actual_cluster_count = cluster_count - sysect;

  fs->n_fatent = actual_cluster_count + 2;
  fs->volbase = 0;
  fs->fatbase = 1;
  fs->database = sysect;
  // Root directory entry
  fs->dirbase = fs->fatbase + fs->fsize * 2;

  // Initialize cluster allocation information
  fs->last_clst = fs->free_clst = 0xFFFFFFFF;
  fs->fsi_flag = 0x80;

  fs->id = 0;
  fs->cdir = 0;

  return RES_OK;
}

DRESULT vff_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE - 480;
  if (!s_vff->Seek(offset, IOS::HLE::FS::SeekMode::Set))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  if (!s_vff->Read(buff, size))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF read failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

DRESULT vff_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
  const u64 offset = static_cast<u64>(sector) * SECTOR_SIZE - 480;
  if (!s_vff->Seek(offset, IOS::HLE::FS::SeekMode::Set))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * SECTOR_SIZE;
  const auto res = s_vff->Write(buff, size);
  if (!res)
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF write failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

DRESULT vff_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
  switch (cmd)
  {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    *reinterpret_cast<LBA_t*>(buff) = s_vff->GetStatus()->size / SECTOR_SIZE;
    return RES_OK;
  default:
    WARN_LOG_FMT(IOS_WC24, "Unexpected FAT ioctl {}", cmd);
    return RES_OK;
  }
}

namespace IOS::HLE::NWC24
{
static ErrorCode WriteFile(const std::string& filename, const std::vector<u8>& tmp_buffer)
{
  FIL dst;
  const auto open_error_code = f_open(&dst, filename.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
  if (open_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to open file {} in VFF", filename);
    return WC24_ERR_FILE_OPEN;
  }

  // FIXME: Handle written_size < tmp_buffer.size()
  u32 written_size;
  const auto write_error_code =
      f_write(&dst, tmp_buffer.data(), static_cast<UINT>(tmp_buffer.size()), &written_size);
  if (write_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to write file {} to VFF {}", filename, write_error_code);
    return WC24_ERR_FILE_WRITE;
  }

  const auto close_error_code = f_close(&dst);
  if (close_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to close file {} in VFF", filename);
    return WC24_ERR_FILE_CLOSE;
  }

  return WC24_OK;
}

namespace
{
class VffFatFsCallbacks : public Common::FatFsCallbacks
{
public:
  int DiskRead(u8 pdrv, u8* buff, u32 sector, unsigned int count) override
  {
    return vff_read(pdrv, buff, sector, count);
  }

  int DiskWrite(u8 pdrv, const u8* buff, u32 sector, unsigned int count) override
  {
    return vff_write(pdrv, buff, sector, count);
  }

  int DiskIOCtl(u8 pdrv, u8 cmd, void* buff) override { return vff_ioctl(pdrv, cmd, buff); }
};
}  // namespace

ErrorCode OpenVFF(const std::string& path, const std::string& filename,
                  const std::shared_ptr<FS::FileSystem>& fs, const std::vector<u8>& data)
{
  VffFatFsCallbacks callbacks;
  ErrorCode return_value;
  Common::RunInFatFsContext(callbacks, [&]() {
    auto file = fs->OpenFile(PID_KD, PID_KD, path, FS::Mode::ReadWrite);
    if (!file)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to open VFF at: {}", path);
      return_value = WC24_ERR_NOT_FOUND;
      return;
    }

    s_vff = file.operator->();

    Common::ScopeGuard vff_guard{[] { s_vff = nullptr; }};
    Common::ScopeGuard vff_delete_guard{[&] { fs->Delete(PID_KD, PID_KD, path); }};

    FATFS fatfs;
    const FRESULT fatfs_mount_error_code = f_mount(&fatfs, "", 0);
    if (fatfs_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    const FRESULT vff_mount_error_code = vff_mount(&fatfs);
    if (vff_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

    const auto write_error_code = WriteFile(filename, data);
    if (write_error_code != WC24_OK)
    {
      return_value = write_error_code;
      return;
    }

    vff_delete_guard.Dismiss();

    return_value = WC24_OK;
    return;
  });

  return return_value;
}
}  // namespace IOS::HLE::NWC24
