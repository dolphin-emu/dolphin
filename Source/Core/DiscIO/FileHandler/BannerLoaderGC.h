// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

#include "DiscIO/FileHandler/BannerLoader.h"
#include "DiscIO/Volume/Volume.h"
#include "DiscIO/Volume/VolumeGC.h"

namespace DiscIO
{

class IFileSystem;

class CBannerLoaderGC
	: public IBannerLoader
{
public:
	CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume* volume);
	virtual ~CBannerLoaderGC();

	virtual std::vector<u32> GetBanner(int* pWidth, int* pHeight) override;

	virtual std::vector<std::string> GetNames() override;
	virtual std::string GetCompany() override;
	virtual std::vector<std::string> GetDescriptions() override;

private:
	enum
	{
		DVD_BANNER_WIDTH  = 96,
		DVD_BANNER_HEIGHT = 32
	};

	enum BANNER_TYPE
	{
		BANNER_UNKNOWN,
		BANNER_BNR1,
		BANNER_BNR2,
	};

	// Banner Comment
	struct DVDBannerComment
	{
		char shortTitle[32]; // Short game title shown in IPL menu
		char shortMaker[32]; // Short developer, publisher names shown in IPL menu
		char longTitle[64]; // Long game title shown in IPL game start screen
		char longMaker[64]; // Long developer, publisher names shown in IPL game start screen
		char comment[128]; // Game description shown in IPL game start screen in two lines.
	};

	// "opening.bnr" file format for EU console
	struct DVDBanner
	{
		u32 id; // 'BNR2'
		u32 padding[7];
		u16 image[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 96x32 texture image
		DVDBannerComment comment[6]; // Comments in six languages (only 1 for BNR1 type)
	};

	static const u32 BNR1_SIZE = sizeof(DVDBanner) - sizeof(DVDBannerComment) * 5;
	static const u32 BNR2_SIZE = sizeof(DVDBanner);

	template <u32 N>
	std::string GetDecodedString(const char (&data)[N])
	{
		auto const string_decoder = CVolumeGC::GetStringDecoder(m_country);

		// strnlen to trim NULLs
		return string_decoder(std::string(data, strnlen(data, sizeof(data))));
	}

	BANNER_TYPE m_BNRType;
	BANNER_TYPE getBannerType();

	DiscIO::IVolume::ECountry const m_country;
};

} // namespace
