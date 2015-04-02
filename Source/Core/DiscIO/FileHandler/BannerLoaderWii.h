// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/FileHandler/BannerLoader.h"

namespace DiscIO
{

class IVolume;

class CBannerLoaderWii
	: public IBannerLoader
{
public:
	CBannerLoaderWii(DiscIO::IVolume *pVolume);

	virtual ~CBannerLoaderWii();

	virtual std::vector<u32> GetBanner(int* pWidth, int* pHeight) override;

	virtual std::vector<std::string> GetNames() override;
	virtual std::string GetCompany() override;
	virtual std::vector<std::string> GetDescriptions() override;

private:
	enum
	{
		TEXTURE_SIZE = 192 * 64 * 2,
		ICON_SIZE    = 48 * 48 * 2,
		COMMENT_SIZE = 32
	};

	enum CommentIndex
	{
		NAME_IDX,
		DESC_IDX
	};

	struct SWiiBanner
	{
		u32 ID;

		u32 m_Flag;
		u16 m_Speed;
		u8  m_Unknown[22];

		// Not null terminated!
		u16 m_Comment[2][COMMENT_SIZE];
		u8  m_BannerTexture[TEXTURE_SIZE];
		u8  m_IconTexture[8][ICON_SIZE];
	};

	bool GetStringFromComments(const CommentIndex index, std::string& s);
};

} // namespace
