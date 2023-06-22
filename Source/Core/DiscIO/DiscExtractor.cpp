// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/DiscExtractor.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
u64 ReadFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
             u8* buffer, u64 max_buffer_size, u64 offset_in_file)
{
  if (!file_info || file_info->IsDirectory() || offset_in_file >= file_info->GetSize())
    return 0;

  const u64 read_length = std::min(max_buffer_size, file_info->GetSize() - offset_in_file);

  DEBUG_LOG_FMT(DISCIO, "Reading {:x} bytes at {:x} from file {}. Offset: {:x} Size: {:x}",
                read_length, offset_in_file, file_info->GetPath(), file_info->GetOffset(),
                file_info->GetSize());

  if (!volume.Read(file_info->GetOffset() + offset_in_file, read_length, buffer, partition))
    return 0;

  return read_length;
}

u64 ReadFile(const Volume& volume, const Partition& partition, std::string_view path, u8* buffer,
             u64 max_buffer_size, u64 offset_in_file)
{
  const FileSystem* file_system = volume.GetFileSystem(partition);
  if (!file_system)
    return 0;

  return ReadFile(volume, partition, file_system->FindFileInfo(path).get(), buffer, max_buffer_size,
                  offset_in_file);
}

bool ExportData(const Volume& volume, const Partition& partition, u64 offset, u64 size,
                const std::string& export_filename)
{
  File::IOFile f(export_filename, "wb");
  if (!f)
    return false;

  while (size)
  {
    // Limit read size to 128 MB
    const size_t read_size = static_cast<size_t>(std::min<u64>(size, 0x08000000));

    std::vector<u8> buffer(read_size);

    if (!volume.Read(offset, read_size, buffer.data(), partition))
      return false;

    if (!f.WriteBytes(buffer.data(), read_size))
      return false;

    size -= read_size;
    offset += read_size;
  }

  return true;
}

bool ExportFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
                const std::string& export_filename)
{
  if (!file_info || file_info->IsDirectory())
    return false;

  return ExportData(volume, partition, file_info->GetOffset(), file_info->GetSize(),
                    export_filename);
}

bool ExportFile(const Volume& volume, const Partition& partition, std::string_view path,
                const std::string& export_filename)
{
  const FileSystem* file_system = volume.GetFileSystem(partition);
  if (!file_system)
    return false;

  return ExportFile(volume, partition, file_system->FindFileInfo(path).get(), export_filename);
}

void ExportDirectory(const Volume& volume, const Partition& partition, const FileInfo& directory,
                     bool recursive, const std::string& filesystem_path,
                     const std::string& export_folder,
                     const std::function<bool(const std::string& path)>& update_progress)
{
  std::string export_root = export_folder + '/';
  if (directory.IsDirectory() && !directory.IsRoot())
    export_root += directory.GetName() + '/';

  File::CreateFullPath(export_root);

  for (const FileInfo& file_info : directory)
  {
    const std::string name = file_info.GetName() + (file_info.IsDirectory() ? "/" : "");
    const std::string path = filesystem_path + name;
    const std::string export_path = export_root + name;

    if (update_progress(path))
      return;

    DEBUG_LOG_FMT(DISCIO, "{}", export_path);

    if (!file_info.IsDirectory())
    {
      if (File::Exists(export_path))
        NOTICE_LOG_FMT(DISCIO, "{} already exists", export_path);
      else if (!ExportFile(volume, partition, &file_info, export_path))
        ERROR_LOG_FMT(DISCIO, "Could not export {}", export_path);
    }
    else if (recursive)
    {
      ExportDirectory(volume, partition, file_info, recursive, filesystem_path, export_root,
                      update_progress);
    }
  }
}

bool ExportWiiUnencryptedHeader(const Volume& volume, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, WII_NONPARTITION_DISCHEADER_ADDRESS,
                    WII_NONPARTITION_DISCHEADER_SIZE, export_filename);
}

bool ExportWiiRegionData(const Volume& volume, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, WII_REGION_DATA_ADDRESS, WII_REGION_DATA_SIZE,
                    export_filename);
}

bool ExportTicket(const Volume& volume, const Partition& partition,
                  const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + WII_PARTITION_TICKET_ADDRESS,
                    WII_PARTITION_TICKET_SIZE, export_filename);
}

bool ExportTMD(const Volume& volume, const Partition& partition, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u32> size =
      volume.ReadSwapped<u32>(partition.offset + WII_PARTITION_TMD_SIZE_ADDRESS, PARTITION_NONE);
  const std::optional<u64> offset = volume.ReadSwappedAndShifted(
      partition.offset + WII_PARTITION_TMD_OFFSET_ADDRESS, PARTITION_NONE);
  if (!size || !offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, *size, export_filename);
}

bool ExportCertificateChain(const Volume& volume, const Partition& partition,
                            const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u32> size = volume.ReadSwapped<u32>(
      partition.offset + WII_PARTITION_CERT_CHAIN_SIZE_ADDRESS, PARTITION_NONE);
  const std::optional<u64> offset = volume.ReadSwappedAndShifted(
      partition.offset + WII_PARTITION_CERT_CHAIN_OFFSET_ADDRESS, PARTITION_NONE);
  if (!size || !offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, *size, export_filename);
}

bool ExportH3Hashes(const Volume& volume, const Partition& partition,
                    const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u64> offset = volume.ReadSwappedAndShifted(
      partition.offset + WII_PARTITION_H3_OFFSET_ADDRESS, PARTITION_NONE);
  if (!offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, WII_PARTITION_H3_SIZE,
                    export_filename);
}

bool ExportHeader(const Volume& volume, const Partition& partition,
                  const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  return ExportData(volume, partition, DISCHEADER_ADDRESS, DISCHEADER_SIZE, export_filename);
}

bool ExportBI2Data(const Volume& volume, const Partition& partition,
                   const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  return ExportData(volume, partition, BI2_ADDRESS, BI2_SIZE, export_filename);
}

bool ExportApploader(const Volume& volume, const Partition& partition,
                     const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  const std::optional<u64> apploader_size = GetApploaderSize(volume, partition);
  if (!apploader_size)
    return false;

  return ExportData(volume, partition, APPLOADER_ADDRESS, *apploader_size, export_filename);
}

bool ExportDOL(const Volume& volume, const Partition& partition, const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  const std::optional<u64> dol_offset = GetBootDOLOffset(volume, partition);
  if (!dol_offset)
    return false;
  const std::optional<u32> dol_size = GetBootDOLSize(volume, partition, *dol_offset);
  if (!dol_size)
    return false;

  return ExportData(volume, partition, *dol_offset, *dol_size, export_filename);
}

bool ExportFST(const Volume& volume, const Partition& partition, const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  const std::optional<u64> fst_offset = GetFSTOffset(volume, partition);
  const std::optional<u64> fst_size = GetFSTSize(volume, partition);
  if (!fst_offset || !fst_size)
    return false;

  return ExportData(volume, partition, *fst_offset, *fst_size, export_filename);
}

bool ExportSystemData(const Volume& volume, const Partition& partition,
                      const std::string& export_folder)
{
  bool success = true;

  File::CreateFullPath(export_folder + "/sys/");
  success &= ExportHeader(volume, partition, export_folder + "/sys/boot.bin");
  success &= ExportBI2Data(volume, partition, export_folder + "/sys/bi2.bin");
  success &= ExportApploader(volume, partition, export_folder + "/sys/apploader.img");
  success &= ExportDOL(volume, partition, export_folder + "/sys/main.dol");
  success &= ExportFST(volume, partition, export_folder + "/sys/fst.bin");

  if (volume.GetVolumeType() == Platform::WiiDisc)
  {
    File::CreateFullPath(export_folder + "/disc/");
    success &= ExportWiiUnencryptedHeader(volume, export_folder + "/disc/header.bin");
    success &= ExportWiiRegionData(volume, export_folder + "/disc/region.bin");

    success &= ExportTicket(volume, partition, export_folder + "/ticket.bin");
    success &= ExportTMD(volume, partition, export_folder + "/tmd.bin");
    success &= ExportCertificateChain(volume, partition, export_folder + "/cert.bin");
    if (volume.HasWiiHashes())
      success &= ExportH3Hashes(volume, partition, export_folder + "/h3.bin");
  }

  return success;
}

}  // namespace DiscIO
