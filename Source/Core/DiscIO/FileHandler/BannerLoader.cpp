// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>

#include "DiscIO/FileHandler/BannerLoader.h"
#include "DiscIO/FileHandler/BannerLoaderGC.h"
#include "DiscIO/FileHandler/BannerLoaderWii.h"
#include "DiscIO/FileSystem/Filesystem.h"

namespace DiscIO
{

class IBannerLoader;
class IVolume;

IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume *pVolume)
{
	if (pVolume->IsWiiDisc() || pVolume->IsWadFile())
		return new CBannerLoaderWii(pVolume);
	if (_rFileSystem.IsValid())
		return new CBannerLoaderGC(_rFileSystem, pVolume);

	return nullptr;
}

} // namespace
