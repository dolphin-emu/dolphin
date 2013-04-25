// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _BANNER_LOADER_H_
#define _BANNER_LOADER_H_

#include <vector>
#include <string>

#include "Filesystem.h"

namespace DiscIO
{
class IBannerLoader
{
	public:

		IBannerLoader()
		{}


		virtual ~IBannerLoader()
		{}


		virtual bool IsValid() = 0;

		virtual bool GetBanner(u32* _pBannerImage) = 0;

		virtual std::vector<std::string> GetNames() = 0;
		virtual std::string GetCompany() = 0;
		virtual std::vector<std::string> GetDescriptions() = 0;
};

IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume *pVolume);
} // namespace

#endif

