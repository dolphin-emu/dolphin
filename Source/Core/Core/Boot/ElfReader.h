// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/Boot/ElfTypes.h"

enum KnownElfTypes
{
	KNOWNELF_PSP = 0,
	KNOWNELF_DS  = 1,
	KNOWNELF_GBA = 2,
	KNOWNELF_GC  = 3,
};

typedef int SectionID;

class ElfReader
{
private:
	char* base;
	u32* base32;

	Elf32_Ehdr* header;
	Elf32_Phdr* segments;
	Elf32_Shdr* sections;

	u32 *sectionAddrs;
	bool bRelocate;
	u32 entryPoint;

public:
	ElfReader(void* ptr);
	~ElfReader() { }

	u32 Read32(int off) const { return base32[off>>2]; }

	// Quick accessors
	ElfType GetType() const { return (ElfType)(header->e_type); }
	ElfMachine GetMachine() const { return (ElfMachine)(header->e_machine); }
	u32 GetEntryPoint() const { return entryPoint; }
	u32 GetFlags() const { return (u32)(header->e_flags); }
	bool LoadIntoMemory();
	bool LoadSymbols();

	int GetNumSegments() const { return (int)(header->e_phnum); }
	int GetNumSections() const { return (int)(header->e_shnum); }
	const u8* GetPtr(int offset) const { return (u8*)base + offset; }
	const char* GetSectionName(int section) const;
	const u8 *GetSectionDataPtr(int section) const
	{
		if (section < 0 || section >= header->e_shnum)
			return nullptr;
		if (sections[section].sh_type != SHT_NOBITS)
			return GetPtr(sections[section].sh_offset);
		else
			return nullptr;
	}
	bool IsCodeSection(int section) const
	{
		return sections[section].sh_type == SHT_PROGBITS;
	}
	const u8* GetSegmentPtr(int segment)
	{
		return GetPtr(segments[segment].p_offset);
	}
	u32 GetSectionAddr(SectionID section) const { return sectionAddrs[section]; }
	int GetSectionSize(SectionID section) const { return sections[section].sh_size; }
	SectionID GetSectionByName(const char* name, int firstSection = 0) const; //-1 for not found

	bool DidRelocate() const
	{
		return bRelocate;
	}
};
