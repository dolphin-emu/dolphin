// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{

class IFileSystem;
class IVolume;

class IBannerLoader
{
	public:

		IBannerLoader()
		{}


		virtual ~IBannerLoader()
		{}


		virtual bool IsValid() = 0;

		virtual std::vector<u32> GetBanner(int* pWidth, int* pHeight) = 0;

		virtual std::vector<std::string> GetNames() = 0;
		virtual std::string GetCompany() = 0;
		virtual std::vector<std::string> GetDescriptions() = 0;
};

IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume *pVolume);

}  // namespace DiscIO
