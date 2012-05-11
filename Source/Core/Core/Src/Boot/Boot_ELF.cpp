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
	
	// Use the same method as the DOL loader uses: search for mfspr from HID4,
	// which should only be used in Wii ELFs.
	// 
	// Likely to have some false positives/negatives, patches implementing a
	// better heuristic are welcome.

	u32 HID4_pattern = 0x7c13fba6;
	u32 HID4_mask = 0xfc1fffff;
	ElfReader reader(mem);
	bool isWii = false;

	for (int i = 0; i < reader.GetNumSections(); ++i)
	{
		if (reader.IsCodeSection(i))
		{
			for (unsigned int j = 0; j < reader.GetSectionSize(i) / sizeof (u32); ++j)
			{
				u32 word = Common::swap32(((u32*)reader.GetSectionDataPtr(i))[j]);
				if ((word & HID4_mask) == HID4_pattern)
				{
					isWii = true;
					break;
				}
			}
		}
	}

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
