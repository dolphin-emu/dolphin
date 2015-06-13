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
enum EDiscType
{
  DISC_TYPE_UNK,
  DISC_TYPE_WII,
  DISC_TYPE_WII_CONTAINER,
  DISC_TYPE_GC,
  DISC_TYPE_WAD
};

EDiscType GetDiscType(IBlobReader& _rReader);

std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename)
{
  std::unique_ptr<IBlobReader> reader(CreateBlobReader(filename));
  if (reader == nullptr)
    return nullptr;

  switch (GetDiscType(*reader))
  {
  case DISC_TYPE_WII:
  case DISC_TYPE_GC:
    return std::make_unique<CVolumeGC>(std::move(reader));

  case DISC_TYPE_WAD:
    return std::make_unique<CVolumeWAD>(std::move(reader));

  case DISC_TYPE_WII_CONTAINER:
    return std::make_unique<CVolumeWiiCrypted>(std::move(reader));

  case DISC_TYPE_UNK:
    return nullptr;
  }

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

EDiscType GetDiscType(IBlobReader& _rReader)
{
  CBlobBigEndianReader Reader(_rReader);

  // Check for Wii
  u32 WiiMagic = 0;
  Reader.ReadSwapped(0x18, &WiiMagic);
  u32 WiiContainerMagic = 0;
  Reader.ReadSwapped(0x60, &WiiContainerMagic);
  if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic != 0)
    return DISC_TYPE_WII;
  if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic == 0)
    return DISC_TYPE_WII_CONTAINER;

  // Check for WAD
  // 0x206962 for boot2 wads
  u32 WADMagic = 0;
  Reader.ReadSwapped(0x02, &WADMagic);
  if (WADMagic == 0x00204973 || WADMagic == 0x00206962)
    return DISC_TYPE_WAD;

  // Check for GC
  u32 GCMagic = 0;
  Reader.ReadSwapped(0x1C, &GCMagic);
  if (GCMagic == 0xC2339F3D)
    return DISC_TYPE_GC;

  // No known magic words found
  return DISC_TYPE_UNK;
}

}  // namespace
