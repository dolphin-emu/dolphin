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

#include "BannerLoaderGC.h"

namespace DiscIO
{
CBannerLoaderGC::CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem)
	: m_pBannerFile(NULL),
	m_IsValid(false)
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

	// load the opening.bnr
	size_t FileSize = _rFileSystem.GetFileSize("opening.bnr");

	if (FileSize > 0)
	{
		m_pBannerFile = new u8[FileSize];
		_rFileSystem.ReadFile("opening.bnr", m_pBannerFile, FileSize);
		m_IsValid = true;
	}
}


CBannerLoaderGC::~CBannerLoaderGC()
{
	delete[] m_pBannerFile;
}


bool
CBannerLoaderGC::IsValid()
{
	return(m_IsValid);
}


bool
CBannerLoaderGC::GetBanner(u32* _pBannerImage)
{
	if (!IsValid())
	{
		return(false);
	}

	DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;
	decode5A3image(_pBannerImage, pBanner->image, DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT);

	return(true);
}


bool
CBannerLoaderGC::GetName(std::string& _rName)
{
	_rName = "invalid image";

	if (!IsValid())
	{
		return(false);
	}

	DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;

	if (!CopyToStringAndCheck(_rName, pBanner->comment[0].longTitle))
	{
		return(false);
	}

	return(true);
}


bool
CBannerLoaderGC::GetCompany(std::string& _rCompany)
{
	_rCompany = "invalid images";

	if (!IsValid())
	{
		return(false);
	}

	DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;

	if (!CopyToStringAndCheck(_rCompany, pBanner->comment[0].shortMaker))
	{
		return(false);
	}

	return(true);
}


bool
CBannerLoaderGC::GetDescription(std::string& _rDescription)
{
	_rDescription = "invalid images";

	if (!IsValid())
	{
		return(false);
	}

	DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;

	if (!CopyToStringAndCheck(_rDescription, pBanner->comment[0].comment))
	{
		_rDescription = "";
	}

	return(true);
}


u32
CBannerLoaderGC::decode5A3(u16 val)
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

	return((a << 24) | (r << 16) | (g << 8) | b);
}


void
CBannerLoaderGC::decode5A3image(u32* dst, u16* src, int width, int height)
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
					dst[(y + iy) * 96 + (x + ix)] = RGBA;
				}
			}
		}
	}
}
} // namespace
