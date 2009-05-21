// Copyright (C) 2003-2009 Dolphin Project.

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

// HyperIris: need clean code
#include "../../Core/Src/ConfigManager.h"

#include "ColorUtil.h"
#include "BannerLoaderGC.h"

namespace DiscIO
{
CBannerLoaderGC::CBannerLoaderGC(DiscIO::IFileSystem& _rFileSystem)
	: m_pBannerFile(NULL),
	m_IsValid(false)
{
	// load the opening.bnr
	size_t FileSize = (size_t) _rFileSystem.GetFileSize("opening.bnr");

	if (FileSize > 0)
	{
		m_pBannerFile = new u8[FileSize];
		if (m_pBannerFile)
		{
			_rFileSystem.ReadFile("opening.bnr", m_pBannerFile, FileSize);
			m_IsValid = true;
		}
	}
}


CBannerLoaderGC::~CBannerLoaderGC()
{
	if (m_pBannerFile)
	{
		delete [] m_pBannerFile;
		m_pBannerFile = NULL;
	}
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
CBannerLoaderGC::GetName(std::string _rName[])
{
	bool returnCode = false;

	if (!IsValid())
	{
		return(false);
	}

	// find Banner type
	switch (getBannerType())
	{
	case CBannerLoaderGC::BANNER_BNR1:
		{
			DVDBanner* pBanner = (DVDBanner*)m_pBannerFile;
			char tempBuffer[65] = {0};
			if (pBanner->comment.longTitle[0])
			{
				memcpy(tempBuffer, pBanner->comment.longTitle, 64);
			}
			else
			{
				memcpy(tempBuffer, pBanner->comment.shortTitle, 32);
			}
			for (int i = 0; i < 6; i++)
			{
				CopyToStringAndCheck(_rName[i], tempBuffer);
			}
			returnCode = true;
		}
		break;
	case CBannerLoaderGC::BANNER_BNR2:
		{
			DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;

			u32 languageID = SConfig::GetInstance().m_InterfaceLanguage;
			for (int i = 0; i < 6; i++)
			{
				char tempBuffer[65] = {0};
				if (pBanner->comment[i].longTitle[0])
				{
					memcpy(tempBuffer, pBanner->comment[i].longTitle, 64);
				}
				else
				{
					memcpy(tempBuffer, pBanner->comment[i].shortTitle, 32);
				}
				CopyToStringAndCheck(_rName[i], tempBuffer);
			}

			returnCode = true;

		}
		break;
	default:
		break;
	}
	
	return returnCode;
}


bool
CBannerLoaderGC::GetCompany(std::string& _rCompany)
{
	_rCompany = "N/A";

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
CBannerLoaderGC::GetDescription(std::string* _rDescription)
{
	bool returnCode = false;

	if (!IsValid())
	{
		return(false);
	}

	// find Banner type
	switch (getBannerType())
	{
	case CBannerLoaderGC::BANNER_BNR1:
		{
			DVDBanner* pBanner = (DVDBanner*)m_pBannerFile;
			char tempBuffer[129] = {0};
			memcpy(tempBuffer, pBanner->comment.comment, 128);
			for (int i = 0; i < 6; i++)
			{
				CopyToStringAndCheck(_rDescription[i], tempBuffer);
			}
			returnCode = true;
		}
		break;
	case CBannerLoaderGC::BANNER_BNR2:
		{
			DVDBanner2* pBanner = (DVDBanner2*)m_pBannerFile;

			for (int i = 0; i< 6; i++)
			{
				char tempBuffer[129] = {0};
				memcpy(tempBuffer, pBanner->comment[i].comment, 128);
				CopyToStringAndCheck(_rDescription[i], tempBuffer);
			}
			returnCode = true;
		}
		break;
	default:
		break;
	}
	return returnCode;
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
	case 0x31524e42:
		type = CBannerLoaderGC::BANNER_BNR1;
		break;
	case 0x32524e42:
		type = CBannerLoaderGC::BANNER_BNR2;
		break;
	}
	return type;
}
} // namespace
