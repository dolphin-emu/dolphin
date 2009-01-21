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

#include "stdafx.h"

#include <stdio.h>

#include "Common.h"
#include "BannerLoaderWii.h"
#include "FileUtil.h"

namespace DiscIO
{
CBannerLoaderWii::CBannerLoaderWii(DiscIO::IFileSystem& _rFileSystem)	
	: m_pBannerFile(NULL)
	, m_IsValid(false)
{
	InitLUTTable();
	
	char Filename[260];
	char TitleID[4];
	
	_rFileSystem.GetVolume()->Read(0, 4, (u8*)TitleID);
	sprintf(Filename, FULL_WII_USER_DIR "title/00010000/%02x%02x%02x%02x/data/banner.bin", (u8)TitleID[0], (u8)TitleID[1], (u8)TitleID[2], (u8)TitleID[3]);

	// load the opening.bnr
	size_t FileSize = (size_t) File::GetSize(Filename);

	if (FileSize > 0)
	{
		m_pBannerFile = new u8[FileSize];
		FILE* pFile = fopen(Filename, "rb");
		if ((pFile != NULL) && (m_pBannerFile != NULL))
		{
			fread(m_pBannerFile, FileSize, 1, pFile);
			fclose(pFile);
			m_IsValid = true;
		}
	}
}

CBannerLoaderWii::~CBannerLoaderWii()
{
	if (m_pBannerFile)
	{
		delete [] m_pBannerFile;
		m_pBannerFile = NULL;
	}
}


bool
CBannerLoaderWii::IsValid()
{
	return (m_IsValid);
}


bool
CBannerLoaderWii::GetBanner(u32* _pBannerImage)
{
	if (!IsValid())
	{
		return false;
	}

	SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;

	static u32 Buffer[192 * 64];
	decode5A3image(Buffer, (u16*)pBanner->m_BannerTexture, 192, 64);

	// ugly scaling :)
	for (int y=0; y<32; y++)
	{
		for (int x=0; x<96; x++)
		{
			_pBannerImage[y*96+x] = Buffer[(y*192*2)+(x*2)];
		}
	}


	return true;
}

std::string 
CBannerLoaderWii::StupidWideCharToString(u16* _pSrc, size_t _max)
{
	std::string temp;

	u32 offset = 0;
	while (_pSrc[offset] != 0x0000)
	{
		temp += (char)(_pSrc[offset] >> 8);
		offset ++;

		if (offset >= _max)
			break;
	}

	return temp;
}

bool
CBannerLoaderWii::GetName(std::string& _rName, DiscIO::IVolume::ECountry language)
{
	_rName = "no name";

	if (!IsValid())
	{
		return false;
	}

	// find Banner type
	SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;
	if (DiscIO::IVolume::COUNTRY_JAP == language)
	{
		return CopyUnicodeToString(_rName, pBanner->m_Comment[0]);
	}
	else
	{
		// very stupid
		_rName = StupidWideCharToString(pBanner->m_Comment[0], WII_BANNER_COMMENT_SIZE);
		return true;
	}
	return true;
}


bool
CBannerLoaderWii::GetCompany(std::string& _rCompany)
{
    _rCompany = "N/A";
    return true;
}


bool
CBannerLoaderWii::GetDescription(std::string& _rDescription, DiscIO::IVolume::ECountry language)
{
	_rDescription = "";

	if (!IsValid())
	{
		return false;
	}

	// find Banner type
	SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;
	if (DiscIO::IVolume::COUNTRY_JAP == language)
	{
		return CopyUnicodeToString(_rDescription, pBanner->m_Comment[1]);
	}
	else
	{
		// very stupid
		_rDescription = StupidWideCharToString(pBanner->m_Comment[1], WII_BANNER_COMMENT_SIZE);
		return true;
	}

	return false;
}


void 
CBannerLoaderWii::InitLUTTable()
{
	// build LUT Table
	for (int i = 0; i < 8; i++)
	{
		lut3to8[i] = (i * 255) / 7;
	}

	for (int i = 0; i < 16; i++)
	{
		lut4to8[i] = (i * 255) / 15;
	}

	for (int i = 0; i < 32; i++)
	{
		lut5to8[i] = (i * 255) / 31;
	}
}

u32
CBannerLoaderWii::decode5A3(u16 val)
{
	u32 bannerBGColor = 0x00000000;

	int r, g, b, a;

	if ((val & 0x8000))
	{
		r = lut5to8[(val >> 10) & 0x1f];
		g = lut5to8[(val >> 5) & 0x1f];
		b = lut5to8[(val) & 0x1f];
		a = 0xFF;
	}
	else
	{
		a = lut3to8[(val >> 12) & 0x7];
		r = (lut4to8[(val >> 8) & 0xf] * a + (bannerBGColor & 0xFF) * (255 - a)) / 255;
		g = (lut4to8[(val >> 4) & 0xf] * a + ((bannerBGColor >> 8) & 0xFF) * (255 - a)) / 255;
		b = (lut4to8[(val) & 0xf] * a + ((bannerBGColor >> 16) & 0xFF) * (255 - a)) / 255;
		a = 0xFF;
	}

	return ((a << 24) | (r << 16) | (g << 8) | b);
}


void
CBannerLoaderWii::decode5A3image(u32* dst, u16* src, int width, int height)
{
	for (int y = 0; y < height; y += 4)
	{
		for (int x = 0; x < width; x += 4)
		{
			for (int iy = 0; iy < 4; iy++, src += 4)
			{
				for (int ix = 0; ix < 4; ix++)
				{
					u32 RGBA = decode5A3(Common::swap16(src[ix]));
					dst[(y + iy) * width + (x + ix)] = RGBA;
				}
			}
		}
	}
}

} // namespace

