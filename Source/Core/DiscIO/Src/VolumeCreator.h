// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VOLUME_CREATOR_H
#define _VOLUME_CREATOR_H

#include "Volume.h"

namespace DiscIO
{
IVolume* CreateVolumeFromFilename(const std::string& _rFilename, u32 _PartitionGroup = 0, u32 _VolumeNum = -1);
IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii, const std::string& _rApploader = "", const std::string& _rDOL = "");
bool IsVolumeWiiDisc(const IVolume *_rVolume);
bool IsVolumeWadFile(const IVolume *_rVolume);
} // namespace

#endif

