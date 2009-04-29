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
#include "ColorUtil.h"
#include "BannerLoaderWii.h"
#include "FileUtil.h"

namespace DiscIO
{

CBannerLoaderWii::CBannerLoaderWii(DiscIO::IFileSystem& _rFileSystem)	
	: m_pBannerFile(NULL)
	, m_IsValid(false)
{
	char Filename[260];
	u64 TitleID;

	_rFileSystem.GetVolume()->RAWRead((u64)0x0F8001DC, 8, (u8*)&TitleID);
	TitleID = Common::swap64(TitleID);

	sprintf(Filename, FULL_WII_USER_DIR "title/%08x/%08x/data/banner.bin",
		    (u32)(TitleID>>32), (u32)TitleID);

	if (!File::Exists(Filename))
	{
		m_IsValid = false;
		return;
	}

	// load the opening.bnr
	size_t FileSize = (size_t) File::GetSize(Filename);

	if (FileSize > 0)
	{
		m_pBannerFile = new u8[FileSize];
		FILE* pFile = fopen(Filename, "rb");
		if (pFile)
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

bool CBannerLoaderWii::IsValid()
{
	return m_IsValid;
}

bool CBannerLoaderWii::GetBanner(u32* _pBannerImage)
{
	if (IsValid())
	{
		SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;

		u32 Buffer[192 * 64];
		decode5A3image(Buffer, (u16*)pBanner->m_BannerTexture, 192, 64);

		// ugly scaling :) TODO: at least a 2x2 box filter, preferably a 3x3 gaussian :)
		for (int y = 0; y < 32; y++)
		{
			for (int x = 0; x < 96; x++)
			{
				_pBannerImage[y*96+x] = Buffer[(y*192*2) + (x*2)];
			}
		}	
	}
	return true;
}

bool CBannerLoaderWii::GetName(std::string* _rName)
{
	if (IsValid())
	{
		// find Banner type
		SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;

		std::string name;
		if (CopyBeUnicodeToString(name, pBanner->m_Comment[0], WII_BANNER_COMMENT_SIZE))
		{
			for (int i = 0; i < 6; i++)
			{
				_rName[i] = name;
			}
			return true;
		}	
	}
	return false;
}

bool CBannerLoaderWii::GetCompany(std::string& _rCompany)
{
    _rCompany = "N/A";
    return true;
}

bool CBannerLoaderWii::GetDescription(std::string* _rDescription)
{
	if (IsValid())
	{
		// find Banner type
		SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;

		std::string description;
		if (CopyBeUnicodeToString(description, pBanner->m_Comment[1], WII_BANNER_COMMENT_SIZE))
		{
			for (int i = 0; i< 6; i++)
			{
				_rDescription[i] = description;
			}
			return true;
		}
	}
	return false;
}

void CBannerLoaderWii::decode5A3image(u32* dst, u16* src, int width, int height)
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

} // namespace
