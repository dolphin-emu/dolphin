// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
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


std::map<IVolume::ELanguage, std::string> CBannerLoaderGC::GetNames()
{
	std::map<IVolume::ELanguage, std::string> names;

	if (!IsValid())
	{
		return names;
	}

	u32 name_count = 0;
	IVolume::ELanguage language;
	bool is_japanese = m_country == IVolume::ECountry::COUNTRY_JAPAN;

	// find Banner type
	switch (m_BNRType)
	{
	case CBannerLoaderGC::BANNER_BNR1:
		name_count = 1;
		language = is_japanese ? IVolume::ELanguage::LANGUAGE_JAPANESE : IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	// English, German, French, Spanish, Italian, Dutch
	case CBannerLoaderGC::BANNER_BNR2:
		name_count = 6;
		language = IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	default:
		break;
	}

	auto const banner = reinterpret_cast<const DVDBanner*>(m_pBannerFile);

	for (u32 i = 0; i < name_count; ++i)
	{
		auto& comment = banner->comment[i];
		std::string name = GetDecodedString(comment.longTitle);

		if (name.empty())
			name = GetDecodedString(comment.shortTitle);

		if (!name.empty())
			names[(IVolume::ELanguage)(language + i)] = name;
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


std::map<IVolume::ELanguage, std::string> CBannerLoaderGC::GetDescriptions()
{
	std::map<IVolume::ELanguage, std::string> descriptions;

	if (!IsValid())
	{
		return descriptions;
	}

	u32 desc_count = 0;
	IVolume::ELanguage language;
	bool is_japanese = m_country == IVolume::ECountry::COUNTRY_JAPAN;

	// find Banner type
	switch (m_BNRType)
	{
	case CBannerLoaderGC::BANNER_BNR1:
		desc_count = 1;
		language = is_japanese ? IVolume::ELanguage::LANGUAGE_JAPANESE : IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	// English, German, French, Spanish, Italian, Dutch
	case CBannerLoaderGC::BANNER_BNR2:
		language = IVolume::ELanguage::LANGUAGE_ENGLISH;
		desc_count = 6;
		break;

	default:
		break;
	}

	auto banner = reinterpret_cast<const DVDBanner*>(m_pBannerFile);

	for (u32 i = 0; i < desc_count; ++i)
	{
		auto& data = banner->comment[i].comment;
		std::string description = GetDecodedString(data);

		if (!description.empty())
			descriptions[(IVolume::ELanguage)(language + i)] = description;
	}

	return descriptions;
}

CBannerLoaderGC::BANNER_TYPE CBannerLoaderGC::getBannerType()
{
	u32 bannerSignature = *(u32*)m_pBannerFile;
	switch (bannerSignature)
	{
	case 0x31524e42:	// "BNR1"
		return CBannerLoaderGC::BANNER_BNR1;
	case 0x32524e42:	// "BNR2"
		return CBannerLoaderGC::BANNER_BNR2;
	default:
		return CBannerLoaderGC::BANNER_UNKNOWN;
	}
}

} // namespace
