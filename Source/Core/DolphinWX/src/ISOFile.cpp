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

#include "Common.h"
#include "Globals.h"
#include "ISOFile.h"

#include "VolumeCreator.h"
#include "Filesystem.h"
#include "BannerLoader.h"
#include "FileSearch.h"

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

static u32 g_ImageTemp[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];

CISOFile::CISOFile(const std::string& _rFileName)
	: m_FileName(_rFileName),
	m_FileSize(0),
	m_Valid(false)
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_rFileName);

	if (pVolume != NULL)
	{
		m_Name = _rFileName;
		m_Country  = pVolume->GetCountry();
		m_FileSize = pVolume->GetSize();
		m_Name = pVolume->GetName();

		// check if we can get some infos from the banner file too
		DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(*pVolume);

		if (pFileSystem != NULL)
		{
			DiscIO::IBannerLoader* pBannerLoader = DiscIO::CreateBannerLoader(*pFileSystem);

			if (pBannerLoader != NULL)
			{
				if (pBannerLoader->IsValid())
				{
					pBannerLoader->GetName(m_Name);
					pBannerLoader->GetCompany(m_Company);

					if (pBannerLoader->GetBanner(g_ImageTemp))
					{
						unsigned char* pImage = (unsigned char*)malloc(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT * 3);

						for (size_t i = 0; i < DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT; i++)
						{
							pImage[i * 3 + 0] = (g_ImageTemp[i] & 0xFF0000) >> 16;
							pImage[i * 3 + 1] = (g_ImageTemp[i] & 0x00FF00) >>  8;
							pImage[i * 3 + 2] = (g_ImageTemp[i] & 0x0000FF) >>  0;
						}

						m_Image.Create(DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT, pImage);
					}
				}
				else
				{
					// default banner
				}

				delete pBannerLoader;
			}

			delete pFileSystem;
		}

		delete pVolume;

		m_Valid = true;
	}
}


CISOFile::~CISOFile()
{}


