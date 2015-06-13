// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

namespace DiscIO
{
class IVolume;

std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename);
std::unique_ptr<IVolume> CreateVolumeFromDirectory(const std::string& directory, bool is_wii,
                                                   const std::string& apploader = "",
                                                   const std::string& dol = "");

}  // namespace
