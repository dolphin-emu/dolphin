// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeDirectory.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{
std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename)
{
  std::unique_ptr<IBlobReader> reader(CreateBlobReader(filename));
  if (reader == nullptr)
    return nullptr;
  CBlobBigEndianReader be_reader(*reader);

  // Check for Wii
  u32 wii_magic = 0;
  be_reader.ReadSwapped(0x18, &wii_magic);
  u32 wii_container_magic = 0;
  be_reader.ReadSwapped(0x60, &wii_container_magic);
  if (wii_magic == 0x5D1C9EA3 && wii_container_magic != 0)
    return std::make_unique<CVolumeGC>(std::move(reader));
  if (wii_magic == 0x5D1C9EA3 && wii_container_magic == 0)
    return std::make_unique<CVolumeWiiCrypted>(std::move(reader));

  // Check for WAD
  // 0x206962 for boot2 wads
  u32 wad_magic = 0;
  be_reader.ReadSwapped(0x02, &wad_magic);
  if (wad_magic == 0x00204973 || wad_magic == 0x00206962)
    return std::make_unique<CVolumeWAD>(std::move(reader));

  // Check for GC
  u32 gc_magic = 0;
  be_reader.ReadSwapped(0x1C, &gc_magic);
  if (gc_magic == 0xC2339F3D)
    return std::make_unique<CVolumeGC>(std::move(reader));

  // No known magic words found
  return nullptr;
}

std::unique_ptr<IVolume> CreateVolumeFromDirectory(const std::string& directory, bool is_wii,
                                                   const std::string& apploader,
                                                   const std::string& dol)
{
  if (CVolumeDirectory::IsValidDirectory(directory))
    return std::make_unique<CVolumeDirectory>(directory, is_wii, apploader, dol);

  return nullptr;
}

}  // namespace
