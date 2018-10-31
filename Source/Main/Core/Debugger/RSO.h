// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <list>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class PPCSymbolDB;

struct RSOEntry
{
  u32 next_entry;
  u32 prev_entry;
  u32 section_count;
  u32 section_table_offset;
  u32 name_offset;
  u32 name_size;
  u32 version;
  u32 bss_size;
};

struct RSOSectionInfo
{
  u8 prolog_section_index;
  u8 epilog_section_index;
  u8 unresolved_section_index;
  u8 bss_section_index;
  u32 prolog_offset;
  u32 epilog_offset;
  u32 unresolved_offset;
};

struct RSORelocationTables
{
  u32 internals_offset;
  u32 internals_size;
  u32 externals_offset;
  u32 externals_size;
};

struct RSOSymbolTables
{
  u32 exports_offset;
  u32 exports_size;
  u32 exports_name_table;
  u32 imports_offset;
  u32 imports_size;
  u32 imports_name_table;
};

struct RSOHeader
{
  RSOEntry entry;
  RSOSectionInfo section_info;
  RSORelocationTables relocation_tables;
  RSOSymbolTables symbol_tables;
};

struct RSOSection
{
  u32 offset;
  u32 size;
};

struct RSOImport
{
  u32 name_offset;
  u32 code_offset;
  u32 entry_offset;
};

struct RSOExport
{
  u32 name_offset;
  u32 code_offset;
  u32 section_index;
  u32 hash;
};

enum class RSORelocationTableType
{
  Internals,
  Externals
};

template <RSORelocationTableType Disambiguate>
struct RSORelocationTableEntry
{
  u32 r_offset;
  u32 r_info;
  u32 r_addend;
};

using RSOInternalsEntry = RSORelocationTableEntry<RSORelocationTableType::Internals>;
using RSOExternalsEntry = RSORelocationTableEntry<RSORelocationTableType::Externals>;

class RSOHeaderView
{
public:
  void Load(u32 address);

  u32 GetNextEntry() const;
  u32 GetPrevEntry() const;
  u32 GetSectionCount() const;
  u32 GetSectionTableOffset() const;
  const std::string& GetName() const;
  u32 GetVersion() const;
  u32 GetBssSectionSize() const;
  u32 GetPrologSectionIndex() const;
  u32 GetEpilogSectionIndex() const;
  u32 GetUnresolvedSectionIndex() const;
  u32 GetBssSectionIndex() const;
  u32 GetPrologSectionOffset() const;
  u32 GetEpilogSectionOffset() const;
  u32 GetUnresolvedSectionOffset() const;
  u32 GetInternalsOffset() const;
  u32 GetInternalsSize() const;
  u32 GetExternalsOffset() const;
  u32 GetExternalsSize() const;
  u32 GetExportsOffset() const;
  u32 GetExportsSize() const;
  u32 GetExportsNameTable() const;
  u32 GetImportsOffset() const;
  u32 GetImportsSize() const;
  u32 GetImportsNameTable() const;

private:
  RSOHeader m_header;
  std::string m_name;
  u32 m_address = 0;
};

class RSOSectionsView
{
public:
  void Load(u32 address, std::size_t count = 1);
  std::size_t Count() const;

  const RSOSection& GetSection(std::size_t index) const;
  const std::vector<RSOSection>& GetSections() const;

private:
  std::vector<RSOSection> m_sections;
  u32 m_address = 0;
};

class RSOImportsView
{
public:
  void Load(u32 address, std::size_t count = 1);
  std::size_t Count() const;

  const RSOImport& GetImport(std::size_t index) const;
  const std::vector<RSOImport>& GetImports() const;

private:
  std::vector<RSOImport> m_imports;
  u32 m_address = 0;
};

class RSOExportsView
{
public:
  void Load(u32 address, std::size_t count = 1);
  std::size_t Count() const;

  const RSOExport& GetExport(std::size_t index) const;
  const std::vector<RSOExport>& GetExports() const;

private:
  std::vector<RSOExport> m_exports;
  u32 m_address = 0;
};

class RSOInternalsView
{
public:
  void Load(u32 address, std::size_t count = 1);
  std::size_t Count() const;

  const RSOInternalsEntry& GetEntry(std::size_t index) const;
  const std::vector<RSOInternalsEntry>& GetEntries() const;

private:
  std::vector<RSOInternalsEntry> m_entries;
  u32 m_address = 0;
};

class RSOExternalsView
{
public:
  void Load(u32 address, std::size_t count = 1);
  std::size_t Count() const;

  const RSOExternalsEntry& GetEntry(std::size_t index) const;
  const std::vector<RSOExternalsEntry>& GetEntries() const;

private:
  std::vector<RSOExternalsEntry> m_entries;
  u32 m_address = 0;
};

class RSOView
{
public:
  void LoadHeader(u32 address);
  void LoadSections();
  void LoadImports();
  void LoadExports();
  void LoadInternals();
  void LoadExternals();
  void LoadAll(u32 address);

  void Apply(PPCSymbolDB* symbol_db) const;

  u32 GetNextEntry() const;
  u32 GetPrevEntry() const;
  u32 GetVersion() const;
  u32 GetAddress() const;

  std::size_t GetSectionCount() const;
  const RSOSection& GetSection(std::size_t index) const;
  const std::vector<RSOSection>& GetSections() const;

  std::size_t GetImportsCount() const;
  const RSOImport& GetImport(std::size_t index) const;
  std::string GetImportName(const RSOImport& rso_import) const;
  const std::vector<RSOImport>& GetImports() const;

  std::size_t GetExportsCount() const;
  const RSOExport& GetExport(std::size_t index) const;
  std::string GetExportName(const RSOExport& rso_export) const;
  u32 GetExportAddress(const RSOExport& rso_export) const;
  const std::vector<RSOExport>& GetExports() const;

  std::size_t GetInternalsCount() const;
  const RSOInternalsEntry& GetInternalsEntry(std::size_t index) const;
  const std::vector<RSOInternalsEntry>& GetInternals() const;

  std::size_t GetExternalsCount() const;
  const RSOExternalsEntry& GetExternalsEntry(std::size_t index) const;
  const std::vector<RSOExternalsEntry>& GetExternals() const;

  const std::string& GetName() const;
  std::string GetName(const RSOImport& rso_import) const;
  std::string GetName(const RSOExport& rso_export) const;

  u32 GetProlog() const;
  u32 GetEpilog() const;
  u32 GetUnresolved() const;

private:
  RSOHeaderView m_header;
  RSOSectionsView m_sections;
  RSOImportsView m_imports;
  RSOExportsView m_exports;
  RSOInternalsView m_internals;
  RSOExternalsView m_externals;
  u32 m_address = 0;
};

class RSOChainView
{
public:
  bool Load(u32 address);
  void Apply(PPCSymbolDB* symbol_db) const;
  void Clear();
  const std::list<RSOView>& GetChain() const;

private:
  bool LoadNextChain(const RSOView& view);
  bool LoadPrevChain(const RSOView& view);

  std::list<RSOView> m_chain;
};
