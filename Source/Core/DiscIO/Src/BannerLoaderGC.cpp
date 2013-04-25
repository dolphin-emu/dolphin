// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// HyperIris: need clean code
#include "../../Core/Src/ConfigManager.h"

#include "ColorUtil.h"
#include "BannerLoaderGC.h"

namespace DiscIO
{
CBannerLoaderGC::CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume* volume)
	: m_pBannerFile(NULL)
	, m_IsValid(false)
	, m_country(volume->GetCountry())
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
		m_pBannerFile = NULL;
	}
}


bool CBannerLoaderGC::IsValid()
{
	return m_IsValid;
}

bool CBannerLoaderGC::GetBanner(u32* _pBannerImage)
{
	if (!IsValid())
	{
		return false;
	}

	auto const pBanner = (DVDBanner*)m_pBannerFile;
	decode5A3image(_pBannerImage, pBanner->image, DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT);

	return true;
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


void CBannerLoaderGC::decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = ColorUtil::Decode5A3(Common::swap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
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
