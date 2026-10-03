// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Core/Boot/Boot.h"
#include "Core/Boot/ElfTypes.h"

namespace File
{
class IOFile;
}

enum KnownElfTypes
{
  KNOWNELF_PSP = 0,
  KNOWNELF_DS = 1,
  KNOWNELF_GBA = 2,
  KNOWNELF_GC = 3,
};

typedef int SectionID;

class ElfReader final : public BootExecutableReader
{
public:
  explicit ElfReader(const std::string& filename);
  explicit ElfReader(File::IOFile file);
  explicit ElfReader(std::vector<u8> buffer);
  ~ElfReader() override;
  u32 Read32(int off) const { return base32[off >> 2]; }
  // Quick accessors
  ElfType GetType() const { return (ElfType)(header->e_type); }
  ElfMachine GetMachine() const { return (ElfMachine)(header->e_machine); }
  u32 GetEntryPoint() const override { return entryPoint; }
  u32 GetFlags() const { return (u32)(header->e_flags); }
  bool LoadIntoMemory(Core::System& system, bool only_in_mem1 = false) const override;
  bool LoadSymbols(const Core::CPUThreadGuard& guard, PPCSymbolDB& ppc_symbol_db,
                   const std::string& filename) const override;
  bool IsValid() const override { return m_is_valid; }
  bool IsWii() const override;

  int GetNumSegments() const { return (int)(header->e_phnum); }
  int GetNumSections() const { return (int)(header->e_shnum); }
  const u8* GetPtr(int offset) const { return (u8*)base + offset; }
  const char* GetSectionName(int section) const;
  const u8* GetSectionDataPtr(int section) const
  {
    if (section < 0 || section >= header->e_shnum)
      return nullptr;
    if (sections[section].sh_type != SHT_NOBITS)
      return GetPtr(sections[section].sh_offset);
    else
      return nullptr;
  }
  bool IsCodeSegment(int segment) const { return segments[segment].p_flags & PF_X; }
  const u8* GetSegmentPtr(int segment) const { return GetPtr(segments[segment].p_offset); }
  int GetSegmentSize(int segment) const { return segments[segment].p_filesz; }
  u32 GetSectionAddr(SectionID section) const { return sectionAddrs[section]; }
  int GetSectionSize(SectionID section) const { return sections[section].sh_size; }
  SectionID GetSectionByName(const char* name, int firstSection = 0) const;  //-1 for not found

  bool DidRelocate() const { return bRelocate; }

private:
  bool Initialize();

  char* base = nullptr;
  u32* base32 = nullptr;

  Elf32_Ehdr* header = nullptr;
  Elf32_Phdr* segments = nullptr;
  Elf32_Shdr* sections = nullptr;

  u32* sectionAddrs = nullptr;
  bool bRelocate = false;
  u32 entryPoint = 0;
  bool m_is_valid = false;
};
