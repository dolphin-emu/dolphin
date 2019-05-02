// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Debugger/RSO.h"

#include <cstddef>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"

void RSOHeaderView::Load(u32 address)
{
  m_address = address;
  m_header.entry.next_entry = PowerPC::HostRead_U32(address);
  m_header.entry.prev_entry = PowerPC::HostRead_U32(address + 0x04);
  m_header.entry.section_count = PowerPC::HostRead_U32(address + 0x08);
  m_header.entry.section_table_offset = PowerPC::HostRead_U32(address + 0xC);
  m_header.entry.name_offset = PowerPC::HostRead_U32(address + 0x10);
  m_header.entry.name_size = PowerPC::HostRead_U32(address + 0x14);
  m_header.entry.version = PowerPC::HostRead_U32(address + 0x18);
  m_header.entry.bss_size = PowerPC::HostRead_U32(address + 0x1C);
  m_header.section_info.prolog_section_index = PowerPC::HostRead_U8(address + 0x20);
  m_header.section_info.epilog_section_index = PowerPC::HostRead_U8(address + 0x21);
  m_header.section_info.unresolved_section_index = PowerPC::HostRead_U8(address + 0x22);
  m_header.section_info.bss_section_index = PowerPC::HostRead_U8(address + 0x23);
  m_header.section_info.prolog_offset = PowerPC::HostRead_U32(address + 0x24);
  m_header.section_info.epilog_offset = PowerPC::HostRead_U32(address + 0x28);
  m_header.section_info.unresolved_offset = PowerPC::HostRead_U32(address + 0x2C);
  m_header.relocation_tables.internals_offset = PowerPC::HostRead_U32(address + 0x30);
  m_header.relocation_tables.internals_size = PowerPC::HostRead_U32(address + 0x34);
  m_header.relocation_tables.externals_offset = PowerPC::HostRead_U32(address + 0x38);
  m_header.relocation_tables.externals_size = PowerPC::HostRead_U32(address + 0x3C);
  m_header.symbol_tables.exports_offset = PowerPC::HostRead_U32(address + 0x40);
  m_header.symbol_tables.exports_size = PowerPC::HostRead_U32(address + 0x44);
  m_header.symbol_tables.exports_name_table = PowerPC::HostRead_U32(address + 0x48);
  m_header.symbol_tables.imports_offset = PowerPC::HostRead_U32(address + 0x4C);
  m_header.symbol_tables.imports_size = PowerPC::HostRead_U32(address + 0x50);
  m_header.symbol_tables.imports_name_table = PowerPC::HostRead_U32(address + 0x54);

  // Prevent an invalid name going wild
  if (m_header.entry.name_size < 0x100)
    m_name = PowerPC::HostGetString(m_header.entry.name_offset, m_header.entry.name_size);
  else
    m_name = PowerPC::HostGetString(m_header.entry.name_offset, 0x100);
}

u32 RSOHeaderView::GetNextEntry() const
{
  return m_header.entry.next_entry;
}

u32 RSOHeaderView::GetPrevEntry() const
{
  return m_header.entry.prev_entry;
}

u32 RSOHeaderView::GetSectionCount() const
{
  return m_header.entry.section_count;
}

u32 RSOHeaderView::GetSectionTableOffset() const
{
  return m_header.entry.section_table_offset;
}

const std::string& RSOHeaderView::GetName() const
{
  return m_name;
}

u32 RSOHeaderView::GetVersion() const
{
  return m_header.entry.version;
}

u32 RSOHeaderView::GetBssSectionSize() const
{
  return m_header.entry.bss_size;
}

u32 RSOHeaderView::GetPrologSectionIndex() const
{
  return m_header.section_info.prolog_section_index;
}

u32 RSOHeaderView::GetEpilogSectionIndex() const
{
  return m_header.section_info.epilog_section_index;
}

u32 RSOHeaderView::GetUnresolvedSectionIndex() const
{
  return m_header.section_info.unresolved_section_index;
}

u32 RSOHeaderView::GetBssSectionIndex() const
{
  return m_header.section_info.bss_section_index;
}

u32 RSOHeaderView::GetPrologSectionOffset() const
{
  return m_header.section_info.prolog_offset;
}

u32 RSOHeaderView::GetEpilogSectionOffset() const
{
  return m_header.section_info.epilog_offset;
}

u32 RSOHeaderView::GetUnresolvedSectionOffset() const
{
  return m_header.section_info.unresolved_offset;
}

u32 RSOHeaderView::GetInternalsOffset() const
{
  return m_header.relocation_tables.internals_offset;
}

u32 RSOHeaderView::GetInternalsSize() const
{
  return m_header.relocation_tables.internals_size;
}

u32 RSOHeaderView::GetExternalsOffset() const
{
  return m_header.relocation_tables.externals_offset;
}

u32 RSOHeaderView::GetExternalsSize() const
{
  return m_header.relocation_tables.externals_size;
}

u32 RSOHeaderView::GetExportsOffset() const
{
  return m_header.symbol_tables.exports_offset;
}

u32 RSOHeaderView::GetExportsSize() const
{
  return m_header.symbol_tables.exports_size;
}

u32 RSOHeaderView::GetExportsNameTable() const
{
  return m_header.symbol_tables.exports_name_table;
}

u32 RSOHeaderView::GetImportsOffset() const
{
  return m_header.symbol_tables.imports_offset;
}

u32 RSOHeaderView::GetImportsSize() const
{
  return m_header.symbol_tables.imports_size;
}

u32 RSOHeaderView::GetImportsNameTable() const
{
  return m_header.symbol_tables.imports_name_table;
}

void RSOSectionsView::Load(u32 address, std::size_t count)
{
  m_address = address;
  for (std::size_t i = 0; i < count; ++i)
  {
    RSOSection section;
    section.offset = PowerPC::HostRead_U32(address);
    section.size = PowerPC::HostRead_U32(address + 4);
    m_sections.emplace_back(std::move(section));
    address += sizeof(RSOSection);
  }
}

size_t RSOSectionsView::Count() const
{
  return m_sections.size();
}

const RSOSection& RSOSectionsView::GetSection(std::size_t index) const
{
  return m_sections.at(index);
}

const std::vector<RSOSection>& RSOSectionsView::GetSections() const
{
  return m_sections;
}

void RSOImportsView::Load(u32 address, std::size_t count)
{
  m_address = address;
  for (std::size_t i = 0; i < count; ++i)
  {
    RSOImport rso_import;
    rso_import.name_offset = PowerPC::HostRead_U32(address);
    rso_import.code_offset = PowerPC::HostRead_U32(address + 4);
    rso_import.entry_offset = PowerPC::HostRead_U32(address + 8);
    m_imports.emplace_back(std::move(rso_import));
    address += sizeof(RSOImport);
  }
}

std::size_t RSOImportsView::Count() const
{
  return m_imports.size();
}

const RSOImport& RSOImportsView::GetImport(std::size_t index) const
{
  return m_imports.at(index);
}

const std::vector<RSOImport>& RSOImportsView::GetImports() const
{
  return m_imports;
}

void RSOExportsView::Load(u32 address, std::size_t count)
{
  m_address = address;
  for (std::size_t i = 0; i < count; ++i)
  {
    RSOExport rso_export;
    rso_export.name_offset = PowerPC::HostRead_U32(address);
    rso_export.code_offset = PowerPC::HostRead_U32(address + 4);
    rso_export.section_index = PowerPC::HostRead_U32(address + 8);
    rso_export.hash = PowerPC::HostRead_U32(address + 12);
    m_exports.emplace_back(std::move(rso_export));
    address += sizeof(RSOExport);
  }
}

std::size_t RSOExportsView::Count() const
{
  return m_exports.size();
}

const RSOExport& RSOExportsView::GetExport(std::size_t index) const
{
  return m_exports.at(index);
}

const std::vector<RSOExport>& RSOExportsView::GetExports() const
{
  return m_exports;
}

void RSOInternalsView::Load(u32 address, std::size_t count)
{
  m_address = address;
  for (std::size_t i = 0; i < count; ++i)
  {
    RSOInternalsEntry entry;
    entry.r_offset = PowerPC::HostRead_U32(address);
    entry.r_info = PowerPC::HostRead_U32(address + 4);
    entry.r_addend = PowerPC::HostRead_U32(address + 8);
    m_entries.emplace_back(std::move(entry));
    address += sizeof(RSOInternalsEntry);
  }
}

std::size_t RSOInternalsView::Count() const
{
  return m_entries.size();
}

const RSOInternalsEntry& RSOInternalsView::GetEntry(std::size_t index) const
{
  return m_entries.at(index);
}

const std::vector<RSOInternalsEntry>& RSOInternalsView::GetEntries() const
{
  return m_entries;
}

void RSOExternalsView::Load(u32 address, std::size_t count)
{
  m_address = address;
  for (std::size_t i = 0; i < count; ++i)
  {
    RSOExternalsEntry entry;
    entry.r_offset = PowerPC::HostRead_U32(address);
    entry.r_info = PowerPC::HostRead_U32(address + 4);
    entry.r_addend = PowerPC::HostRead_U32(address + 8);
    m_entries.emplace_back(std::move(entry));
    address += sizeof(RSOExternalsEntry);
  }
}

std::size_t RSOExternalsView::Count() const
{
  return m_entries.size();
}

const RSOExternalsEntry& RSOExternalsView::GetEntry(std::size_t index) const
{
  return m_entries.at(index);
}

const std::vector<RSOExternalsEntry>& RSOExternalsView::GetEntries() const
{
  return m_entries;
}

void RSOView::LoadHeader(u32 address)
{
  m_address = address;
  m_header.Load(address);
}

void RSOView::LoadSections()
{
  m_sections.Load(m_header.GetSectionTableOffset(), m_header.GetSectionCount());
}

void RSOView::LoadImports()
{
  std::size_t size = m_header.GetImportsSize();
  if (size % sizeof(RSOImport) != 0)
    WARN_LOG(SYMBOLS, "RSO Imports Table has an incoherent size (%08zx)", size);
  m_imports.Load(m_header.GetImportsOffset(), size / sizeof(RSOImport));
}

void RSOView::LoadExports()
{
  std::size_t size = m_header.GetExportsSize();
  if (size % sizeof(RSOExport) != 0)
    WARN_LOG(SYMBOLS, "RSO Exports Table has an incoherent size (%08zx)", size);
  m_exports.Load(m_header.GetExportsOffset(), size / sizeof(RSOExport));
}

void RSOView::LoadInternals()
{
  std::size_t size = m_header.GetInternalsSize();
  if (size % sizeof(RSOInternalsEntry) != 0)
    WARN_LOG(SYMBOLS, "RSO Internals Relocation Table has an incoherent size (%08zx)", size);
  m_imports.Load(m_header.GetInternalsOffset(), size / sizeof(RSOInternalsEntry));
}

void RSOView::LoadExternals()
{
  std::size_t size = m_header.GetExternalsSize();
  if (size % sizeof(RSOExternalsEntry) != 0)
    WARN_LOG(SYMBOLS, "RSO Externals Relocation Table has an incoherent size (%08zx)", size);
  m_imports.Load(m_header.GetExternalsOffset(), size / sizeof(RSOExternalsEntry));
}

void RSOView::LoadAll(u32 address)
{
  LoadHeader(address);
  LoadSections();
  LoadImports();
  LoadExports();
  LoadInternals();
  LoadExternals();
}

void RSOView::Apply(PPCSymbolDB* symbol_db) const
{
  for (const RSOExport& rso_export : GetExports())
  {
    u32 address = GetExportAddress(rso_export);
    if (address != 0)
    {
      Common::Symbol* symbol = symbol_db->AddFunction(address);
      if (!symbol)
        symbol = symbol_db->GetSymbolFromAddr(address);

      const std::string export_name = GetExportName(rso_export);
      if (symbol)
      {
        // Function symbol
        symbol->Rename(export_name);
      }
      else
      {
        // Data symbol
        symbol_db->AddKnownSymbol(address, 0, export_name, Common::Symbol::Type::Data);
      }
    }
  }
  DEBUG_LOG(SYMBOLS, "RSO(%s): %zu symbols applied", GetName().c_str(), GetExportsCount());
}

u32 RSOView::GetNextEntry() const
{
  return m_header.GetNextEntry();
}

u32 RSOView::GetPrevEntry() const
{
  return m_header.GetPrevEntry();
}

u32 RSOView::GetVersion() const
{
  return m_header.GetVersion();
}

u32 RSOView::GetAddress() const
{
  return m_address;
}

std::size_t RSOView::GetSectionCount() const
{
  return m_sections.Count();
}

const RSOSection& RSOView::GetSection(std::size_t index) const
{
  return m_sections.GetSection(index);
}

const std::vector<RSOSection>& RSOView::GetSections() const
{
  return m_sections.GetSections();
}

std::size_t RSOView::GetImportsCount() const
{
  return m_imports.Count();
}

const RSOImport& RSOView::GetImport(std::size_t index) const
{
  return m_imports.GetImport(index);
}

std::string RSOView::GetImportName(const RSOImport& rso_import) const
{
  return PowerPC::HostGetString(m_header.GetImportsNameTable() + rso_import.name_offset);
}

const std::vector<RSOImport>& RSOView::GetImports() const
{
  return m_imports.GetImports();
}

std::size_t RSOView::GetExportsCount() const
{
  return m_exports.Count();
}

const RSOExport& RSOView::GetExport(std::size_t index) const
{
  return m_exports.GetExport(index);
}

std::string RSOView::GetExportName(const RSOExport& rso_export) const
{
  return PowerPC::HostGetString(m_header.GetExportsNameTable() + rso_export.name_offset);
}

u32 RSOView::GetExportAddress(const RSOExport& rso_export) const
{
  u32 address = 0;

  if (rso_export.section_index < GetSectionCount())
    address = GetSection(rso_export.section_index).offset + rso_export.code_offset;

  return address;
}

const std::vector<RSOExport>& RSOView::GetExports() const
{
  return m_exports.GetExports();
}

std::size_t RSOView::GetInternalsCount() const
{
  return m_internals.Count();
}

const RSOInternalsEntry& RSOView::GetInternalsEntry(std::size_t index) const
{
  return m_internals.GetEntry(index);
}

const std::vector<RSOInternalsEntry>& RSOView::GetInternals() const
{
  return m_internals.GetEntries();
}

std::size_t RSOView::GetExternalsCount() const
{
  return m_externals.Count();
}

const RSOExternalsEntry& RSOView::GetExternalsEntry(std::size_t index) const
{
  return m_externals.GetEntry(index);
}

const std::vector<RSOExternalsEntry>& RSOView::GetExternals() const
{
  return m_externals.GetEntries();
}

const std::string& RSOView::GetName() const
{
  return m_header.GetName();
}

std::string RSOView::GetName(const RSOImport& rso_import) const
{
  return GetImportName(rso_import);
}

std::string RSOView::GetName(const RSOExport& rso_export) const
{
  return GetExportName(rso_export);
}

u32 RSOView::GetProlog() const
{
  u32 section_index = m_header.GetPrologSectionIndex();
  if (section_index == 0)
    WARN_LOG(SYMBOLS, "RSO doesn't have a prolog function");
  else if (section_index >= m_sections.Count())
    WARN_LOG(SYMBOLS, "RSO prolog section index out of bound");
  else
    return GetSection(section_index).offset + m_header.GetPrologSectionOffset();
  return 0;
}

u32 RSOView::GetEpilog() const
{
  u32 section_index = m_header.GetEpilogSectionIndex();
  if (section_index == 0)
    WARN_LOG(SYMBOLS, "RSO doesn't have an epilog function");
  else if (section_index >= m_sections.Count())
    WARN_LOG(SYMBOLS, "RSO epilog section index out of bound");
  else
    return GetSection(section_index).offset + m_header.GetEpilogSectionOffset();
  return 0;
}

u32 RSOView::GetUnresolved() const
{
  u32 section_index = m_header.GetUnresolvedSectionIndex();
  if (section_index == 0)
    WARN_LOG(SYMBOLS, "RSO doesn't have a unresolved function");
  else if (section_index >= m_sections.Count())
    WARN_LOG(SYMBOLS, "RSO unresolved section index out of bound");
  else
    return GetSection(section_index).offset + m_header.GetUnresolvedSectionOffset();
  return 0;
}

bool RSOChainView::Load(u32 address)
{
  // Load node
  RSOView node;
  node.LoadHeader(address);
  DEBUG_LOG(SYMBOLS, "RSOChain node name: %s", node.GetName().c_str());
  m_chain.emplace_front(std::move(node));

  if (LoadNextChain(m_chain.front()) && LoadPrevChain(m_chain.front()))
  {
    for (RSOView& view : m_chain)
    {
      view.LoadSections();
      view.LoadExports();
      view.LoadImports();
      view.LoadExternals();
      view.LoadInternals();
    }
    return true;
  }
  return false;
}

void RSOChainView::Apply(PPCSymbolDB* symbol_db) const
{
  for (const RSOView& rso_view : m_chain)
    rso_view.Apply(symbol_db);
}

void RSOChainView::Clear()
{
  m_chain.clear();
}

const std::list<RSOView>& RSOChainView::GetChain() const
{
  return m_chain;
}

bool RSOChainView::LoadNextChain(const RSOView& view)
{
  u32 prev_address = view.GetAddress();
  u32 next_address = view.GetNextEntry();

  while (next_address != 0)
  {
    RSOView next_node;
    next_node.LoadHeader(next_address);

    if (prev_address != next_node.GetPrevEntry())
    {
      ERROR_LOG(SYMBOLS, "RSOChain has an incoherent previous link %08x != %08x in %s",
                prev_address, next_node.GetPrevEntry(), next_node.GetName().c_str());
      return false;
    }

    prev_address = next_address;
    next_address = next_node.GetNextEntry();
    m_chain.emplace_back(std::move(next_node));
  }

  return true;
}

bool RSOChainView::LoadPrevChain(const RSOView& view)
{
  u32 prev_address = view.GetPrevEntry();
  u32 next_address = view.GetAddress();

  while (prev_address != 0)
  {
    RSOView prev_node;
    prev_node.LoadHeader(prev_address);

    if (next_address != prev_node.GetNextEntry())
    {
      ERROR_LOG(SYMBOLS, "RSOChain has an incoherent next link %08x != %08x in %s", next_address,
                prev_node.GetNextEntry(), prev_node.GetName().c_str());
      return false;
    }

    next_address = prev_address;
    prev_address = prev_node.GetPrevEntry();
    m_chain.emplace_front(std::move(prev_node));
  }

  return true;
}
