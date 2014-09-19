// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

#include "Core/Boot/ElfReader.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PPCSymbolDB.h"

static void bswap(u32 &w) {w = Common::swap32(w);}
static void bswap(u16 &w) {w = Common::swap16(w);}

static void byteswapHeader(Elf32_Ehdr &ELF_H)
{
	bswap(ELF_H.e_type);
	bswap(ELF_H.e_machine);
	bswap(ELF_H.e_ehsize);
	bswap(ELF_H.e_phentsize);
	bswap(ELF_H.e_phnum);
	bswap(ELF_H.e_shentsize);
	bswap(ELF_H.e_shnum);
	bswap(ELF_H.e_shstrndx);
	bswap(ELF_H.e_version);
	bswap(ELF_H.e_entry);
	bswap(ELF_H.e_phoff);
	bswap(ELF_H.e_shoff);
	bswap(ELF_H.e_flags);
}

static void byteswapSegment(Elf32_Phdr &sec)
{
	bswap(sec.p_align);
	bswap(sec.p_filesz);
	bswap(sec.p_flags);
	bswap(sec.p_memsz);
	bswap(sec.p_offset);
	bswap(sec.p_paddr);
	bswap(sec.p_vaddr);
	bswap(sec.p_type);
}

static void byteswapSection(Elf32_Shdr &sec)
{
	bswap(sec.sh_addr);
	bswap(sec.sh_addralign);
	bswap(sec.sh_entsize);
	bswap(sec.sh_flags);
	bswap(sec.sh_info);
	bswap(sec.sh_link);
	bswap(sec.sh_name);
	bswap(sec.sh_offset);
	bswap(sec.sh_size);
	bswap(sec.sh_type);
}

ElfReader::ElfReader(void *ptr)
{
	base = (char*)ptr;
	base32 = (u32 *)ptr;
	header = (Elf32_Ehdr*)ptr;
	byteswapHeader(*header);

	segments = (Elf32_Phdr *)(base + header->e_phoff);
	sections = (Elf32_Shdr *)(base + header->e_shoff);

	for (int i = 0; i < GetNumSegments(); i++)
	{
		byteswapSegment(segments[i]);
	}

	for (int i = 0; i < GetNumSections(); i++)
	{
		byteswapSection(sections[i]);
	}
	entryPoint = header->e_entry;
}

const char *ElfReader::GetSectionName(int section) const
{
	if (sections[section].sh_type == SHT_NULL)
		return nullptr;

	int nameOffset = sections[section].sh_name;
	char *ptr = (char*)GetSectionDataPtr(header->e_shstrndx);

	if (ptr)
		return ptr + nameOffset;
	else
		return nullptr;
}

bool ElfReader::LoadInto(u32 vaddr)
{
	DEBUG_LOG(MASTER_LOG,"String section: %i", header->e_shstrndx);

//	sectionOffsets = new u32[GetNumSections()];
//	sectionAddrs = new u32[GetNumSections()];

	// Should we relocate?
	bRelocate = (header->e_type != ET_EXEC);

	if (bRelocate)
	{
		DEBUG_LOG(MASTER_LOG,"Relocatable module");
		entryPoint += vaddr;
	}
	else
	{
		DEBUG_LOG(MASTER_LOG,"Prerelocated executable");
	}

	INFO_LOG(MASTER_LOG,"%i segments:", header->e_phnum);

	// First pass : Get the bits into RAM
	u32 segmentVAddr[32];

	u32 baseAddress = bRelocate?vaddr:0;
	for (int i = 0; i < header->e_phnum; i++)
	{
		Elf32_Phdr *p = segments + i;

		INFO_LOG(MASTER_LOG, "Type: %i Vaddr: %08x Filesz: %i Memsz: %i ", p->p_type, p->p_vaddr, p->p_filesz, p->p_memsz);

		if (p->p_type == PT_LOAD)
		{
			segmentVAddr[i] = baseAddress + p->p_vaddr;
			u32 writeAddr = segmentVAddr[i];

			const u8 *src = GetSegmentPtr(i);
			u8 *dst = Memory::GetPointer(writeAddr);
			u32 srcSize = p->p_filesz;
			u32 dstSize = p->p_memsz;
			u32 *s = (u32*)src;
			u32 *d = (u32*)dst;
			for (int j = 0; j < (int)(srcSize + 3) / 4; j++)
			{
				*d++ = /*_byteswap_ulong*/(*s++);
			}
			if (srcSize < dstSize)
			{
				//memset(dst + srcSize, 0, dstSize-srcSize); //zero out bss
			}
			INFO_LOG(MASTER_LOG,"Loadable Segment Copied to %08x, size %08x", writeAddr, p->p_memsz);
		}
	}

	/*
	LOG(MASTER_LOG,"%i sections:", header->e_shnum);

	for (int i=0; i<GetNumSections(); i++)
	{
		Elf32_Shdr *s = &sections[i];
		const char *name = GetSectionName(i);

		u32 writeAddr = s->sh_addr + baseAddress;
		sectionOffsets[i] = writeAddr - vaddr;
		sectionAddrs[i] = writeAddr;

		if (s->sh_flags & SHF_ALLOC)
		{
			LOG(MASTER_LOG,"Data Section found: %s     Sitting at %08x, size %08x", name, writeAddr, s->sh_size);

		}
		else
		{
			LOG(MASTER_LOG,"NonData Section found: %s     Ignoring (size=%08x) (flags=%08x)", name, s->sh_size, s->sh_flags);
		}
	}
*/
	INFO_LOG(MASTER_LOG,"Done loading.");
	return true;
}

SectionID ElfReader::GetSectionByName(const char *name, int firstSection) const
{
	for (int i = firstSection; i < header->e_shnum; i++)
	{
		const char *secname = GetSectionName(i);

		if (secname != nullptr && strcmp(name, secname) == 0)
			return i;
	}
	return -1;
}

bool ElfReader::LoadSymbols()
{
	bool hasSymbols = false;
	SectionID sec = GetSectionByName(".symtab");
	if (sec != -1)
	{
		int stringSection = sections[sec].sh_link;
		const char *stringBase = (const char *)GetSectionDataPtr(stringSection);

		//We have a symbol table!
		Elf32_Sym *symtab = (Elf32_Sym *)(GetSectionDataPtr(sec));
		int numSymbols = sections[sec].sh_size / sizeof(Elf32_Sym);
		for (int sym = 0; sym < numSymbols; sym++)
		{
			int size = Common::swap32(symtab[sym].st_size);
			if (size == 0)
				continue;

			// int bind = symtab[sym].st_info >> 4;
			int type = symtab[sym].st_info & 0xF;
			int sectionIndex = Common::swap16(symtab[sym].st_shndx);
			int value = Common::swap32(symtab[sym].st_value);
			const char *name = stringBase + Common::swap32(symtab[sym].st_name);
			if (bRelocate)
				value += sectionAddrs[sectionIndex];

			int symtype = Symbol::SYMBOL_DATA;
			switch (type)
			{
			case STT_OBJECT:
				symtype = Symbol::SYMBOL_DATA; break;
			case STT_FUNC:
				symtype = Symbol::SYMBOL_FUNCTION; break;
			default:
				continue;
			}
			g_symbolDB.AddKnownSymbol(value, size, name, symtype);
			hasSymbols = true;
		}
	}
	g_symbolDB.Index();
	return hasSymbols;
}
