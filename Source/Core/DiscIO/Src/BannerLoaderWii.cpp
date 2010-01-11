// Copyright (C) 2003 Dolphin Project.

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
#include "VolumeCreator.h"
#include "FileUtil.h"
#include "FileHandlerARC.h"

namespace DiscIO
{

CBannerLoaderWii::CBannerLoaderWii(DiscIO::IVolume *pVolume)	
	: m_pBannerFile(NULL)
	, m_IsValid(false)
{
	char Filename[260];
	u64 TitleID;

	if (DiscIO::IsVolumeWadFile(pVolume))
		pVolume->GetTitleID((u8*)&TitleID);
	else
		pVolume->RAWRead((u64)0x0F8001DC, 8, (u8*)&TitleID);

	TitleID = Common::swap64(TitleID);

	sprintf(Filename, FULL_WII_USER_DIR "title/%08x/%08x/data/banner.bin",
		    (u32)(TitleID>>32), (u32)TitleID);

	if (!File::Exists(Filename))
	{
		// TODO(XK): Finish the 'commented' code. Turns out the banner.bin
		//           from the savefiles is very different from the banner.bin
		//           inside opening.bnr
#if 0
		char bnrFilename[260], titleFolder[260];

		// Creating title folder
		sprintf(titleFolder, FULL_WII_USER_DIR "title/%08x/%08x/data/",
			(u32)(TitleID>>32), (u32)TitleID);
		if(!File::Exists(titleFolder))
			File::CreateFullPath(titleFolder);
		
		// Extracting banner.bin from opening.bnr
		sprintf(bnrFilename, FULL_WII_USER_DIR "title/%08x/%08x/data/opening.bnr",
			(u32)(TitleID>>32), (u32)TitleID);

		if(!_rFileSystem.ExportFile("opening.bnr", bnrFilename)) {
			m_IsValid = false;
			return;
		}

		CARCFile bnrArc (bnrFilename, 0x600);

		if(!bnrArc.ExportFile("meta/banner.bin", Filename)) {
			m_IsValid = false;
			return;
		}

		// Now we have an LZ77-compressed file with a short IMD5 header
		// TODO: Finish the job
		
		File::Delete(bnrFilename);
#else
		m_IsValid = false;
		return;
#endif
	}

	// load the banner.bin
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

static inline u32 Average32(u32 a, u32 b) {
	return ((a >> 1) & 0x7f7f7f7f) + ((b >> 1) & 0x7f7f7f7f);
}

static inline u32 GetPixel(u32 *buffer, unsigned int x, unsigned int y) {
	// thanks to unsignedness, these also check for <0 automatically.
	if (x > 191) return 0;
	if (y > 63) return 0;
	return buffer[y * 192 + x];
}

bool CBannerLoaderWii::GetBanner(u32* _pBannerImage)
{
	if (IsValid())
	{
		SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;
		u32* Buffer = new u32[192 * 64];
		decode5A3image(Buffer, (u16*)pBanner->m_BannerTexture, 192, 64);
		for (int y = 0; y < 32; y++)
		{
			for (int x = 0; x < 96; x++)
			{
				// simplified plus-shaped "gaussian"
				u32 surround = Average32(
					Average32(GetPixel(Buffer, x*2 - 1, y*2), GetPixel(Buffer, x*2 + 1, y*2)),
					Average32(GetPixel(Buffer, x*2, y*2 - 1), GetPixel(Buffer, x*2, y*2 + 1)));
				_pBannerImage[y * 96 + x] = Average32(GetPixel(Buffer, x*2, y*2), surround);
			}
		}
		delete[] Buffer;
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
