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

#include "../PowerPC/PowerPC.h"
#include "Boot.h"
#include "../HLE/HLE.h"
#include "Boot_ELF.h"
#include "ElfReader.h"
#include "FileUtil.h"

bool CBoot::IsElfWii(const char *filename)
{
	/* We already check if filename existed before we called this function, so
	   there is no need for another check, just read the file right away */

	const u64 filesize = File::GetSize(filename);
	u8 *const mem = new u8[(size_t)filesize];

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(mem, (size_t)filesize);
	}

	ElfReader reader(mem);
	// TODO: Find a more reliable way to distinguish.
	bool isWii = reader.GetEntryPoint() >= 0x80004000;
	delete[] mem;
	
    return isWii;
}


bool CBoot::Boot_ELF(const char *filename)
{
	const u64 filesize = File::GetSize(filename);
	u8 *mem = new u8[(size_t)filesize];

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(mem, (size_t)filesize);
	}
	
	ElfReader reader(mem);
	reader.LoadInto(0x80000000);
	if (!reader.LoadSymbols())
	{
		if (LoadMapFromFilename(filename))
			HLE::PatchFunctions();
	}
	else
	{
		HLE::PatchFunctions();
	}
	
	PC = reader.GetEntryPoint();
	delete[] mem;

    return true;
}
