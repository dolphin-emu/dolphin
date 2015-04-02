// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "DiscIO/FileHandler/BannerLoaderWii.h"
#include "DiscIO/Volume/Volume.h"

namespace DiscIO
{

CBannerLoaderWii::CBannerLoaderWii(DiscIO::IVolume *pVolume)
{
	u64 TitleID = 0;
	pVolume->GetTitleID((u8*)&TitleID);
	TitleID = Common::swap64(TitleID);

	std::string Filename = StringFromFormat("%stitle/%08x/%08x/data/banner.bin",
		File::GetUserPath(D_WIIUSER_IDX).c_str(), (u32)(TitleID>>32), (u32)TitleID);

	if (!File::Exists(Filename))
	{
		m_IsValid = false;
		return;
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
		m_pBannerFile = nullptr;
	}
}

std::vector<u32> CBannerLoaderWii::GetBanner(int* pWidth, int* pHeight)
{
	SWiiBanner* pBanner = (SWiiBanner*)m_pBannerFile;
	std::vector<u32> Buffer;
	Buffer.resize(192 * 64);
	ColorUtil::decode5A3image(&Buffer[0], (u16*)pBanner->m_BannerTexture, 192, 64);
	*pWidth = 192;
	*pHeight = 64;
	return Buffer;
}

bool CBannerLoaderWii::GetStringFromComments(const CommentIndex index, std::string& result)
{
	if (IsValid())
	{
		auto const banner = reinterpret_cast<const SWiiBanner*>(m_pBannerFile);
		auto const src_ptr = banner->m_Comment[index];

		// Trim at first nullptr
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

} // namespace
