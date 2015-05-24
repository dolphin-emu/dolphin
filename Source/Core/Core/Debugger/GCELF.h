// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// ELF File Types
#define ET_NONE         0 // No file type
#define ET_REL          1 // Relocatable file
#define ET_EXEC         2 // Executable file
#define ET_DYN          3 // Shared object file
#define ET_CORE         4 // Core file
#define ET_LOPROC       0xFF00 // Processor specific
#define ET_HIPROC       0xFFFF // Processor specific

// ELF Machine Types
#define EM_NONE         0 // No machine
#define EM_M32          1 // AT&T WE 32100
#define EM_SPARC        2 // SPARC
#define EM_386          3 // Intel Architecture
#define EM_68K          4 // Motorola 68000
#define EM_88K          5 // Motorola 88000
#define EM_860          6 // Intel 80860
#define EM_MIPS         7 // MIPS RS3000 Big-Endian
#define EM_MIPS_RS4_BE  8 // MIPS RS4000 Big-Endian
#define EM_ARM         40 // ARM/Thumb Architecture

// ELF Version Types
#define EV_NONE         0 // Invalid version
#define EV_CURRENT      1 // Current version

// ELF Section Header Types
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11


struct ELF_Header
{
	unsigned char ID[4];
	unsigned char clazz;
	unsigned char data;
	unsigned char version;
	unsigned char pad[9];
	unsigned short e_type; // ELF file type
	unsigned short e_machine; // ELF target machine
	unsigned int e_version; // ELF file version number
	unsigned int e_entry;
	unsigned int e_phoff;
	unsigned int e_shoff;
	unsigned int e_flags;
	unsigned short e_ehsize;
	unsigned short e_phentsize;
	unsigned short e_phnum;
	unsigned short e_shentsize;
	unsigned short e_shnum;
	unsigned short e_shtrndx;
};

struct Program_Header
{
	unsigned int type;
	unsigned int offset;
	unsigned int vaddr;
	unsigned int paddr;
	unsigned int filesz;
	unsigned int memsz;
	unsigned int flags;
	unsigned int align;
};

struct Section_Header
{
	unsigned int name;
	unsigned int type;
	unsigned int flags;
	unsigned int addr;
	unsigned int offset;
	unsigned int size;
	unsigned int link;
	unsigned int info;
	unsigned int addralign;
	unsigned int entsize;
};

struct Symbol_Header
{
	unsigned int name;
	unsigned int value;
	unsigned int size;
	unsigned char info;
	unsigned char other;
	unsigned short shndx;
};

struct Rela_Header
{
	unsigned int offset;
	unsigned int info;
	signed int addend;
};

const char ELFID[4] = {0x7F, 'E', 'L', 'F'};
