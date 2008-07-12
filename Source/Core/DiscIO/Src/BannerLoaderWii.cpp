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
#include "BannerLoaderWii.h"
#include "FileHandlerARC.h"
// #include "FileHandlerLZ77.h"

namespace DiscIO
{
CBannerLoaderWii::CBannerLoaderWii(DiscIO::IFileSystem& _rFileSystem)
	: m_pBuffer(NULL)
{
	FILE* pFile = fopen("e:\\opening.bnr", "rb");

	if (pFile)
	{
		fseek(pFile, 0, SEEK_END);
		int insize = ftell(pFile);
		fseek(pFile, 0, SEEK_SET);

		m_pBuffer = new u8[insize];

		fread(m_pBuffer, 1, insize, pFile);

		fclose(pFile);

		CARCFile ArcFile(m_pBuffer + 0x600, insize - 0x600);

		size_t BannerSize = ArcFile.GetFileSize("meta\\banner.bin");

		if (BannerSize > 0)
		{
			u8* TempBuffer = new u8[BannerSize];
			ArcFile.ReadFile("meta\\banner.bin", TempBuffer, BannerSize);

//			CLZ77File File(TempBuffer + 0x20 + 4, BannerSize - 0x20 - 4);
			delete[] TempBuffer;

//			CARCFile IconFile(File.GetBuffer(), File.GetSize());

//			IconFile.ExportFile("arc\\anim\\banner_loop.brlan", "e:\\banner_loop.brlan");

			//IconFile.ReadFile("meta\\icon.bin", TempBuffer, BannerSize);
		}
	}


	m_Name = std::string("Wii: ") + _rFileSystem.GetVolume().GetName();
}


CBannerLoaderWii::~CBannerLoaderWii()
{
	delete m_pBuffer;
}


bool
CBannerLoaderWii::IsValid()
{
	return(true);
}


bool
CBannerLoaderWii::GetBanner(u32* _pBannerImage)
{
	return(false);
}


bool
CBannerLoaderWii::GetName(std::string& _rName)
{
	_rName = m_Name;
	return(true);
}


bool
CBannerLoaderWii::GetCompany(std::string& _rCompany)
{
	return(false);
}


bool
CBannerLoaderWii::GetDescription(std::string& _rDescription)
{
	return(false);
}
} // namespace

