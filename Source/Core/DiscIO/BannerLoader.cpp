// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>

#include "DiscIO/BannerLoader.h"
#include "DiscIO/BannerLoaderGC.h"
#include "DiscIO/BannerLoaderWii.h"
#include "DiscIO/Filesystem.h"

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
