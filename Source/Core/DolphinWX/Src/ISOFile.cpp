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

#include <string>
#include <vector>
#include <wx/mstream.h>

#include "Globals.h"
#include "FileUtil.h"
#include "ISOFile.h"
#include "StringUtil.h"

#include "VolumeCreator.h"
#include "Filesystem.h"
#include "BannerLoader.h"
#include "FileSearch.h"
#include "CompressedBlob.h"
#include "ChunkFile.h"
#include "../resources/no_banner.cpp"

#define CACHE_REVISION 0x103

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

static u32 g_ImageTemp[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];

GameListItem::GameListItem(const std::string& _rFileName)
	: m_FileName(_rFileName)
	, m_FileSize(0)
	, m_Valid(false)
	, m_BlobCompressed(false)
	, m_pImage(NULL)
	, m_ImageSize(0)
{

	if (LoadFromCache())
	{
		m_Valid = true;
	}
	else
	{
		DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_rFileName);

		if (pVolume != NULL)
		{
			m_Name = _rFileName;
			m_Country  = pVolume->GetCountry();
			m_FileSize = File::GetSize(_rFileName.c_str());
			m_VolumeSize = pVolume->GetSize();
			m_Name = pVolume->GetName();
			m_UniqueID = pVolume->GetUniqueID();
			m_BlobCompressed = DiscIO::IsCompressedBlob(_rFileName.c_str());

			// check if we can get some infos from the banner file too
			DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

			if (pFileSystem != NULL)
			{
				DiscIO::IBannerLoader* pBannerLoader = DiscIO::CreateBannerLoader(*pFileSystem);

				if (pBannerLoader != NULL)
				{
					if (pBannerLoader->IsValid())
					{
						pBannerLoader->GetName(m_Name, 0); //m_Country == DiscIO::IVolume::COUNTRY_JAP ? 1 : 0);
						pBannerLoader->GetCompany(m_Company);
						pBannerLoader->GetDescription(m_Description);
						if (pBannerLoader->GetBanner(g_ImageTemp))
						{
							m_ImageSize = DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT * 3;
							m_pImage = new u8[m_ImageSize]; //(u8*)malloc(m_ImageSize);

							for (size_t i = 0; i < DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT; i++)
							{
								m_pImage[i * 3 + 0] = (g_ImageTemp[i] & 0xFF0000) >> 16;
								m_pImage[i * 3 + 1] = (g_ImageTemp[i] & 0x00FF00) >>  8;
								m_pImage[i * 3 + 2] = (g_ImageTemp[i] & 0x0000FF) >>  0;
							}		
						}
					}

					delete pBannerLoader;
				}

				delete pFileSystem;
			}

			delete pVolume;

			m_Valid = true;

			SaveToCache();
		}
	}

	// i am not sure if this is a leak or if wxImage will release the code
	if (m_pImage)
	{
#if !defined(OSX64)
		m_Image.Create(DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT, m_pImage);
#endif
	}
	else
	{
		// default banner
		wxMemoryInputStream istream(no_banner_png, sizeof no_banner_png);
		wxImage iNoBanner(istream, wxBITMAP_TYPE_PNG);
		m_Image = iNoBanner;
	}
}


GameListItem::~GameListItem()
{
}

bool GameListItem::LoadFromCache()
{
	return CChunkFileReader::Load<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::SaveToCache()
{
	if (!File::IsDirectory("ISOCache"))
	{
		File::CreateDir("ISOCache");
	}

	CChunkFileReader::Save<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::DoState(PointerWrap &p)
{
	p.Do(m_Name);
	p.Do(m_Company);
	p.Do(m_Description);
	p.Do(m_UniqueID);
	p.Do(m_FileSize);
	p.Do(m_VolumeSize);
	p.Do(m_Country);
	p.Do(m_BlobCompressed);
	p.DoBuffer(&m_pImage, m_ImageSize);
}

std::string GameListItem::CreateCacheFilename()
{
	std::string Filename;
	SplitPath(m_FileName, NULL, &Filename, NULL);
	Filename.append(".cache");

	std::string fullname("ISOCache\\");
	fullname += Filename;
	return fullname;
}
