// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <locale>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeDirectory.h"

namespace DiscIO
{
static u32 ComputeNameSize(const File::FSTEntry& parent_entry);
static std::string ASCIIToLowercase(std::string str);

const size_t CVolumeDirectory::MAX_NAME_LENGTH;
const size_t CVolumeDirectory::MAX_ID_LENGTH;

CVolumeDirectory::CVolumeDirectory(const std::string& directory, bool is_wii,
                                   const std::string& apploader, const std::string& dol)
    : m_data_start_address(-1), m_disk_header(DISKHEADERINFO_ADDRESS),
      m_disk_header_info(std::make_unique<SDiskHeaderInfo>()), m_fst_address(0), m_dol_address(0)
{
  m_root_directory = ExtractDirectoryName(directory);

  // create the default disk header
  SetGameID("AGBJ01");
  SetName("Default name");

  if (is_wii)
    SetDiskTypeWii();
  else
    SetDiskTypeGC();

  // Don't load the DOL if we don't have an apploader
  if (SetApploader(apploader))
    SetDOL(dol);

  BuildFST();
}

CVolumeDirectory::~CVolumeDirectory()
{
}

bool CVolumeDirectory::IsValidDirectory(const std::string& directory)
{
  return File::IsDirectory(ExtractDirectoryName(directory));
}

bool CVolumeDirectory::Read(u64 offset, u64 length, u8* buffer, bool decrypt) const
{
  if (!decrypt && (offset + length >= 0x400) && m_is_wii)
  {
    // Fully supporting this would require re-encrypting every file that's read.
    // Only supporting the areas that IOS allows software to read could be more feasible.
    // Currently, only the header (up to 0x400) is supported, though we're cheating a bit
    // with it by reading the header inside the current partition instead. Supporting the
    // header is enough for booting games, but not for running things like the Disc Channel.
    return false;
  }

  if (decrypt && !m_is_wii)
    PanicAlertT("Tried to decrypt data from a non-Wii volume");

  // header
  if (offset < DISKHEADERINFO_ADDRESS)
  {
    WriteToBuffer(DISKHEADER_ADDRESS, DISKHEADERINFO_ADDRESS, m_disk_header.data(), &offset,
                  &length, &buffer);
  }
  // header info
  if (offset >= DISKHEADERINFO_ADDRESS && offset < APPLOADER_ADDRESS)
  {
    WriteToBuffer(DISKHEADERINFO_ADDRESS, sizeof(m_disk_header_info), (u8*)m_disk_header_info.get(),
                  &offset, &length, &buffer);
  }
  // apploader
  if (offset >= APPLOADER_ADDRESS && offset < APPLOADER_ADDRESS + m_apploader.size())
  {
    WriteToBuffer(APPLOADER_ADDRESS, m_apploader.size(), m_apploader.data(), &offset, &length,
                  &buffer);
  }
  // dol
  if (offset >= m_dol_address && offset < m_dol_address + m_dol.size())
  {
    WriteToBuffer(m_dol_address, m_dol.size(), m_dol.data(), &offset, &length, &buffer);
  }
  // fst
  if (offset >= m_fst_address && offset < m_data_start_address)
  {
    WriteToBuffer(m_fst_address, m_fst_data.size(), m_fst_data.data(), &offset, &length, &buffer);
  }

  if (m_virtual_disk.empty())
    return true;

  // Determine which file the offset refers to
  std::map<u64, std::string>::const_iterator fileIter = m_virtual_disk.lower_bound(offset);
  if (fileIter->first > offset && fileIter != m_virtual_disk.begin())
    --fileIter;

  // zero fill to start of file data
  PadToAddress(fileIter->first, &offset, &length, &buffer);

  while (fileIter != m_virtual_disk.end() && length > 0)
  {
    _dbg_assert_(DVDINTERFACE, fileIter->first <= offset);
    u64 fileOffset = offset - fileIter->first;
    const std::string fileName = fileIter->second;

    File::IOFile file(fileName, "rb");
    if (!file)
      return false;

    u64 fileSize = file.GetSize();

    if (fileOffset < fileSize)
    {
      u64 fileBytes = std::min(fileSize - fileOffset, length);

      if (!file.Seek(fileOffset, SEEK_SET))
        return false;
      if (!file.ReadBytes(buffer, fileBytes))
        return false;

      length -= fileBytes;
      buffer += fileBytes;
      offset += fileBytes;
    }

    ++fileIter;

    if (fileIter != m_virtual_disk.end())
    {
      _dbg_assert_(DVDINTERFACE, fileIter->first >= offset);
      PadToAddress(fileIter->first, &offset, &length, &buffer);
    }
  }

  return true;
}

std::string CVolumeDirectory::GetGameID() const
{
  return std::string(m_disk_header.begin(), m_disk_header.begin() + MAX_ID_LENGTH);
}

void CVolumeDirectory::SetGameID(const std::string& id)
{
  memcpy(m_disk_header.data(), id.c_str(), std::min(id.length(), MAX_ID_LENGTH));
}

Region CVolumeDirectory::GetRegion() const
{
  if (m_is_wii)
    return RegionSwitchWii(m_disk_header[3]);

  return RegionSwitchGC(m_disk_header[3]);
}

Country CVolumeDirectory::GetCountry() const
{
  return CountrySwitch(m_disk_header[3]);
}

std::string CVolumeDirectory::GetMakerID() const
{
  // Not implemented
  return "00";
}

std::string CVolumeDirectory::GetInternalName() const
{
  char name[0x60];
  if (Read(0x20, 0x60, (u8*)name, false))
    return DecodeString(name);
  else
    return "";
}

std::map<Language, std::string> CVolumeDirectory::GetLongNames() const
{
  std::string name = GetInternalName();
  if (name.empty())
    return {{}};
  return {{Language::LANGUAGE_UNKNOWN, name}};
}

std::vector<u32> CVolumeDirectory::GetBanner(int* width, int* height) const
{
  // Not implemented
  *width = 0;
  *height = 0;
  return std::vector<u32>();
}

void CVolumeDirectory::SetName(const std::string& name)
{
  size_t length = std::min(name.length(), MAX_NAME_LENGTH);
  memcpy(&m_disk_header[0x20], name.c_str(), length);
  m_disk_header[length + 0x20] = 0;
}

u64 CVolumeDirectory::GetFSTSize() const
{
  // Not implemented
  return 0;
}

std::string CVolumeDirectory::GetApploaderDate() const
{
  // Not implemented
  return "VOID";
}

Platform CVolumeDirectory::GetVolumeType() const
{
  return m_is_wii ? Platform::WII_DISC : Platform::GAMECUBE_DISC;
}

BlobType CVolumeDirectory::GetBlobType() const
{
  // VolumeDirectory isn't actually a blob, but it sort of acts
  // like one, so it makes sense that it has its own blob type.
  // It should be made into a proper blob in the future.
  return BlobType::DIRECTORY;
}

u64 CVolumeDirectory::GetSize() const
{
  // Not implemented
  return 0;
}

u64 CVolumeDirectory::GetRawSize() const
{
  // Not implemented
  return 0;
}

std::string CVolumeDirectory::ExtractDirectoryName(const std::string& directory)
{
  std::string result = directory;

  size_t last_separator = result.find_last_of(DIR_SEP_CHR);

  if (last_separator != result.size() - 1)
  {
    // TODO: This assumes that file names will always have a dot in them
    //       and directory names never will; both assumptions are often
    //       right but in general wrong.
    size_t extension_start = result.find_last_of('.');
    if (extension_start != std::string::npos && extension_start > last_separator)
    {
      result.resize(last_separator);
    }
  }
  else
  {
    result.resize(last_separator);
  }

  return result;
}

void CVolumeDirectory::SetDiskTypeWii()
{
  Write32(0x5d1c9ea3, 0x18, &m_disk_header);
  memset(&m_disk_header[0x1c], 0, 4);

  m_is_wii = true;
  m_address_shift = 2;
}

void CVolumeDirectory::SetDiskTypeGC()
{
  memset(&m_disk_header[0x18], 0, 4);
  Write32(0xc2339f3d, 0x1c, &m_disk_header);

  m_is_wii = false;
  m_address_shift = 0;
}

bool CVolumeDirectory::SetApploader(const std::string& apploader)
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

void CVolumeDirectory::SetDOL(const std::string& dol)
{
  if (!dol.empty())
  {
    std::string data;
    File::ReadFileToString(dol, data);
    m_dol.resize(data.size());
    std::copy(data.begin(), data.end(), m_dol.begin());

    Write32((u32)(m_dol_address >> m_address_shift), 0x0420, &m_disk_header);

    // 32byte aligned (plus 0x20 padding)
    m_fst_address = Common::AlignUp(m_dol_address + m_dol.size() + 0x20, 0x20ull);
  }
}

void CVolumeDirectory::BuildFST()
{
  m_fst_data.clear();

  File::FSTEntry rootEntry = File::ScanDirectoryTree(m_root_directory, true);
  u32 name_table_size = ComputeNameSize(rootEntry);

  m_fst_name_offset = rootEntry.size * ENTRY_SIZE;  // offset of name table in FST
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
  WriteEntryData(&fst_offset, DIRECTORY_ENTRY, 0, 0, rootEntry.size);

  WriteDirectory(rootEntry, &fst_offset, &name_offset, &current_data_address, root_offset);

  // overflow check
  _dbg_assert_(DVDINTERFACE, name_offset == name_table_size);

  // write FST size and location
  Write32((u32)(m_fst_address >> m_address_shift), 0x0424, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x0428, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x042c, &m_disk_header);
}

void CVolumeDirectory::WriteToBuffer(u64 source_start_address, u64 source_length, const u8* source,
                                     u64* address, u64* length, u8** buffer) const
{
  if (*length == 0)
    return;

  _dbg_assert_(DVDINTERFACE, *address >= source_start_address);

  u64 source_offset = *address - source_start_address;

  if (source_offset < source_length)
  {
    size_t bytes_to_read = std::min(source_length - source_offset, *length);

    memcpy(*buffer, source + source_offset, bytes_to_read);

    *length -= bytes_to_read;
    *buffer += bytes_to_read;
    *address += bytes_to_read;
  }
}

void CVolumeDirectory::PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer) const
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

void CVolumeDirectory::Write32(u32 data, u32 offset, std::vector<u8>* const buffer)
{
  (*buffer)[offset++] = (data >> 24);
  (*buffer)[offset++] = (data >> 16) & 0xff;
  (*buffer)[offset++] = (data >> 8) & 0xff;
  (*buffer)[offset] = (data)&0xff;
}

void CVolumeDirectory::WriteEntryData(u32* entry_offset, u8 type, u32 name_offset, u64 data_offset,
                                      u64 length)
{
  m_fst_data[(*entry_offset)++] = type;

  m_fst_data[(*entry_offset)++] = (name_offset >> 16) & 0xff;
  m_fst_data[(*entry_offset)++] = (name_offset >> 8) & 0xff;
  m_fst_data[(*entry_offset)++] = (name_offset)&0xff;

  Write32((u32)(data_offset >> m_address_shift), *entry_offset, &m_fst_data);
  *entry_offset += 4;

  Write32((u32)length, *entry_offset, &m_fst_data);
  *entry_offset += 4;
}

void CVolumeDirectory::WriteEntryName(u32* name_offset, const std::string& name)
{
  strncpy((char*)&m_fst_data[*name_offset + m_fst_name_offset], name.c_str(), name.length() + 1);

  *name_offset += (u32)(name.length() + 1);
}

void CVolumeDirectory::WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset,
                                      u32* name_offset, u64* data_offset, u32 parent_entry_index)
{
  std::vector<File::FSTEntry> sorted_entries = parent_entry.children;

  // Sort for determinism
  std::sort(sorted_entries.begin(), sorted_entries.end(), [](const File::FSTEntry& one,
                                                             const File::FSTEntry& two) {
    // For some reason, sorting by lowest ASCII value first prevents many games from
    // fully booting. We make the comparison case insensitive to solve the problem.
    // (Highest ASCII value first seems to work regardless of case sensitivity.)
    const std::string one_lower = ASCIIToLowercase(one.virtualName);
    const std::string two_lower = ASCIIToLowercase(two.virtualName);
    return one_lower == two_lower ? one.virtualName < two.virtualName : one_lower < two_lower;
  });

  for (const File::FSTEntry& entry : sorted_entries)
  {
    if (entry.isDirectory)
    {
      u32 entry_index = *fst_offset / ENTRY_SIZE;
      WriteEntryData(fst_offset, DIRECTORY_ENTRY, *name_offset, parent_entry_index,
                     entry_index + entry.size + 1);
      WriteEntryName(name_offset, entry.virtualName);

      WriteDirectory(entry, fst_offset, name_offset, data_offset, entry_index);
    }
    else
    {
      // put entry in FST
      WriteEntryData(fst_offset, FILE_ENTRY, *name_offset, *data_offset, entry.size);
      WriteEntryName(name_offset, entry.virtualName);

      // write entry to virtual disk
      _dbg_assert_(DVDINTERFACE, m_virtual_disk.find(*data_offset) == m_virtual_disk.end());
      m_virtual_disk.emplace(*data_offset, entry.physicalName);

      // 4 byte aligned
      *data_offset = Common::AlignUp(*data_offset + std::max<u64>(entry.size, 1ull), 0x8000ull);
    }
  }
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

static std::string ASCIIToLowercase(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(),
                 [](char c) { return std::tolower(c, std::locale::classic()); });
  return str;
}

}  // namespace
