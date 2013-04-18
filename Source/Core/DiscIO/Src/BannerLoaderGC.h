// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _BANNER_LOADER_GC_H_
#define _BANNER_LOADER_GC_H_

#include "BannerLoader.h"
#include "VolumeGC.h"
#include "StringUtil.h"

namespace DiscIO
{
class CBannerLoaderGC
	: public IBannerLoader
{
	public:
		CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem, DiscIO::IVolume* volume);
		virtual ~CBannerLoaderGC();

		virtual bool IsValid();

		virtual bool GetBanner(u32* _pBannerImage);

		virtual std::vector<std::string> GetNames();
		virtual std::string GetCompany();
		virtual std::vector<std::string> GetDescriptions();

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

		u8* m_pBannerFile;
		bool m_IsValid;
		BANNER_TYPE m_BNRType;

		void decode5A3image(u32* dst, u16* src, int width, int height);
		BANNER_TYPE getBannerType();
		
		DiscIO::IVolume::ECountry const m_country;
};

} // namespace

#endif

