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
std::string CreateTicketFileName(u64 _TitleID)
{
	char TicketFilename[1024];
	sprintf(TicketFilename, "%sticket/%08x/%08x.tik", File::GetUserPath(D_WIIUSER_IDX), (u32)(_TitleID >> 32), (u32)_TitleID);

	return TicketFilename;
}

std::string CreateTitleContentPath(u64 _TitleID)
{
	char ContentPath[1024];
	sprintf(ContentPath, "%stitle/%08x/%08x/content", File::GetUserPath(D_WIIUSER_IDX), (u32)(_TitleID >> 32), (u32)_TitleID);

	return ContentPath;
}

};
