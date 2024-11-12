// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/VFF/VFFUtil.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <fmt/format.h>

// Does not compile if diskio.h is included first.
// clang-format off
#include "ff.h"
#include "diskio.h"
// clang-format on

#include "Common/Align.h"
#include "Common/EnumUtils.h"
#include "Common/FatFsUtil.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/Swap.h"

#include "Core/IOS/Uids.h"

static DRESULT read_vff_header(IOS::HLE::FS::FileHandle* vff, FATFS* fs)
{
  IOS::HLE::NWC24::VFFHeader header;
  if (!vff->Read(&header, 1))
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read VFF header.");
    return RES_ERROR;
  }

  u16 cluster_size = 0;
  u16 cluster_count = 0;

  switch (Common::swap16(header.endianness))
  {
  case IOS::HLE::NWC24::VF_BIG_ENDIAN:
    cluster_size = Common::swap16(header.cluster_size) * 16;
    cluster_count = Common::swap32(header.volume_size) / cluster_size;
    break;
  case IOS::HLE::NWC24::VF_LITTLE_ENDIAN:
    // TODO: Actually implement.
    // Another option is to just delete these VFFs and let the current channel create a Big Endian
    // one.
    return RES_ERROR;
  }

  if (cluster_count < 4085)
  {
    fs->fs_type = FS_FAT12;

    u32 table_size = ((cluster_count + 1) / 2) * 3;
    table_size = Common::AlignUp(table_size, cluster_size);

    // Fsize is the full table size divided by 512 (Cluster size).
    fs->fsize = table_size / 512;
  }
  else if (cluster_count < 65525)
  {
    fs->fs_type = FS_FAT16;

    u32 table_size = cluster_count * 2;
    table_size = Common::AlignUp(table_size, cluster_size);
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

  // invalidate window
  fs->wflag = 0;
  fs->winsect = std::numeric_limits<LBA_t>::max();

  return RES_OK;
}

static FRESULT vff_mount(IOS::HLE::FS::FileHandle* vff, FATFS* fs)
{
  fs->fs_type = 0;  // Clear the filesystem object
  fs->pdrv = 0;     // Volume hosting physical drive

  DRESULT ret = read_vff_header(vff, fs);
  if (ret != RES_OK)
    return FR_DISK_ERR;

  return FR_OK;
}

static DRESULT vff_read(IOS::HLE::FS::FileHandle* vff, BYTE pdrv, BYTE* buff, LBA_t sector,
                        UINT count)
{
  // We cannot read or write data to the 0th sector in a VFF.
  if (sector == 0)
  {
    ERROR_LOG_FMT(IOS_WC24, "Attempted to read the 0th sector in the VFF: Invalid VFF?");
    return RES_ERROR;
  }

  const u64 offset = static_cast<u64>(sector) * IOS::HLE::NWC24::SECTOR_SIZE - 480;
  if (!vff->Seek(offset, IOS::HLE::FS::SeekMode::Set))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * IOS::HLE::NWC24::SECTOR_SIZE;
  if (!vff->Read(buff, size))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF read failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

static DRESULT vff_write(IOS::HLE::FS::FileHandle* vff, BYTE pdrv, const BYTE* buff, LBA_t sector,
                         UINT count)
{
  if (sector == 0)
  {
    ERROR_LOG_FMT(IOS_WC24, "Attempted to write to the 0th sector in the VFF: Invalid VFF?");
    return RES_ERROR;
  }

  const u64 offset = static_cast<u64>(sector) * IOS::HLE::NWC24::SECTOR_SIZE - 480;
  if (!vff->Seek(offset, IOS::HLE::FS::SeekMode::Set))
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF seek failed (offset={})", offset);
    return RES_ERROR;
  }

  const size_t size = static_cast<size_t>(count) * IOS::HLE::NWC24::SECTOR_SIZE;
  const auto res = vff->Write(buff, size);
  if (!res)
  {
    ERROR_LOG_FMT(IOS_WC24, "VFF write failed (offset={}, size={})", offset, size);
    return RES_ERROR;
  }

  return RES_OK;
}

static DRESULT vff_ioctl(IOS::HLE::FS::FileHandle* vff, BYTE pdrv, BYTE cmd, void* buff)
{
  switch (cmd)
  {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    *reinterpret_cast<LBA_t*>(buff) = vff->GetStatus()->size / IOS::HLE::NWC24::SECTOR_SIZE;
    return RES_OK;
  default:
    WARN_LOG_FMT(IOS_WC24, "Unexpected FAT ioctl {}", cmd);
    return RES_OK;
  }
}

namespace IOS::HLE::NWC24
{
static ErrorCode WriteFile(const std::string& filename, std::span<const u8> tmp_buffer)
{
  FIL dst{};
  const auto open_error_code = f_open(&dst, filename.c_str(), FA_CREATE_ALWAYS | FA_WRITE);
  if (open_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to open file {} in VFF", filename);
    return WC24_ERR_FILE_OPEN;
  }

  size_t size = tmp_buffer.size();
  size_t offset = 0;
  while (size > 0)
  {
    constexpr size_t MAX_CHUNK_SIZE = 32768;
    u32 chunk_size = static_cast<u32>(std::min(size, MAX_CHUNK_SIZE));

    u32 written_size;
    const auto write_error_code =
        f_write(&dst, tmp_buffer.data() + offset, chunk_size, &written_size);
    if (write_error_code != FR_OK)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to write file {} to VFF: {}", filename,
                    Common::ToUnderlying(write_error_code));
      return WC24_ERR_FILE_WRITE;
    }

    if (written_size != chunk_size)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to write bytes of file {} to VFF ({} != {})", filename,
                    written_size, chunk_size);
      return WC24_ERR_FILE_WRITE;
    }

    size -= chunk_size;
    offset += chunk_size;
  }

  const auto close_error_code = f_close(&dst);
  if (close_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to close file {} in VFF", filename);
    return WC24_ERR_FILE_CLOSE;
  }

  return WC24_OK;
}

static ErrorCode ReadFile(const std::string& filename, std::vector<u8>& out)
{
  FIL src{};
  const auto open_error_code = f_open(&src, filename.c_str(), FA_READ);
  if (open_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to open file {} in VFF", filename);
    return WC24_ERR_FILE_OPEN;
  }

  Common::ScopeGuard vff_close_guard{[&] { f_close(&src); }};

  u32 size = static_cast<u32>(out.size());
  u32 read_size{};
  const auto read_error_code = f_read(&src, out.data(), size, &read_size);
  if (read_error_code != FR_OK)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read file {} in VFF: {}", filename,
                  static_cast<u32>(read_error_code));
    return WC24_ERR_FILE_READ;
  }

  if (read_size != size)
  {
    ERROR_LOG_FMT(IOS_WC24, "Failed to read bytes of file {} to VFF ({} != {})", filename,
                  read_size, size);
    return WC24_ERR_FILE_READ;
  }

  // As prior operations did not fail, dismiss the guard and handle a potential error with f_close.
  vff_close_guard.Dismiss();

  const auto close_error_code = f_close(&src);
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
    return vff_read(m_vff, pdrv, buff, sector, count);
  }

  int DiskWrite(u8 pdrv, const u8* buff, u32 sector, unsigned int count) override
  {
    return vff_write(m_vff, pdrv, buff, sector, count);
  }

  int DiskIOCtl(u8 pdrv, u8 cmd, void* buff) override { return vff_ioctl(m_vff, pdrv, cmd, buff); }

  IOS::HLE::FS::FileHandle* m_vff = nullptr;
};
}  // namespace

ErrorCode WriteToVFF(const std::string& path, const std::string& filename,
                     const std::shared_ptr<FS::FileSystem>& fs, std::span<const u8> data)
{
  VffFatFsCallbacks callbacks;
  ErrorCode return_value;
  Common::RunInFatFsContext(callbacks, [&]() {
    auto temp = fs->OpenFile(PID_KD, PID_KD, path, FS::Mode::ReadWrite);
    if (!temp)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to open VFF at: {}", path);
      return_value = WC24_ERR_FILE_OPEN;
      return;
    }

    callbacks.m_vff = &*temp;

    Common::ScopeGuard vff_delete_guard{[&] { fs->Delete(PID_KD, PID_KD, path); }};

    FATFS fatfs{};
    const FRESULT fatfs_mount_error_code = f_mount(&fatfs, "", 0);
    if (fatfs_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_FILE_READ;
      return;
    }

    Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

    const FRESULT vff_mount_error_code = vff_mount(callbacks.m_vff, &fatfs);
    if (vff_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_FILE_READ;
      return;
    }

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

ErrorCode ReadFromVFF(const std::string& path, const std::string& filename,
                      const std::shared_ptr<FS::FileSystem>& fs, std::vector<u8>& out)
{
  VffFatFsCallbacks callbacks;
  ErrorCode return_value;
  Common::RunInFatFsContext(callbacks, [&]() {
    auto temp = fs->OpenFile(PID_KD, PID_KD, path, FS::Mode::ReadWrite);
    if (!temp)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to open VFF at: {}", path);
      return_value = WC24_ERR_NOT_FOUND;
      return;
    }

    callbacks.m_vff = &*temp;

    FATFS fatfs{};
    const FRESULT fatfs_mount_error_code = f_mount(&fatfs, "", 0);
    if (fatfs_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

    const FRESULT vff_mount_error_code = vff_mount(callbacks.m_vff, &fatfs);
    if (vff_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    const ErrorCode read_error_code = ReadFile(filename, out);
    if (read_error_code != WC24_OK)
    {
      return_value = read_error_code;
      return;
    }

    return_value = WC24_OK;
    return;
  });

  return return_value;
}

ErrorCode DeleteFileFromVFF(const std::string& path, const std::string& filename,
                            const std::shared_ptr<FS::FileSystem>& fs)
{
  VffFatFsCallbacks callbacks;
  ErrorCode return_value;
  Common::RunInFatFsContext(callbacks, [&]() {
    auto temp = fs->OpenFile(PID_KD, PID_KD, path, FS::Mode::ReadWrite);
    if (!temp)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to open VFF at: {}", path);
      return_value = WC24_ERR_NOT_FOUND;
      return;
    }

    callbacks.m_vff = &*temp;

    FATFS fatfs{};
    const FRESULT fatfs_mount_error_code = f_mount(&fatfs, "", 0);
    if (fatfs_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    Common::ScopeGuard unmount_guard{[] { f_unmount(""); }};

    const FRESULT vff_mount_error_code = vff_mount(callbacks.m_vff, &fatfs);
    if (vff_mount_error_code != FR_OK)
    {
      // The VFF is most likely broken.
      ERROR_LOG_FMT(IOS_WC24, "Failed to mount VFF at: {}", path);
      return_value = WC24_ERR_BROKEN;
      return;
    }

    const FRESULT unlink_code = f_unlink(filename.c_str());
    if (unlink_code != FR_OK)
    {
      ERROR_LOG_FMT(IOS_WC24, "Failed to delete file {} in VFF at: {} Code: {}", filename, path,
                    static_cast<u32>(unlink_code));
      return_value = WC24_ERR_BROKEN;
      return;
    }

    return_value = WC24_OK;
    return;
  });

  return return_value;
}
}  // namespace IOS::HLE::NWC24
