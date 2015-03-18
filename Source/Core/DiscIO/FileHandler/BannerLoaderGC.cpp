// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "DiscIO/BannerLoaderGC.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
CBannerLoaderGC::CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume* volume)
	: m_country(volume->GetCountry())
{
	// load the opening.bnr
	size_t FileSize = (size_t) _rFileSystem.GetFileSize("opening.bnr");
	if (FileSize == BNR1_SIZE || FileSize == BNR2_SIZE)
	{
		m_pBannerFile = new u8[FileSize];
		if (m_pBannerFile)
		{
			_rFileSystem.ReadFile("opening.bnr", m_pBannerFile, FileSize);
			m_BNRType = getBannerType();
			if (m_BNRType == BANNER_UNKNOWN)
				PanicAlertT("Invalid opening.bnr found in gcm:\n%s\n You may need to redump this game.",
					_rFileSystem.GetVolume()->GetName().c_str());
			else m_IsValid = true;
		}
	}
	else WARN_LOG(DISCIO, "Invalid opening.bnr size: %0lx",
		(unsigned long)FileSize);
}


CBannerLoaderGC::~CBannerLoaderGC()
{
	if (m_pBannerFile)
	{
		delete [] m_pBannerFile;
		m_pBannerFile = nullptr;
	}
}

std::vector<u32> CBannerLoaderGC::GetBanner(int* pWidth, int* pHeight)
{
	std::vector<u32> Buffer;
	Buffer.resize(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT);
	auto const pBanner = (DVDBanner*)m_pBannerFile;
	ColorUtil::decode5A3image(&Buffer[0], pBanner->image, DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT);
	*pWidth = DVD_BANNER_WIDTH;
	*pHeight = DVD_BANNER_HEIGHT;
	return Buffer;
}


std::vector<std::string> CBannerLoaderGC::GetNames()
{
	std::vector<std::string> names;

	if (!IsValid())
	{
		return names;
	}

	u32 name_count = 0;

	// find Banner type
	switch (m_BNRType)
	{
	case CBannerLoaderGC::BANNER_BNR1:
		name_count = 1;
		break;

	case CBannerLoaderGC::BANNER_BNR2:
		name_count = 6;
		break;

	default:
		break;
	}

	auto const banner = reinterpret_cast<const DVDBanner*>(m_pBannerFile);

	for (u32 i = 0; i != name_count; ++i)
	{
		auto& comment = banner->comment[i];

		if (comment.longTitle[0])
		{
			auto& data = comment.longTitle;
			names.push_back(GetDecodedString(data));
		}
		else
		{
			auto& data = comment.shortTitle;
			names.push_back(GetDecodedString(data));
		}
	}

	return names;
}


std::string CBannerLoaderGC::GetCompany()
{
	std::string company;

	if (IsValid())
	{
		auto const pBanner = (DVDBanner*)m_pBannerFile;
		auto& data = pBanner->comment[0].shortMaker;
		company = GetDecodedString(data);
	}

	return company;
}


std::vector<std::string> CBannerLoaderGC::GetDescriptions()
{
	std::vector<std::string> descriptions;

	if (!IsValid())
	{
		return descriptions;
	}

	u32 desc_count = 0;

	// find Banner type
	switch (m_BNRType)
	{
	case CBannerLoaderGC::BANNER_BNR1:
		desc_count = 1;
		break;

	// English, German, French, Spanish, Italian, Dutch
	case CBannerLoaderGC::BANNER_BNR2:
		desc_count = 6;
		break;

	default:
		break;
	}

	auto banner = reinterpret_cast<const DVDBanner*>(m_pBannerFile);

	for (u32 i = 0; i != desc_count; ++i)
	{
		auto& data = banner->comment[i].comment;
		descriptions.push_back(GetDecodedString(data));
	}

	return descriptions;
}

CBannerLoaderGC::BANNER_TYPE CBannerLoaderGC::getBannerType()
{
	u32 bannerSignature = *(u32*)m_pBannerFile;
	CBannerLoaderGC::BANNER_TYPE type = CBannerLoaderGC::BANNER_UNKNOWN;
	switch (bannerSignature)
	{
	// "BNR1"
	case 0x31524e42:
		type = CBannerLoaderGC::BANNER_BNR1;
		break;

	// "BNR2"
	case 0x32524e42:
		type = CBannerLoaderGC::BANNER_BNR2;
		break;
	}
	return type;
}

} // namespace
