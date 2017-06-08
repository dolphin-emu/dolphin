// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <locale>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Boot/DolReader.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DirectoryBlob.h"

namespace DiscIO
{
static u32 ComputeNameSize(const File::FSTEntry& parent_entry);
static std::string ASCIIToUppercase(std::string str);
static void ConvertUTF8NamesToSHIFTJIS(File::FSTEntry& parent_entry);

constexpr u64 GAME_PARTITION_ADDRESS = 0x50000;
constexpr u64 PARTITION_TABLE_ADDRESS = 0x40000;
const std::array<u32, 10> PARTITION_TABLE = {
    {Common::swap32(1), Common::swap32((PARTITION_TABLE_ADDRESS + 0x20) >> 2), 0, 0, 0, 0, 0, 0,
     Common::swap32(GAME_PARTITION_ADDRESS >> 2), 0}};

const size_t DirectoryBlobReader::MAX_NAME_LENGTH;
const size_t DirectoryBlobReader::MAX_ID_LENGTH;

bool DirectoryBlobReader::IsValidDirectoryBlob(std::string dol_path)
{
#ifdef _WIN32
  std::replace(dol_path.begin(), dol_path.end(), '\\', '/');
#endif
  return StringEndsWith(dol_path, "/sys/main.dol");
}

std::unique_ptr<DirectoryBlobReader> DirectoryBlobReader::Create(File::IOFile dol,
                                                                 const std::string& dol_path)
{
  if (!dol || !IsValidDirectoryBlob(dol_path))
    return nullptr;

  const size_t chars_to_remove = std::string("sys/main.dol").size();
  const std::string root_directory = dol_path.substr(0, dol_path.size() - chars_to_remove);
  return std::unique_ptr<DirectoryBlobReader>(
      new DirectoryBlobReader(std::move(dol), root_directory));
}

DirectoryBlobReader::DirectoryBlobReader(File::IOFile dol_file, const std::string& root_directory)
    : m_root_directory(root_directory), m_data_start_address(UINT64_MAX),
      m_disk_header(DISKHEADERINFO_ADDRESS),
      m_disk_header_info(std::make_unique<SDiskHeaderInfo>()), m_fst_address(0), m_dol_address(0)
{
  // create the default disk header
  SetGameID("AGBJ01");
  SetName("Default name");

  // Setting the DOL relies on m_dol_address, which is set by SetApploader
  if (SetApploader(m_root_directory + "sys/apploader.img"))
    SetDOLAndDiskType(std::move(dol_file));

  BuildFST();

  m_virtual_disc.emplace(DISKHEADER_ADDRESS, DISKHEADERINFO_ADDRESS, m_disk_header.data());
  m_virtual_disc.emplace(DISKHEADERINFO_ADDRESS, sizeof(m_disk_header_info),
                         reinterpret_cast<const u8*>(m_disk_header_info.get()));
  m_virtual_disc.emplace(APPLOADER_ADDRESS, m_apploader.size(), m_apploader.data());
  m_virtual_disc.emplace(m_dol_address, m_dol.size(), m_dol.data());
  m_virtual_disc.emplace(m_fst_address, m_fst_data.size(), m_fst_data.data());

  if (m_is_wii)
  {
    m_nonpartition_contents.emplace(DISKHEADER_ADDRESS, DISKHEADERINFO_ADDRESS,
                                    m_disk_header.data());
    m_nonpartition_contents.emplace(PARTITION_TABLE_ADDRESS, PARTITION_TABLE.size() * sizeof(u32),
                                    reinterpret_cast<const u8*>(PARTITION_TABLE.data()));

    constexpr u32 TICKET_OFFSET = 0x0;
    constexpr u32 TICKET_SIZE = 0x2a4;
    constexpr u32 TMD_OFFSET = 0x2c0;
    AddFileToContents(&m_nonpartition_contents, m_root_directory + "ticket.bin",
                      GAME_PARTITION_ADDRESS + TICKET_OFFSET, TICKET_SIZE);
    const DiscContent& tmd =
        AddFileToContents(&m_nonpartition_contents, m_root_directory + "tmd.bin",
                          GAME_PARTITION_ADDRESS + 0x2c0, 0x49e4);
    m_tmd_header = {Common::swap32(static_cast<u32>(tmd.GetSize())),
                    Common::swap32(TMD_OFFSET >> m_address_shift)};
    m_nonpartition_contents.emplace(GAME_PARTITION_ADDRESS + TICKET_SIZE,
                                    m_tmd_header.size() * sizeof(u32),
                                    reinterpret_cast<const u8*>(m_tmd_header.data()));

    // TODO: We don't handle raw access to the encrypted area of Wii discs correctly.
  }
}

bool DirectoryBlobReader::ReadInternal(u64 offset, u64 length, u8* buffer,
                                       const std::set<DiscContent>& contents)
{
  if (contents.empty())
    return true;

  // Determine which DiscContent the offset refers to
  std::set<DiscContent>::const_iterator it = contents.lower_bound(DiscContent(offset));
  if (it->GetOffset() > offset && it != contents.begin())
    --it;

  // zero fill to start of file data
  PadToAddress(it->GetOffset(), &offset, &length, &buffer);

  while (it != contents.end() && length > 0)
  {
    _dbg_assert_(DISCIO, it->GetOffset() <= offset);
    if (!it->Read(&offset, &length, &buffer))
      return false;

    ++it;

    if (it != contents.end())
    {
      _dbg_assert_(DISCIO, it->GetOffset() >= offset);
      PadToAddress(it->GetOffset(), &offset, &length, &buffer);
    }
  }

  return true;
}

bool DirectoryBlobReader::Read(u64 offset, u64 length, u8* buffer)
{
  return ReadInternal(offset, length, buffer, m_is_wii ? m_nonpartition_contents : m_virtual_disc);
}

bool DirectoryBlobReader::SupportsReadWiiDecrypted() const
{
  return m_is_wii;
}

bool DirectoryBlobReader::ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_offset)
{
  if (!m_is_wii || partition_offset != GAME_PARTITION_ADDRESS)
    return false;

  return ReadInternal(offset, size, buffer, m_virtual_disc);
}

void DirectoryBlobReader::SetGameID(const std::string& id)
{
  memcpy(m_disk_header.data(), id.c_str(), std::min(id.length(), MAX_ID_LENGTH));
}

void DirectoryBlobReader::SetName(const std::string& name)
{
  size_t length = std::min(name.length(), MAX_NAME_LENGTH);
  memcpy(&m_disk_header[0x20], name.c_str(), length);
  m_disk_header[length + 0x20] = 0;
}

BlobType DirectoryBlobReader::GetBlobType() const
{
  return BlobType::DIRECTORY;
}

u64 DirectoryBlobReader::GetRawSize() const
{
  // Not implemented
  return 0;
}

u64 DirectoryBlobReader::GetDataSize() const
{
  // Not implemented
  return 0;
}

void DirectoryBlobReader::SetDiskTypeWii()
{
  Write32(0x5d1c9ea3, 0x18, &m_disk_header);
  memset(&m_disk_header[0x1c], 0, 4);

  m_is_wii = true;
  m_address_shift = 2;
}

void DirectoryBlobReader::SetDiskTypeGC()
{
  memset(&m_disk_header[0x18], 0, 4);
  Write32(0xc2339f3d, 0x1c, &m_disk_header);

  m_is_wii = false;
  m_address_shift = 0;
}

bool DirectoryBlobReader::SetApploader(const std::string& apploader)
{
  if (!apploader.empty())
  {
    std::string data;
    if (!File::ReadFileToString(apploader, data))
    {
      PanicAlertT("Apploader unable to load from file");
      return false;
    }
    size_t apploader_size = 0x20 + Common::swap32(*(u32*)&data.data()[0x14]) +
                            Common::swap32(*(u32*)&data.data()[0x18]);
    if (apploader_size != data.size())
    {
      PanicAlertT("Apploader is the wrong size...is it really an apploader?");
      return false;
    }
    m_apploader.resize(apploader_size);
    std::copy(data.begin(), data.end(), m_apploader.begin());

    // 32byte aligned (plus 0x20 padding)
    m_dol_address = Common::AlignUp(APPLOADER_ADDRESS + m_apploader.size() + 0x20, 0x20ull);
    return true;
  }
  else
  {
    m_apploader.resize(0x20);
    // Make sure BS2 HLE doesn't try to run the apploader
    *(u32*)&m_apploader[0x10] = (u32)-1;
    return false;
  }
}

void DirectoryBlobReader::SetDOLAndDiskType(File::IOFile dol_file)
{
  m_dol.resize(dol_file.GetSize());
  dol_file.Seek(0, SEEK_SET);
  dol_file.ReadBytes(m_dol.data(), m_dol.size());

  if (DolReader(std::move(dol_file)).IsWii())
    SetDiskTypeWii();
  else
    SetDiskTypeGC();

  Write32((u32)(m_dol_address >> m_address_shift), 0x0420, &m_disk_header);

  // 32byte aligned (plus 0x20 padding)
  m_fst_address = Common::AlignUp(m_dol_address + m_dol.size() + 0x20, 0x20ull);
}

void DirectoryBlobReader::BuildFST()
{
  m_fst_data.clear();

  File::FSTEntry rootEntry = File::ScanDirectoryTree(m_root_directory + "files/", true);

  ConvertUTF8NamesToSHIFTJIS(rootEntry);

  u32 name_table_size = Common::AlignUp(ComputeNameSize(rootEntry), 1ull << m_address_shift);
  u64 total_entries = rootEntry.size + 1;  // The root entry itself isn't counted in rootEntry.size

  m_fst_name_offset = total_entries * ENTRY_SIZE;  // offset of name table in FST
  m_fst_data.resize(m_fst_name_offset + name_table_size);

  // if FST hasn't been assigned (ie no apploader/dol setup), set to default
  if (m_fst_address == 0)
    m_fst_address = APPLOADER_ADDRESS + 0x2000;

  // 4 byte aligned start of data on disk
  m_data_start_address = Common::AlignUp(m_fst_address + m_fst_data.size(), 0x8000ull);
  u64 current_data_address = m_data_start_address;

  u32 fst_offset = 0;   // Offset within FST data
  u32 name_offset = 0;  // Offset within name table
  u32 root_offset = 0;  // Offset of root of FST

  // write root entry
  WriteEntryData(&fst_offset, DIRECTORY_ENTRY, 0, 0, total_entries, m_address_shift);

  WriteDirectory(rootEntry, &fst_offset, &name_offset, &current_data_address, root_offset);

  // overflow check, compare the aligned name offset with the aligned name table size
  _assert_(Common::AlignUp(name_offset, 1ull << m_address_shift) == name_table_size);

  // write FST size and location
  Write32((u32)(m_fst_address >> m_address_shift), 0x0424, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x0428, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x042c, &m_disk_header);
}

void DirectoryBlobReader::PadToAddress(u64 start_address, u64* address, u64* length,
                                       u8** buffer) const
{
  if (start_address > *address && *length > 0)
  {
    u64 padBytes = std::min(start_address - *address, *length);
    memset(*buffer, 0, (size_t)padBytes);
    *length -= padBytes;
    *buffer += padBytes;
    *address += padBytes;
  }
}

void DirectoryBlobReader::Write32(u32 data, u32 offset, std::vector<u8>* const buffer)
{
  (*buffer)[offset++] = (data >> 24);
  (*buffer)[offset++] = (data >> 16) & 0xff;
  (*buffer)[offset++] = (data >> 8) & 0xff;
  (*buffer)[offset] = (data)&0xff;
}

void DirectoryBlobReader::WriteEntryData(u32* entry_offset, u8 type, u32 name_offset,
                                         u64 data_offset, u64 length, u32 address_shift)
{
  m_fst_data[(*entry_offset)++] = type;

  m_fst_data[(*entry_offset)++] = (name_offset >> 16) & 0xff;
  m_fst_data[(*entry_offset)++] = (name_offset >> 8) & 0xff;
  m_fst_data[(*entry_offset)++] = (name_offset)&0xff;

  Write32((u32)(data_offset >> address_shift), *entry_offset, &m_fst_data);
  *entry_offset += 4;

  Write32((u32)length, *entry_offset, &m_fst_data);
  *entry_offset += 4;
}

void DirectoryBlobReader::WriteEntryName(u32* name_offset, const std::string& name)
{
  strncpy((char*)&m_fst_data[*name_offset + m_fst_name_offset], name.c_str(), name.length() + 1);

  *name_offset += (u32)(name.length() + 1);
}

void DirectoryBlobReader::WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset,
                                         u32* name_offset, u64* data_offset, u32 parent_entry_index)
{
  std::vector<File::FSTEntry> sorted_entries = parent_entry.children;

  // Sort for determinism
  std::sort(sorted_entries.begin(), sorted_entries.end(), [](const File::FSTEntry& one,
                                                             const File::FSTEntry& two) {
    const std::string one_upper = ASCIIToUppercase(one.virtualName);
    const std::string two_upper = ASCIIToUppercase(two.virtualName);
    return one_upper == two_upper ? one.virtualName < two.virtualName : one_upper < two_upper;
  });

  for (const File::FSTEntry& entry : sorted_entries)
  {
    if (entry.isDirectory)
    {
      u32 entry_index = *fst_offset / ENTRY_SIZE;
      WriteEntryData(fst_offset, DIRECTORY_ENTRY, *name_offset, parent_entry_index,
                     entry_index + entry.size + 1, 0);
      WriteEntryName(name_offset, entry.virtualName);

      WriteDirectory(entry, fst_offset, name_offset, data_offset, entry_index);
    }
    else
    {
      // put entry in FST
      WriteEntryData(fst_offset, FILE_ENTRY, *name_offset, *data_offset, entry.size,
                     m_address_shift);
      WriteEntryName(name_offset, entry.virtualName);

      // write entry to virtual disc
      auto result = m_virtual_disc.emplace(*data_offset, entry.size, entry.physicalName);
      _dbg_assert_(DISCIO, result.second);  // Check that this offset wasn't already occupied

      // 4 byte aligned
      *data_offset = Common::AlignUp(*data_offset + std::max<u64>(entry.size, 1ull), 0x8000ull);
    }
  }
}

const DirectoryBlobReader::DiscContent&
DirectoryBlobReader::AddFileToContents(std::set<DiscContent>* contents, const std::string& path,
                                       u64 offset, u64 max_size)
{
  return *(contents->emplace(offset, std::min(File::GetSize(path), max_size), path).first);
}

static u32 ComputeNameSize(const File::FSTEntry& parent_entry)
{
  u32 name_size = 0;
  for (const File::FSTEntry& entry : parent_entry.children)
  {
    if (entry.isDirectory)
      name_size += ComputeNameSize(entry);

    name_size += (u32)entry.virtualName.length() + 1;
  }
  return name_size;
}

static void ConvertUTF8NamesToSHIFTJIS(File::FSTEntry& parent_entry)
{
  for (File::FSTEntry& entry : parent_entry.children)
  {
    if (entry.isDirectory)
      ConvertUTF8NamesToSHIFTJIS(entry);

    entry.virtualName = UTF8ToSHIFTJIS(entry.virtualName);
  }
}

static std::string ASCIIToUppercase(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(),
                 [](char c) { return std::toupper(c, std::locale::classic()); });
  return str;
}

DirectoryBlobReader::DiscContent::DiscContent(u64 offset, u64 size, ContentSource content_source)
    : m_offset(offset), m_size(size), m_content_source(content_source)
{
}

DirectoryBlobReader::DiscContent::DiscContent(u64 offset) : m_offset(offset), m_size(0)
{
}

u64 DirectoryBlobReader::DiscContent::GetOffset() const
{
  return m_offset;
}

u64 DirectoryBlobReader::DiscContent::GetSize() const
{
  return m_size;
}

bool DirectoryBlobReader::DiscContent::Read(u64* offset, u64* length, u8** buffer) const
{
  if (m_size == 0)
    return true;

  _dbg_assert_(DISCIO, *offset >= m_offset);
  const u64 offset_in_content = *offset - m_offset;

  if (offset_in_content < m_size)
  {
    const u64 bytes_to_read = std::min(m_size - offset_in_content, *length);

    if (std::holds_alternative<std::string>(m_content_source))
    {
      File::IOFile file(std::get<std::string>(m_content_source), "rb");
      file.Seek(offset_in_content, SEEK_SET);
      if (!file.ReadBytes(*buffer, bytes_to_read))
        return false;
    }
    else
    {
      const u8* const content_pointer = std::get<const u8*>(m_content_source) + offset_in_content;
      std::copy(content_pointer, content_pointer + bytes_to_read, *buffer);
    }

    *length -= bytes_to_read;
    *buffer += bytes_to_read;
    *offset += bytes_to_read;
  }

  return true;
}

}  // namespace
