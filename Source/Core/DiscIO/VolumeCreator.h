// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IVolume;
class IBlobReader;

IVolume* CreateVolumeFromFilename(const std::string& _rFilename, u32 _PartitionGroup = 0, u32 _VolumeNum = -1);
IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii, const std::string& _rApploader = "", const std::string& _rDOL = "");
bool IsVolumeWiiDisc(const IVolume *_rVolume);
bool IsVolumeWadFile(const IVolume *_rVolume);
void VolumeKeyForParition(IBlobReader& _rReader, u64 offset, u8* VolumeKey);

} // namespace
