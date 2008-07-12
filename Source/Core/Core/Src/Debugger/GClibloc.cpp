//__________________________________________________________________________________________________
//
// GClibloc.c V1.0 by Costis!
// (CONFIDENTIAL VERSION)
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../HLE/HLE.h"
#include "../Host.h"
#include "GCELF.h"
#include "GCLibLoc.h"
#include "../HW/Memmap.h"
#include "Debugger_SymbolMap.h"


#define gELFTypeS(a) (a == ET_NONE ? "NONE" : \
					   a == ET_REL ? "RELOCATABLE" : \
					   a == ET_EXEC ? "EXECUTABLE" : \
					   a == ET_DYN ? "SHARED OBJECT" : \
					   a == ET_CORE ? "CORE" : \
					   a == ET_LOPROC ? "PROCESSOR SPECIFIC: LOPROC" : \
					   a == ET_HIPROC ? "PROCESSOR SPECIFIC: HIPROC" : "INVALID")

inline unsigned int bswap (unsigned int a)
{
	return Common::swap32(a);
}

void readSectionHeader (FILE *ifil, int section, ELF_Header ELF_H, Section_Header *ELF_SH)
{
	fseek (ifil, ELF_H.e_shoff + ELF_H.e_shentsize * section, SEEK_SET);
	fread (ELF_SH, 1, sizeof (Section_Header), ifil);

	// Convert the GameCube (Big Endian) format to Little Endian 
	ELF_SH->name = bswap (ELF_SH->name);
	ELF_SH->type = bswap (ELF_SH->type);
	ELF_SH->flags = bswap (ELF_SH->flags);
	ELF_SH->addr = bswap (ELF_SH->addr);
	ELF_SH->offset = bswap (ELF_SH->offset);
	ELF_SH->size = bswap (ELF_SH->size);
	ELF_SH->link = bswap (ELF_SH->link);
	ELF_SH->info = bswap (ELF_SH->info);
	ELF_SH->addralign = bswap (ELF_SH->addralign);
	ELF_SH->entsize = bswap (ELF_SH->entsize);
}

void readProgramHeader (FILE *ifil, int psection, ELF_Header ELF_H, Program_Header *ELF_PH)
{
	fseek (ifil, ELF_H.e_phoff + ELF_H.e_phentsize * psection, SEEK_SET);
	fread (ELF_PH, 1, sizeof (Program_Header), ifil);

	// Convert the GameCube (Big Endian) format to Little Endian
	ELF_PH->type = bswap (ELF_PH->type);
	ELF_PH->offset = bswap (ELF_PH->offset);
	ELF_PH->vaddr = bswap (ELF_PH->vaddr);
	ELF_PH->paddr = bswap (ELF_PH->paddr);
	ELF_PH->filesz = bswap (ELF_PH->filesz);
	ELF_PH->memsz = bswap (ELF_PH->memsz);
	ELF_PH->flags = bswap (ELF_PH->flags);
	ELF_PH->align = bswap (ELF_PH->align);
}

unsigned int locShStrTab (FILE *ifil, ELF_Header ELF_H)
{
	int i;
	Section_Header ELF_SH;
	char stID[10];

	for (i = 1; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);
		if (ELF_SH.type == SHT_STRTAB)
		{
			fseek (ifil, ELF_SH.offset + ELF_SH.name, SEEK_SET);
			fread (stID, 1, 10, ifil);
			if (strcmp (stID, ".shstrtab") == 0)
				return ELF_SH.offset;
		}
	}
	return 0;
}
	

unsigned int locStrTab (FILE *ifil, ELF_Header ELF_H)
{
	int i;
	Section_Header ELF_SH;
	unsigned int ShStrTab;
	char stID[10];

	// Locate the Section String Table first.
	for (i = 1; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);
		if (ELF_SH.type == SHT_STRTAB)
		{
			fseek (ifil, ELF_SH.offset + ELF_SH.name, SEEK_SET);
			fread (stID, 1, 10, ifil);
			if (strcmp (stID, ".shstrtab") == 0)
				break;
		}
	}
	
	if (i >= ELF_H.e_shnum)
		return 0; // No Section String Table was located.

	ShStrTab = ELF_SH.offset;

	// Locate the String Table using the Section String Table.
	for (i = 1; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);
		if (ELF_SH.type == SHT_STRTAB)
		{
			fseek (ifil, ShStrTab + ELF_SH.name, SEEK_SET);
			fread (stID, 1, 8, ifil);
			if (strcmp (stID, ".strtab") == 0)
				return ELF_SH.offset;
		}
	}

	return 0; // No String Table was located.
}

unsigned int locStrTabShStrTab (FILE *ifil, unsigned int ShStrTab, ELF_Header ELF_H)
{
	int i;
	Section_Header ELF_SH;
	char stID[8];

	for (i = 1; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);
		if (ELF_SH.type == SHT_STRTAB)
		{
			fseek (ifil, ShStrTab + ELF_SH.name, SEEK_SET);
			fread (stID, 1, 8, ifil);
			if (strcmp (stID, ".strtab") == 0)
				return ELF_SH.offset;
		}
	}
	return 0;
}

unsigned int locSection (FILE *ifil, unsigned int ShType, ELF_Header ELF_H)
{
	int i;
	Section_Header ELF_SH;

	for (i = 1; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);
		if (ELF_SH.type == ShType)
			return i;
	}
	return 0;
}

unsigned int locCode (unsigned char *abuf, unsigned int fsize, unsigned char *bbuf, unsigned int offset, unsigned int size)
{
	unsigned int i, j;
	unsigned int b;

	i = 0;
	while (i < fsize)
	{
		
		if ((*(unsigned int*)(abuf + i)) == *(unsigned int*)(bbuf + offset))
		{
			// Code section possibly found.
			for (j = 4; j < size; j += 4)
			{
				b = (*(unsigned int*)(bbuf + offset + j));
				if ((*(unsigned int*)(abuf + i + j) != b) && (b != 0))
					break;
			}

			if (j >= size)
				return i; // The code section was found.
		}
		i += 4;
	}

	return 0;
}			


int LoadSymbolsFromO(const char* filename, unsigned int base, unsigned int count)
{
	unsigned int i, j;
	FILE *ifil;
	char astr[64];
	unsigned char *abuf, *bbuf;
	unsigned int ShStrTab, StrTab, SymTab;
	ELF_Header ELF_H;
	Section_Header ELF_SH;
	Symbol_Header ELF_SYM;
	Rela_Header ELF_RELA;
	
	ifil = fopen (filename, "rb");
    if (ifil == NULL) return 0;

	fread (&ELF_H, 1, sizeof (ELF_Header), ifil);
	ELF_H.e_type = (ELF_H.e_type >> 8) | (ELF_H.e_type << 8);
	ELF_H.e_machine = (ELF_H.e_machine >> 8) | (ELF_H.e_machine << 8);
	ELF_H.e_ehsize = (ELF_H.e_ehsize >> 8) | (ELF_H.e_ehsize << 8);
	ELF_H.e_phentsize = (ELF_H.e_phentsize >> 8) | (ELF_H.e_phentsize << 8);
	ELF_H.e_phnum = (ELF_H.e_phnum >> 8) | (ELF_H.e_phnum << 8);
	ELF_H.e_shentsize = (ELF_H.e_shentsize >> 8) | (ELF_H.e_shentsize << 8);
	ELF_H.e_shnum = (ELF_H.e_shnum >> 8) | (ELF_H.e_shnum << 8);
	ELF_H.e_shtrndx = (ELF_H.e_shtrndx >> 8) | (ELF_H.e_shtrndx << 8);
	ELF_H.e_version = bswap(ELF_H.e_version );
	ELF_H.e_entry   = bswap(ELF_H.e_entry   );
	ELF_H.e_phoff   = bswap(ELF_H.e_phoff   );
	ELF_H.e_shoff   = bswap(ELF_H.e_shoff   );
	ELF_H.e_flags   = bswap(ELF_H.e_flags   );

	if (ELF_H.e_machine != 20) 
	{
		printf ("This is not a GameCube ELF file!\n");
		return 0;
	}

	printf ("Identification ID: %c%c%c%c\n", ELF_H.ID[0], ELF_H.ID[1], ELF_H.ID[2], ELF_H.ID[3]);
	printf ("ELF Type: %d (%s)\n", ELF_H.e_type, gELFTypeS (ELF_H.e_type));
	printf ("ELF Machine: %d\n", ELF_H.e_machine);
	printf ("ELF Version: %d\n", ELF_H.e_version);

	ShStrTab = locShStrTab (ifil, ELF_H);
	fseek (ifil, ShStrTab + 1, SEEK_SET);
	fread (astr, 1, 32, ifil);

	for (i = 0; i < ELF_H.e_shnum; i++)
	{
		readSectionHeader (ifil, i, ELF_H, &ELF_SH);

		if (ELF_SH.type != 0)
		{
			fseek (ifil, ShStrTab + ELF_SH.name, SEEK_SET);
			j = 0;
			while (1)
				if ((astr[j++] = fgetc (ifil)) == 0) break;
		}
	}


	StrTab = locStrTab (ifil, ELF_H);

	SymTab = locSection (ifil, SHT_SYMTAB, ELF_H);
	readSectionHeader (ifil, SymTab, ELF_H, &ELF_SH);
	for (i = 1; i < ELF_SH.size / 16; i++)
	{
		fseek (ifil, ELF_SH.offset + 16 * i, SEEK_SET);
		fread (&ELF_SYM, 1, sizeof (Symbol_Header), ifil);
		ELF_SYM.name = bswap (ELF_SYM.name);
		ELF_SYM.value = bswap (ELF_SYM.value);
		ELF_SYM.size = bswap (ELF_SYM.size);
		ELF_SYM.shndx = (ELF_SYM.shndx >> 8) | (ELF_SYM.shndx << 8);

		fseek (ifil, StrTab + ELF_SYM.name, SEEK_SET);
		j = 0;
		while (1)
			if ((astr[j++] = fgetc (ifil)) == 0) break;
	}	

	readSectionHeader (ifil, 1, ELF_H, &ELF_SH);
	fseek (ifil, ELF_SH.offset, SEEK_SET);
	if (ELF_SH.size<0 || ELF_SH.size>0x10000000)
	{
		PanicAlert("WTF??");
	}
	abuf = (unsigned char *)malloc (ELF_SH.size);
	fread (abuf, 1, ELF_SH.size, ifil);

	//printf ("\nRelocatable Addend Header Information:\n\n");
	readSectionHeader (ifil, locSection (ifil, 4, ELF_H), ELF_H, &ELF_SH);
	
	if (ELF_SH.entsize==0) //corrupt file?
	{
		char temp[256];
		sprintf(temp,"Huh? ELF_SH.entsize==0 in %s!",filename);
		//MessageBox(GetForegroundWindow(),temp,"Hmm....",0);

		return 0;
	}
	else
	{
		unsigned int num = ELF_SH.size / ELF_SH.entsize;
		for (i = 0; i < num; i++)
		{
			fseek (ifil, ELF_SH.offset + 12 * i, SEEK_SET);
			fread (&ELF_RELA, 1, sizeof (Rela_Header), ifil);
			ELF_RELA.offset = bswap (ELF_RELA.offset);
			ELF_RELA.info = bswap (ELF_RELA.info);
			ELF_RELA.addend = bswap (ELF_RELA.addend);

			*(unsigned int*)(abuf + (ELF_RELA.offset & ~3)) = 0;
			if (ELF_SH.size == 0 || ELF_SH.entsize==0) //corrupt file?
			{
				char temp[256];
				sprintf(temp,"Huh? ELF_SH.entsize==0 in %s!",filename);
				//MessageBox(GetForegroundWindow(),temp,"Hmm....",0);

				return 0;
			}
		}

		bbuf = (unsigned char*)Memory::GetPointer(base);

		readSectionHeader (ifil, SymTab, ELF_H, &ELF_SH);
		for (i = 1; i < ELF_SH.size / 16; i++)
		{
			fseek (ifil, ELF_SH.offset + 16 * i, SEEK_SET);
			fread (&ELF_SYM, 1, sizeof (Symbol_Header), ifil);
			ELF_SYM.name = bswap (ELF_SYM.name);
			ELF_SYM.value = bswap (ELF_SYM.value);
			ELF_SYM.size = bswap (ELF_SYM.size);
			ELF_SYM.shndx = (ELF_SYM.shndx >> 8) | (ELF_SYM.shndx << 8);

			if (ELF_SYM.shndx == 1)
			{
				fseek (ifil, StrTab + ELF_SYM.name, SEEK_SET);
				j = 0;
				while (1)
                {
					if ((astr[j++] = fgetc (ifil)) == 0) 
                        break;
                }

				u32 offset = locCode (bbuf, count, abuf, ELF_SYM.value, ELF_SYM.size);
				if (offset != 0 && ELF_SYM.size>12)
				{
					Debugger::AddSymbol(Debugger::CSymbol(offset+base | 0x80000000, ELF_SYM.size, Debugger::ST_FUNCTION, astr));
				}
			}
		}	
		free (abuf);
		fclose (ifil);
	}
	return 0;
}



void Debugger_LoadSymbolTable(const char* _szFilename)
{
    Debugger::LoadSymbolMap(_szFilename);
	HLE::PatchFunctions();
	Host_NotifyMapLoaded();
}

void Debugger_SaveSymbolTable(const char* _szFilename)
{
	Debugger::SaveSymbolMap(_szFilename); 
}

void Debugger_LocateSymbolTables(const char* _szFilename)
{
	std::vector<std::string> files;

	std::string directory = _szFilename;
	const char *temp = _szFilename;
	const char *oldtemp = temp;
	temp += strlen(temp)+1;
	if (*temp==0)
	{
		//we only got one file
		files.push_back(std::string(oldtemp));
	}
	else
	{
		while (*temp)
		{
			files.push_back(directory + _T("\\") + _T(temp));
			temp+=strlen(temp)+1;
		}
	}

	while(!files.empty())
	{
		std::string strFilename = files.back();
		files.pop_back();

		LOG(MASTER_LOG,"Loading symbols from %s", strFilename.c_str());
		LoadSymbolsFromO(strFilename.c_str(), 0x80000000, 24*1024*1024);		
	}

	HLE::PatchFunctions();
}


void Debugger_ResetSymbolTables()
{
	// this shouldn't work, because we have to de-patch HLE too and that is not possible
	// Well, de-patching hle would be possible if we saved away the old code bytes :P
	// It's not like we use much HLE now anyway -ector
	Debugger::Reset();
	Host_NotifyMapLoaded();
}
