// Copyright (C) 2010 Dolphin Project.

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

#include "CommonPaths.h"
#include "FileUtil.h"
#include "NandPaths.h"

namespace Common
{

std::string CreateTicketFileName(u64 _titleID)
{
	char TicketFilename[1024];
	sprintf(TicketFilename, "%sticket/%08x/%08x.tik", File::GetUserPath(D_WIIUSER_IDX), (u32)(_titleID >> 32), (u32)_titleID);

	return TicketFilename;
}

std::string CreateTitleDataPath(u64 _titleID)
{
	char path[1024];
	sprintf(path, "%stitle/%08x/%08x/data", File::GetUserPath(D_WIIUSER_IDX), (u32)(_titleID >> 32), (u32)_titleID);

	return path;
}

std::string CreateTitleContentPath(u64 _titleID)
{
	char ContentPath[1024];
	sprintf(ContentPath, "%stitle/%08x/%08x/content", File::GetUserPath(D_WIIUSER_IDX), (u32)(_titleID >> 32), (u32)_titleID);

	return ContentPath;
}

bool CheckTitleTMD(u64 _titleID)
{
	std::string TitlePath;
	TitlePath = CreateTitleContentPath(_titleID) + "/title.tmd";
	if (File::Exists(TitlePath.c_str()))
	{
		FILE* pTMDFile = fopen(TitlePath.c_str(), "rb");
		if(pTMDFile)
		{
			u64 TitleID = 0xDEADBEEFDEADBEEFULL;
			fseeko(pTMDFile, 0x18C, SEEK_SET);
			fread(&TitleID, 8, 1, pTMDFile);
			fclose(pTMDFile);
			if (_titleID == Common::swap64(TitleID))
				return true;
		}
	}
	INFO_LOG(DISCIO, "Invalid or no tmd for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

bool CheckTitleTIK(u64 _titleID)
{
	std::string TikPath = Common::CreateTicketFileName(_titleID);	
	if (File::Exists(TikPath.c_str()))
	{
		FILE* pTIKFile = fopen(TikPath.c_str(), "rb");
		if(pTIKFile)
		{
			u64 TitleID = 0xDEADBEEFDEADBEEFULL;
			fseeko(pTIKFile, 0x1dC, SEEK_SET);
			fread(&TitleID, 8, 1, pTIKFile);
			fclose(pTIKFile);
			if (_titleID == Common::swap64(TitleID))
				return true;
		}
	}
	INFO_LOG(DISCIO, "Invalid or no tik for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

};
