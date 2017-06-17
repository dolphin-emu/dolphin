// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/DiscExtractor.h"

#include <algorithm>
#include <cinttypes>
#include <optional>

#include "Common/CommonTypes.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
u64 ReadFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
             u8* buffer, u64 max_buffer_size, u64 offset_in_file)
{
  if (!file_info || file_info->IsDirectory())
    return 0;

  if (offset_in_file >= file_info->GetSize())
    return 0;

  u64 read_length = std::min(max_buffer_size, file_info->GetSize() - offset_in_file);

  DEBUG_LOG(DISCIO, "Reading %" PRIx64 " bytes at %" PRIx64 " from file %s. Offset: %" PRIx64
                    " Size: %" PRIx32,
            read_length, offset_in_file, file_info->GetPath().c_str(), file_info->GetOffset(),
            file_info->GetSize());

  volume.Read(file_info->GetOffset() + offset_in_file, read_length, buffer, partition);

  return read_length;
}

bool ExportFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
                const std::string& export_filename)
{
  if (!file_info || file_info->IsDirectory())
    return false;

  u64 remaining_size = file_info->GetSize();
  u64 file_offset = file_info->GetOffset();

  File::IOFile f(export_filename, "wb");
  if (!f)
    return false;

  bool result = true;

  while (remaining_size)
  {
    // Limit read size to 128 MB
    size_t read_size = (size_t)std::min(remaining_size, (u64)0x08000000);

    std::vector<u8> buffer(read_size);

    result = volume.Read(file_offset, read_size, &buffer[0], partition);

    if (!result)
      break;

    f.WriteBytes(&buffer[0], read_size);

    remaining_size -= read_size;
    file_offset += read_size;
  }

  return result;
}

bool ExportApploader(const Volume& volume, const Partition& partition,
                     const std::string& export_folder)
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

  std::vector<u8> buffer(*apploader_size);
  if (volume.Read(0x2440, *apploader_size, buffer.data(), partition))
  {
    std::string export_name(export_folder + "/apploader.img");

    File::IOFile apploader_file(export_name, "wb");
    if (apploader_file)
    {
      apploader_file.WriteBytes(buffer.data(), *apploader_size);
      return true;
    }
  }

  return false;
}

std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition)
{
  const DiscIO::Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return {};

  std::optional<u32> offset = volume.ReadSwapped<u32>(0x420, partition);
  const u8 offset_shift = volume_type == Platform::WII_DISC ? 2 : 0;
  return offset ? static_cast<u64>(*offset) << offset_shift : std::optional<u64>();
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

bool ExportDOL(const Volume& volume, const Partition& partition, const std::string& export_folder)
{
  if (!IsDisc(volume.GetVolumeType()))
    return false;

  std::optional<u64> dol_offset = GetBootDOLOffset(volume, partition);
  if (!dol_offset)
    return false;
  std::optional<u32> dol_size = GetBootDOLSize(volume, partition, *dol_offset);
  if (!dol_size)
    return false;

  std::vector<u8> buffer(*dol_size);
  if (volume.Read(*dol_offset, *dol_size, buffer.data(), partition))
  {
    std::string export_name(export_folder + "/boot.dol");

    File::IOFile dol_file(export_name, "wb");
    if (dol_file)
    {
      dol_file.WriteBytes(&buffer[0], *dol_size);
      return true;
    }
  }

  return false;
}

}  // namespace DiscIO
