// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IVolume;
class IBlobReader;

std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename, u32 partition_group = 0, u32 volume_number = -1);
std::unique_ptr<IVolume> CreateVolumeFromDirectory(const std::string& directory, bool is_wii, const std::string& apploader = "", const std::string& dol = "");
void VolumeKeyForPartition(IBlobReader& _rReader, u64 offset, u8* VolumeKey);

} // namespace
