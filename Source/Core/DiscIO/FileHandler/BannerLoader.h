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
		: m_IsValid(false)
		, m_pBannerFile(nullptr)
	{}

	virtual ~IBannerLoader()
	{}

	virtual std::vector<u32> GetBanner(int* pWidth, int* pHeight) = 0;

	virtual std::vector<std::string> GetNames() = 0;
	virtual std::string GetCompany() = 0;
	virtual std::vector<std::string> GetDescriptions() = 0;

	bool IsValid()
	{
		return m_IsValid;
	}

protected:
	bool m_IsValid;
	u8* m_pBannerFile;
};

IBannerLoader* CreateBannerLoader(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume *pVolume);

}  // namespace DiscIO
