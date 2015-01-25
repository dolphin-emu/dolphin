// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/StdMakeUnique.h"

#include "Core/Boot/Boot.h"
#include "Core/Boot/ElfReader.h"
#include "Core/HLE/HLE.h"
#include "Core/PowerPC/PowerPC.h"

bool CBoot::IsElfWii(const std::string& filename)
{
	/* We already check if filename existed before we called this function, so
	   there is no need for another check, just read the file right away */

	size_t filesize = File::GetSize(filename);
	auto elf = std::make_unique<u8 []>(filesize);

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(elf.get(), filesize);
	}

	// Use the same method as the DOL loader uses: search for mfspr from HID4,
	// which should only be used in Wii ELFs.
	//
	// Likely to have some false positives/negatives, patches implementing a
	// better heuristic are welcome.

	// Swap these once, instead of swapping every word in the file.
	u32 HID4_pattern = Common::swap32(0x7c13fba6);
	u32 HID4_mask = Common::swap32(0xfc1fffff);
	ElfReader reader(elf.get());

	for (int i = 0; i < reader.GetNumSections(); ++i)
	{
		if (reader.IsCodeSection(i))
		{
			u32* code = (u32*)reader.GetSectionDataPtr(i);
			for (u32 j = 0; j < reader.GetSectionSize(i) / sizeof(u32); ++j)
			{
				if ((code[j] & HID4_mask) == HID4_pattern)
					return true;
			}
		}
	}

	return false;
}


bool CBoot::Boot_ELF(const std::string& filename)
{
	// Read ELF from file
	size_t filesize = File::GetSize(filename);
	auto elf = std::make_unique<u8 []>(filesize);

	{
	File::IOFile f(filename, "rb");
	f.ReadBytes(elf.get(), filesize);
	}

	// Load ELF into GameCube Memory
	ElfReader reader(elf.get());
	if(!reader.LoadIntoMemory())
		return false;

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
