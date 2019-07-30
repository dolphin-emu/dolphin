// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/DiscExtractor.h"

#include <algorithm>
#include <cinttypes>
#include <locale>
#include <optional>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
std::string NameForPartitionType(u32 partition_type, bool include_prefix)
{
  switch (partition_type)
  {
  case PARTITION_DATA:
    return "DATA";
  case PARTITION_UPDATE:
    return "UPDATE";
  case PARTITION_CHANNEL:
    return "CHANNEL";
  case PARTITION_INSTALL:
    // wit doesn't recognize the name "INSTALL", so we can't use it when naming partition folders
    if (!include_prefix)
      return "INSTALL";
    [[fallthrough]];
  default:
    const std::string type_as_game_id{static_cast<char>((partition_type >> 24) & 0xFF),
                                      static_cast<char>((partition_type >> 16) & 0xFF),
                                      static_cast<char>((partition_type >> 8) & 0xFF),
                                      static_cast<char>(partition_type & 0xFF)};
    if (std::all_of(type_as_game_id.cbegin(), type_as_game_id.cend(),
                    [](char c) { return std::isalnum(c, std::locale::classic()); }))
    {
      return include_prefix ? "P-" + type_as_game_id : type_as_game_id;
    }

    return StringFromFormat(include_prefix ? "P%u" : "%u", partition_type);
  }
}

u64 ReadFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
             u8* buffer, u64 max_buffer_size, u64 offset_in_file)
{
  if (!file_info || file_info->IsDirectory() || offset_in_file >= file_info->GetSize())
    return 0;

  const u64 read_length = std::min(max_buffer_size, file_info->GetSize() - offset_in_file);

  DEBUG_LOG(DISCIO,
            "Reading %" PRIx64 " bytes at %" PRIx64 " from file %s. Offset: %" PRIx64
            " Size: %" PRIx32,
            read_length, offset_in_file, file_info->GetPath().c_str(), file_info->GetOffset(),
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
  File::CreateFullPath(export_folder + '/');

  for (const FileInfo& file_info : directory)
  {
    const std::string name = file_info.GetName() + (file_info.IsDirectory() ? "/" : "");
    const std::string path = filesystem_path + name;
    const std::string export_path = export_folder + '/' + name;

    if (update_progress(path))
      return;

    DEBUG_LOG(DISCIO, "%s", export_path.c_str());

    if (!file_info.IsDirectory())
    {
      if (File::Exists(export_path))
        NOTICE_LOG(DISCIO, "%s already exists", export_path.c_str());
      else if (!ExportFile(volume, partition, &file_info, export_path))
        ERROR_LOG(DISCIO, "Could not export %s", export_path.c_str());
    }
    else if (recursive)
    {
      ExportDirectory(volume, partition, file_info, recursive, path, export_path, update_progress);
    }
  }
}

bool ExportWiiUnencryptedHeader(const Volume& volume, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, 0, 0x100, export_filename);
}

bool ExportWiiRegionData(const Volume& volume, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, 0x4E000, 0x20, export_filename);
}

bool ExportTicket(const Volume& volume, const Partition& partition,
                  const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset, 0x2a4, export_filename);
}

bool ExportTMD(const Volume& volume, const Partition& partition, const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u32> size = volume.ReadSwapped<u32>(partition.offset + 0x2a4, PARTITION_NONE);
  const std::optional<u64> offset =
      volume.ReadSwappedAndShifted(partition.offset + 0x2a8, PARTITION_NONE);
  if (!size || !offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, *size, export_filename);
}

bool ExportCertificateChain(const Volume& volume, const Partition& partition,
                            const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u32> size = volume.ReadSwapped<u32>(partition.offset + 0x2ac, PARTITION_NONE);
  const std::optional<u64> offset =
      volume.ReadSwappedAndShifted(partition.offset + 0x2b0, PARTITION_NONE);
  if (!size || !offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, *size, export_filename);
}

bool ExportH3Hashes(const Volume& volume, const Partition& partition,
                    const std::string& export_filename)
{
  if (volume.GetVolumeType() != Platform::WiiDisc)
    return false;

  const std::optional<u64> offset =
      volume.ReadSwappedAndShifted(partition.offset + 0x2b4, PARTITION_NONE);
  if (!offset)
    return false;

  return ExportData(volume, PARTITION_NONE, partition.offset + *offset, 0x18000, export_filename);
}

bool ExportHeader(const Volume& volume, const Partition& partition,
                  const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  return ExportData(volume, partition, 0, 0x440, export_filename);
}

bool ExportBI2Data(const Volume& volume, const Partition& partition,
                   const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  return ExportData(volume, partition, 0x440, 0x2000, export_filename);
}

bool ExportApploader(const Volume& volume, const Partition& partition,
                     const std::string& export_filename)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  std::optional<u32> apploader_size = volume.ReadSwapped<u32>(0x2440 + 0x14, partition);
  const std::optional<u32> trailer_size = volume.ReadSwapped<u32>(0x2440 + 0x18, partition);
  constexpr u32 header_size = 0x20;
  if (!apploader_size || !trailer_size)
    return false;
  *apploader_size += *trailer_size + header_size;
  DEBUG_LOG(DISCIO, "Apploader size -> %x", *apploader_size);

  return ExportData(volume, partition, 0x2440, *apploader_size, export_filename);
}

std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return {};

  std::optional<u64> dol_offset = volume.ReadSwappedAndShifted(0x420, partition);

  // Datel AR disc has 0x00000000 as the offset (invalid) and doesn't use it in the AppLoader.
  if (dol_offset && *dol_offset == 0)
    dol_offset.reset();

  return dol_offset;
}

std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset)
{
  if (!IsDisc(volume.GetVolumeType()))
    return {};

  u32 dol_size = 0;

  // Iterate through the 7 code segments
  for (u8 i = 0; i < 7; i++)
  {
    const std::optional<u32> offset = volume.ReadSwapped<u32>(dol_offset + 0x00 + i * 4, partition);
    const std::optional<u32> size = volume.ReadSwapped<u32>(dol_offset + 0x90 + i * 4, partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  // Iterate through the 11 data segments
  for (u8 i = 0; i < 11; i++)
  {
    const std::optional<u32> offset = volume.ReadSwapped<u32>(dol_offset + 0x1c + i * 4, partition);
    const std::optional<u32> size = volume.ReadSwapped<u32>(dol_offset + 0xac + i * 4, partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  return dol_size;
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

std::optional<u64> GetFSTOffset(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return {};

  return volume.ReadSwappedAndShifted(0x424, partition);
}

std::optional<u64> GetFSTSize(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return {};

  return volume.ReadSwappedAndShifted(0x428, partition);
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
    if (volume.IsEncryptedAndHashed())
      success &= ExportH3Hashes(volume, partition, export_folder + "/h3.bin");
  }

  return success;
}

}  // namespace DiscIO
