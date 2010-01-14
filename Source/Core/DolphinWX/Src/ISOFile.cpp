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

#include <string>
#include <vector>
#include <wx/mstream.h>

#include "Globals.h"
#include "FileUtil.h"
#include "ISOFile.h"
#include "StringUtil.h"

#include "Filesystem.h"
#include "BannerLoader.h"
#include "FileSearch.h"
#include "CompressedBlob.h"
#include "ChunkFile.h"
#include "../resources/no_banner.cpp"

#define CACHE_REVISION 0x109

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
			if (!DiscIO::IsVolumeWadFile(pVolume))
				m_Platform = DiscIO::IsVolumeWiiDisc(pVolume) ? WII_DISC : GAMECUBE_DISC;
			else
				m_Platform = WII_WAD;

			m_Company = "N/A";
			for (int i = 0; i < 6; i++)
			{
				m_Name[i] = pVolume->GetName();
				if(m_Name[i] == "") // Couldn't find the name in the WAD...
				{
					std::string FileName;
					SplitPath(_rFileName, NULL, &FileName, NULL);
					m_Name[i] = FileName; // Then just display the filename... Better than something like "No Name"
				}
				m_Description[i] = "No Description";
			}
			m_Country  = pVolume->GetCountry();
			m_FileSize = File::GetSize(_rFileName.c_str());
			m_VolumeSize = pVolume->GetSize();

			m_UniqueID = pVolume->GetUniqueID();
			m_BlobCompressed = DiscIO::IsCompressedBlob(_rFileName.c_str());

			// check if we can get some infos from the banner file too
			DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

			if (pFileSystem != NULL || m_Platform == WII_WAD)
			{
				DiscIO::IBannerLoader* pBannerLoader = DiscIO::CreateBannerLoader(*pFileSystem, pVolume);

				if (pBannerLoader != NULL)
				{
					if (pBannerLoader->IsValid())
					{
						pBannerLoader->GetName(m_Name); //m_Country == DiscIO::IVolume::COUNTRY_JAP ? 1 : 0);
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

			// just if we have an image create a cache file
			// Wii isos create their images after you have generated the first savegame
			if (m_pImage)
				SaveToCache();
		}
	}

	// i am not sure if this is a leak or if wxImage will release the code
	if (m_pImage)
	{
#if defined(HAVE_WX) && HAVE_WX
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
	if (!File::IsDirectory(FULL_CACHE_DIR))
	{
		File::CreateDir(FULL_CACHE_DIR);
	}

	CChunkFileReader::Save<GameListItem>(CreateCacheFilename(), CACHE_REVISION, *this);
}

void GameListItem::DoState(PointerWrap &p)
{
	p.Do(m_Name[0]);	p.Do(m_Name[1]);	p.Do(m_Name[2]);
	p.Do(m_Name[3]);	p.Do(m_Name[4]);	p.Do(m_Name[5]);
	p.Do(m_Company);
	p.Do(m_Description[0]);	p.Do(m_Description[1]);	p.Do(m_Description[2]);
	p.Do(m_Description[3]);	p.Do(m_Description[4]);	p.Do(m_Description[5]);
	p.Do(m_UniqueID);
	p.Do(m_FileSize);
	p.Do(m_VolumeSize);
	p.Do(m_Country);
	p.Do(m_BlobCompressed);
	p.DoBuffer(&m_pImage, m_ImageSize);
	p.Do(m_Platform);
}

std::string GameListItem::CreateCacheFilename()
{
	std::string Filename;
	SplitPath(m_FileName, NULL, &Filename, NULL);

	if (Filename.empty()) return Filename; // Disc Drive
	// We add gcz to the cache file if the file is compressed to avoid it reading
	// the uncompressed file's cache if it has the same name, but not the same ext.
	if (DiscIO::IsCompressedBlob(m_FileName.c_str()))
		Filename.append(".gcz");
	Filename.append(".cache");

	std::string fullname(FULL_CACHE_DIR);
	fullname += Filename;
	return fullname;
}

const std::string& GameListItem::GetDescription(int index) const
{
	if ((index >=0) && (index < 6))
	{
		return m_Description[index];
	} 
	return m_Description[0];
}

const std::string& GameListItem::GetName(int index) const
{
	if ((index >=0) && (index < 6))
	{
		return m_Name[index];
	} 
	return m_Name[0];
}

const std::string GameListItem::GetWiiFSPath() const
{
	DiscIO::IVolume *Iso = DiscIO::CreateVolumeFromFilename(m_FileName);

	std::string ret("NULL");
	if (Iso != NULL)
	{
		if (DiscIO::IsVolumeWiiDisc(Iso) || DiscIO::IsVolumeWadFile(Iso))
		{
			char Path[250];
			u64 Title;

			Iso->GetTitleID((u8*)&Title);
			Title = Common::swap64(Title);

			sprintf(Path, FULL_WII_USER_DIR "title/%08x/%08x/data/", (u32)(Title>>32), (u32)Title);

			if (!File::Exists(Path))
				File::CreateFullPath(Path);

			ret = std::string(wxGetCwd().mb_str()) + std::string(Path).substr(strlen(ROOT_DIR));
		}
		delete Iso;
	}

	return ret;
}

