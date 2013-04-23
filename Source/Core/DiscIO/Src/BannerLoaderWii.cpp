// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <stdio.h>
#include <algorithm>

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

bool CBannerLoaderWii::GetStringFromComments(const CommentIndex index, std::string& result)
{
	if (IsValid())
	{
		auto const banner = reinterpret_cast<const SWiiBanner*>(m_pBannerFile);
		auto const src_ptr = banner->m_Comment[index];
		
		// Trim at first NULL
		auto const length = std::find(src_ptr, src_ptr + COMMENT_SIZE, 0x0) - src_ptr;
		
		std::wstring src;
		src.resize(length);
		std::transform(src_ptr, src_ptr + src.size(), src.begin(), (u16(&)(u16))Common::swap16);
		result = UTF16ToUTF8(src);
		
		return true;
	}

	return false;
}

std::vector<std::string> CBannerLoaderWii::GetNames()
{
	std::vector<std::string> ret(1);
	
	if (!GetStringFromComments(NAME_IDX, ret[0]))
		ret.clear();

	return ret;
}

std::string CBannerLoaderWii::GetCompany()
{
	return "";
}

std::vector<std::string> CBannerLoaderWii::GetDescriptions()
{
	std::vector<std::string> result(1);
	if (!GetStringFromComments(DESC_IDX, result[0]))
		result.clear();
	return result;
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
