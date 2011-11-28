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

	pVolume->GetTitleID((u8*)&TitleID);

	TitleID = Common::swap64(TitleID);

	sprintf(Filename, "%stitle/%08x/%08x/data/banner.bin",
		    File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(TitleID>>32), (u32)TitleID);

	if (!File::Exists(Filename))
	{
		// TODO(XK): Finish the 'commented' code. Turns out the banner.bin
		//           from the savefiles is very different from the banner.bin
		//           inside opening.bnr
#if 0
		char bnrFilename[260], titleFolder[260];

		// Creating title folder
		sprintf(titleFolder, "%stitle/%08x/%08x/data/",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(TitleID>>32), (u32)TitleID);
		if(!File::Exists(titleFolder))
			File::CreateFullPath(titleFolder);
		
		// Extracting banner.bin from opening.bnr
		sprintf(bnrFilename, "%stitle/%08x/%08x/data/opening.bnr",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(TitleID>>32), (u32)TitleID);

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
		File::IOFile pFile(Filename, "rb");
		if (pFile)
		{
			pFile.ReadBytes(m_pBannerFile, FileSize);
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

bool CBannerLoaderWii::GetStringFromComments(const CommentIndex index, std::string& s)
{
	bool ret = false;

	if (IsValid())
	{
		// find Banner type
		SWiiBanner *pBanner = (SWiiBanner*)m_pBannerFile;

		// Ensure the string is null-terminating, since the banner format
		// doesn't require it
		u16 *src = new u16[COMMENT_SIZE + 1];
		memcpy(src, &pBanner->m_Comment[index], COMMENT_SIZE * sizeof(u16));
		src[COMMENT_SIZE] = 0;

		ret = CopyBeUnicodeToString(s, src, COMMENT_SIZE + 1);

		delete [] src;
	}

	return ret;
}

bool CBannerLoaderWii::GetStringFromComments(const CommentIndex index, std::wstring& s)
{
	if (IsValid())
	{
		// find Banner type
		SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;

		std::wstring description;
		for (int i = 0; i < COMMENT_SIZE; ++i)
			description.push_back(Common::swap16(pBanner->m_Comment[index][i]));

		s = description;
		return true;
	}
	return false;
}

bool CBannerLoaderWii::GetName(std::string* _rName)
{
	return GetStringFromComments(NAME_IDX, *_rName);
}

bool CBannerLoaderWii::GetName(std::vector<std::wstring>&  _rNames)
{
	std::wstring temp;
	bool ret = GetStringFromComments(NAME_IDX, temp);
	_rNames.push_back(temp);
	return ret;
}

bool CBannerLoaderWii::GetCompany(std::string& _rCompany)
{
    _rCompany = "N/A";
    return true;
}

bool CBannerLoaderWii::GetDescription(std::string* _rDescription)
{
	return GetStringFromComments(DESC_IDX, *_rDescription);
}

bool CBannerLoaderWii::GetDescription(std::wstring& _rDescription)
{
	return GetStringFromComments(DESC_IDX, _rDescription);
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
