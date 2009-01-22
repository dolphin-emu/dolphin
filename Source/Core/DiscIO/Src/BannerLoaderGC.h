// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _BANNER_LOADER_GC_H_
#define _BANNER_LOADER_GC_H_

#include "BannerLoader.h"

namespace DiscIO
{
class CBannerLoaderGC
	: public IBannerLoader
{
	public:

		CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem);

		virtual ~CBannerLoaderGC();

		virtual bool IsValid();

		virtual bool GetBanner(u32* _pBannerImage);

		virtual bool GetName(std::string& _rName, DiscIO::IVolume::ECountry language);

		virtual bool GetCompany(std::string& _rCompany);

		virtual bool GetDescription(std::string& _rDescription, DiscIO::IVolume::ECountry language);


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

		// "opening.bnr" file format for JP/US console
		struct DVDBanner
		{
			u32 id; // 'BNR1'
			u32 padding[7];
			u16 image[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 96x32 texture image
			DVDBannerComment comment;
		};

		// "opening.bnr" file format for EU console
		struct DVDBanner2
		{
			u32 id; // 'BNR2'
			u32 padding[7];
			u16 image[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT]; // RGB5A3 96x32 texture image
			DVDBannerComment comment[6]; // Comments in six languages
		};


		// for banner decoding
		int lut3to8[8];
		int lut4to8[16];
		int lut5to8[32];

		u8* m_pBannerFile;

		bool m_IsValid;

		u32 decode5A3(u16 val);
		void decode5A3image(u32* dst, u16* src, int width, int height);
		BANNER_TYPE getBannerType();
};
} // namespace

#endif

