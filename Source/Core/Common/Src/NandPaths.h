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

#ifndef __NANDPATHS_H__
#define __NANDPATHS_H__

#include <string>
#include "CommonTypes.h"

#define TITLEID_SYSMENU 0x0000000100000002ull

namespace Common
{
	typedef std::pair<char, std::string> replace_t;
	typedef std::vector<replace_t> replace_v;

	std::string GetTicketFileName(u64 _titleID);
	std::string GetTMDFileName(u64 _titleID);
	std::string GetTitleDataPath(u64 _titleID);
	std::string GetTitleContentPath(u64 _titleID);
	bool CheckTitleTMD(u64 _titleID);
	bool CheckTitleTIK(u64 _titleID);
	void ReadReplacements(replace_v& replacements);
}
#endif

