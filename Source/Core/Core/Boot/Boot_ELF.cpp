// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"

#include "Core/Boot/Boot.h"
#include "Core/Boot/ElfReader.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/PowerPC.h"

bool CBoot::IsElfWii(const std::string& filename)
{
	/* We already check if filename existed before we called this function, so
	   there is no need for another check, just read the file right away */

	const u64 filesize = File::GetSize(filename);
	std::vector<u8> mem((size_t)filesize);

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(mem.data(), (size_t)filesize);
	}

	// Use the same method as the DOL loader uses: search for mfspr from HID4,
	// which should only be used in Wii ELFs.
	//
	// Likely to have some false positives/negatives, patches implementing a
	// better heuristic are welcome.

	u32 HID4_pattern = 0x7c13fba6;
	u32 HID4_mask = 0xfc1fffff;
	ElfReader reader(mem.data());

	for (int i = 0; i < reader.GetNumSections(); ++i)
	{
		if (reader.IsCodeSection(i))
		{
			for (unsigned int j = 0; j < reader.GetSectionSize(i) / sizeof (u32); ++j)
			{
				u32 word = Common::swap32(((u32*)reader.GetSectionDataPtr(i))[j]);
				if ((word & HID4_mask) == HID4_pattern)
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool CBoot::Boot_ELF(const std::string& filename)
{
	const u64 filesize = File::GetSize(filename);
	std::vector<u8> mem((size_t)filesize);

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(mem.data(), (size_t)filesize);
	}

	ElfReader reader(mem.data());
	reader.LoadInto(0x80000000);
	if (!reader.LoadSymbols())
	{
		if (LoadMapFromFilename())
			HLE::PatchFunctions();
	}
	else
	{
		HLE::PatchFunctions();
	}

	PC = reader.GetEntryPoint();

	return true;
}
