// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IVolume;

IVolume* CreateVolumeFromFilename(const std::string& rFilename, u32 PartitionGroup = 0, u32 VolumeNum = -1);
IVolume* CreateVolumeFromDirectory(const std::string& rDirectory, bool bIsWii, const std::string& rApploader = "", const std::string& rDOL = "");
bool IsVolumeWiiDisc(const IVolume *rVolume);
bool IsVolumeWadFile(const IVolume *rVolume);

} // namespace
