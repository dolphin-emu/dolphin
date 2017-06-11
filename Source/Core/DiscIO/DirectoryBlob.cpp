// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstring>
#include <locale>
#include <map>
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
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
static const DiscContent& AddFileToContents(std::set<DiscContent>* contents,
                                            const std::string& path, u64 offset,
                                            u64 max_size = UINT64_MAX);

// Reads as many bytes as the vector fits (or less, if the file is smaller).
// Returns the number of bytes read.
static size_t ReadFileToVector(const std::string& path, std::vector<u8>* vector);

static void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer);
static void Write32(u32 data, u32 offset, std::vector<u8>* buffer);

static u32 ComputeNameSize(const File::FSTEntry& parent_entry);
static std::string ASCIIToUppercase(std::string str);
static void ConvertUTF8NamesToSHIFTJIS(File::FSTEntry& parent_entry);

enum class PartitionType : u32
{
  Game = 0,
  Update = 1,
  Channel = 2,
  // There are more types used by Super Smash Bros. Brawl, but they don't have special names
};

constexpr u8 ENTRY_SIZE = 0x0c;
constexpr u8 FILE_ENTRY = 0;
constexpr u8 DIRECTORY_ENTRY = 1;

DiscContent::DiscContent(u64 offset, u64 size, const std::string& path)
    : m_offset(offset), m_size(size), m_content_source(path)
{
}

DiscContent::DiscContent(u64 offset, u64 size, const u8* data)
    : m_offset(offset), m_size(size), m_content_source(data)
{
}

DiscContent::DiscContent(u64 offset) : m_offset(offset)
{
}

u64 DiscContent::GetOffset() const
{
  return m_offset;
}

u64 DiscContent::GetSize() const
{
  return m_size;
}

bool DiscContent::Read(u64* offset, u64* length, u8** buffer) const
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

static std::optional<PartitionType> ParsePartitionDirectoryName(const std::string& name)
{
  if (name.size() < 2)
    return {};

  const std::string uppercase_name = ASCIIToUppercase(name);

  if (uppercase_name == "DATA")
    return PartitionType::Game;
  if (uppercase_name == "UPDATE")
    return PartitionType::Update;
  if (uppercase_name == "CHANNEL")
    return PartitionType::Channel;

  if (uppercase_name[0] == 'P')
  {
    // e.g. "P-HA8E" (normally only used for Super Smash Bros. Brawl's VC partitions)
    if (name[1] == '-' && name.size() == 6)
    {
      const u32 result = Common::swap32(reinterpret_cast<const u8*>(name.data() + 2));
      return static_cast<PartitionType>(result);
    }

    // e.g. "P0"
    if (std::all_of(name.cbegin() + 1, name.cend(), [](char c) { return c >= '0' && c <= '9'; }))
    {
      u32 result;
      if (TryParse(name.substr(1), &result))
        return static_cast<PartitionType>(result);
    }
  }

  return {};
}

static bool IsValidDirectoryBlob(const std::string& dol_path, std::string* partition_root,
                                 std::string* true_root)
{
#ifdef _WIN32
  std::string normalized_dol_path = dol_path;
  std::replace(normalized_dol_path.begin(), normalized_dol_path.end(), '\\', '/');
#else
  const std::string& normalized_dol_path = dol_path;
#endif
  if (!StringEndsWith(normalized_dol_path, "/sys/main.dol"))
    return false;

  const size_t chars_to_remove = std::string("sys/main.dol").size();
  *partition_root = dol_path.substr(0, dol_path.size() - chars_to_remove);

  if (File::GetSize(*partition_root + "sys/boot.bin") < 0x20)
    return false;

  *true_root = dol_path.substr(0, normalized_dol_path.rfind('/', partition_root->size() - 2) + 1);

  return true;
}

std::unique_ptr<DirectoryBlobReader> DirectoryBlobReader::Create(const std::string& dol_path)
{
  std::string partition_root, true_root;
  if (!IsValidDirectoryBlob(dol_path, &partition_root, &true_root))
    return nullptr;

  return std::unique_ptr<DirectoryBlobReader>(new DirectoryBlobReader(partition_root, true_root));
}

DirectoryBlobReader::DirectoryBlobReader(const std::string& game_partition_root,
                                         const std::string& true_root)
{
  DirectoryBlobPartition game_partition(game_partition_root, {});
  m_is_wii = game_partition.IsWii();

  if (!m_is_wii)
  {
    m_gamecube_pseudopartition = std::move(game_partition);
    m_data_size = m_gamecube_pseudopartition.GetDataSize();
  }
  else
  {
    SetNonpartitionDiscHeader(game_partition.GetHeader(), game_partition_root);
    SetWiiRegionData(game_partition_root);

    std::vector<PartitionWithType> partitions;
    partitions.emplace_back(std::move(game_partition), PartitionType::Game);

    std::string game_partition_directory_name = game_partition_root.substr(true_root.size());
    game_partition_directory_name.pop_back();  // Remove trailing slash
    if (ParsePartitionDirectoryName(game_partition_directory_name) == PartitionType::Game)
    {
      const File::FSTEntry true_root_entry = File::ScanDirectoryTree(true_root, false);
      for (const File::FSTEntry& entry : true_root_entry.children)
      {
        if (entry.isDirectory)
        {
          const std::optional<PartitionType> type = ParsePartitionDirectoryName(entry.virtualName);
          if (type && *type != PartitionType::Game)
          {
            partitions.emplace_back(DirectoryBlobPartition(entry.physicalName + "/", m_is_wii),
                                    *type);
          }
        }
      }
    }

    SetPartitions(std::move(partitions));
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
  // TODO: We don't handle raw access to the encrypted area of Wii discs correctly.

  const std::set<DiscContent>& contents =
      m_is_wii ? m_nonpartition_contents : m_gamecube_pseudopartition.GetContents();
  return ReadInternal(offset, length, buffer, contents);
}

bool DirectoryBlobReader::SupportsReadWiiDecrypted() const
{
  return m_is_wii;
}

bool DirectoryBlobReader::ReadWiiDecrypted(u64 offset, u64 size, u8* buffer, u64 partition_offset)
{
  if (!m_is_wii)
    return false;

  auto it = m_partitions.find(partition_offset);
  if (it == m_partitions.end())
    return false;

  return ReadInternal(offset, size, buffer, it->second.GetContents());
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
  return m_data_size;
}

void DirectoryBlobReader::SetNonpartitionDiscHeader(const std::vector<u8>& partition_header,
                                                    const std::string& game_partition_root)
{
  constexpr u64 NONPARTITION_DISKHEADER_ADDRESS = 0;
  constexpr u64 NONPARTITION_DISKHEADER_SIZE = 0x100;

  m_disk_header_nonpartition.resize(NONPARTITION_DISKHEADER_SIZE);
  const size_t header_bin_bytes_read =
      ReadFileToVector(game_partition_root + "disc/header.bin", &m_disk_header_nonpartition);

  // If header.bin is missing or smaller than expected, use the content of sys/boot.bin instead
  std::copy(partition_header.data() + header_bin_bytes_read,
            partition_header.data() + m_disk_header_nonpartition.size(),
            m_disk_header_nonpartition.data() + header_bin_bytes_read);

  // 0x60 and 0x61 are the only differences between the partition and non-partition headers
  if (header_bin_bytes_read < 0x60)
    m_disk_header_nonpartition[0x60] = 0;
  if (header_bin_bytes_read < 0x61)
    m_disk_header_nonpartition[0x61] = 0;

  m_nonpartition_contents.emplace(NONPARTITION_DISKHEADER_ADDRESS, NONPARTITION_DISKHEADER_SIZE,
                                  m_disk_header_nonpartition.data());
}

void DirectoryBlobReader::SetWiiRegionData(const std::string& game_partition_root)
{
  m_wii_region_data.resize(0x10, 0x00);
  m_wii_region_data.resize(0x20, 0x80);

  // 0xFF is an arbitrarily picked value. Note that we can't use 0x00, because that means NTSC-J
  constexpr u32 INVALID_REGION = 0xFF;
  Write32(INVALID_REGION, 0, &m_wii_region_data);

  const std::string region_bin_path = game_partition_root + "disc/region.bin";
  const size_t bytes_read = ReadFileToVector(region_bin_path, &m_wii_region_data);
  if (bytes_read < 0x4)
    ERROR_LOG(DISCIO, "Couldn't read region from %s", region_bin_path.c_str());
  else if (bytes_read < 0x20)
    ERROR_LOG(DISCIO, "Couldn't read age ratings from %s", region_bin_path.c_str());

  constexpr u64 WII_REGION_DATA_ADDRESS = 0x4E000;
  constexpr u64 WII_REGION_DATA_SIZE = 0x20;
  m_nonpartition_contents.emplace(WII_REGION_DATA_ADDRESS, WII_REGION_DATA_SIZE,
                                  m_wii_region_data.data());
}

void DirectoryBlobReader::SetPartitions(std::vector<PartitionWithType>&& partitions)
{
  std::sort(partitions.begin(), partitions.end(),
            [](const PartitionWithType& lhs, const PartitionWithType& rhs) {
              if (lhs.type == rhs.type)
                return lhs.partition.GetRootDirectory() < rhs.partition.GetRootDirectory();

              // Ascending sort by partition type, except Update (1) comes before before Game (0)
              return (lhs.type > PartitionType::Update || rhs.type > PartitionType::Update) ?
                         lhs.type < rhs.type :
                         lhs.type > rhs.type;
            });

  constexpr u32 PARTITION_TABLE_ADDRESS = 0x40000;
  constexpr u32 PARTITION_SUBTABLE_OFFSET = 0x20;
  m_partition_table.resize(PARTITION_SUBTABLE_OFFSET + partitions.size() * 8);
  Write32(static_cast<u32>(partitions.size()), 0x0, &m_partition_table);
  Write32((PARTITION_TABLE_ADDRESS + PARTITION_SUBTABLE_OFFSET) >> 2, 0x4, &m_partition_table);

  u64 partition_address = 0x50000;
  u64 offset_in_table = PARTITION_SUBTABLE_OFFSET;
  for (PartitionWithType& partition : partitions)
  {
    Write32(static_cast<u32>(partition_address >> 2), offset_in_table, &m_partition_table);
    offset_in_table += 4;
    Write32(static_cast<u32>(partition.type), offset_in_table, &m_partition_table);
    offset_in_table += 4;

    SetTMDAndTicket(partition.partition.GetRootDirectory(), partition_address);

    m_partitions.emplace(partition_address, std::move(partition.partition));
    const u64 unaligned_next_address = VolumeWii::PartitionOffsetToRawOffset(
        partition.partition.GetDataSize(), Partition(partition_address));
    partition_address = Common::AlignUp(unaligned_next_address, 0x8000ull);
  }
  m_data_size = partition_address;

  m_nonpartition_contents.emplace(PARTITION_TABLE_ADDRESS, m_partition_table.size(),
                                  m_partition_table.data());
}

void DirectoryBlobReader::SetTMDAndTicket(const std::string& partition_root, u64 partition_address)
{
  constexpr u32 TICKET_OFFSET = 0x0;
  constexpr u32 TICKET_SIZE = 0x2a4;
  constexpr u32 TMD_OFFSET = 0x2c0;
  constexpr u32 MAX_TMD_SIZE = 0x49e4;
  AddFileToContents(&m_nonpartition_contents, partition_root + "ticket.bin",
                    partition_address + TICKET_OFFSET, TICKET_SIZE);
  const DiscContent& tmd = AddFileToContents(&m_nonpartition_contents, partition_root + "tmd.bin",
                                             partition_address + TMD_OFFSET, MAX_TMD_SIZE);

  constexpr u32 TMD_HEADER_SIZE = 8;
  m_tmd_headers.emplace_back(TMD_HEADER_SIZE);
  std::vector<u8>& tmd_header = m_tmd_headers.back();
  Write32(static_cast<u32>(tmd.GetSize()), 0x0, &tmd_header);
  Write32(TMD_OFFSET >> 2, 0x4, &tmd_header);
  m_nonpartition_contents.emplace(partition_address + TICKET_SIZE, TMD_HEADER_SIZE,
                                  tmd_header.data());
}

DirectoryBlobPartition::DirectoryBlobPartition(const std::string& root_directory,
                                               std::optional<bool> is_wii)
    : m_root_directory(root_directory)
{
  SetDiscHeaderAndDiscType(is_wii);
  SetBI2();
  BuildFST(SetDOL(SetApploader()));
}

void DirectoryBlobPartition::SetDiscHeaderAndDiscType(std::optional<bool> is_wii)
{
  constexpr u64 DISKHEADER_ADDRESS = 0;
  constexpr u64 DISKHEADER_SIZE = 0x440;

  m_disk_header.resize(DISKHEADER_SIZE);
  const std::string boot_bin_path = m_root_directory + "sys/boot.bin";
  if (ReadFileToVector(boot_bin_path, &m_disk_header) < 0x20)
    ERROR_LOG(DISCIO, "%s doesn't exist or is too small", boot_bin_path.c_str());

  m_contents.emplace(DISKHEADER_ADDRESS, DISKHEADER_SIZE, m_disk_header.data());

  if (is_wii.has_value())
  {
    m_is_wii = *is_wii;
  }
  else
  {
    m_is_wii = Common::swap32(&m_disk_header[0x18]) == 0x5d1c9ea3;
    const bool is_gc = Common::swap32(&m_disk_header[0x1c]) == 0xc2339f3d;
    if (m_is_wii == is_gc)
      ERROR_LOG(DISCIO, "Couldn't detect disc type based on %s", boot_bin_path.c_str());
  }

  m_address_shift = m_is_wii ? 2 : 0;
}

void DirectoryBlobPartition::SetBI2()
{
  constexpr u64 BI2_ADDRESS = 0x440;
  constexpr u64 BI2_SIZE = 0x2000;
  AddFileToContents(&m_contents, m_root_directory + "sys/bi2.bin", BI2_ADDRESS, BI2_SIZE);
}

u64 DirectoryBlobPartition::SetApploader()
{
  bool success = false;

  const std::string path = m_root_directory + "sys/apploader.img";
  File::IOFile file(path, "rb");
  m_apploader.resize(file.GetSize());
  if (m_apploader.size() < 0x20 || !file.ReadBytes(m_apploader.data(), m_apploader.size()))
  {
    ERROR_LOG(DISCIO, "%s couldn't be accessed or is too small", path.c_str());
  }
  else
  {
    const size_t apploader_size = 0x20 + Common::swap32(*(u32*)&m_apploader[0x14]) +
                                  Common::swap32(*(u32*)&m_apploader[0x18]);
    if (apploader_size != m_apploader.size())
      ERROR_LOG(DISCIO, "%s is the wrong size... Is it really an apploader?", path.c_str());
    else
      success = true;
  }

  if (!success)
  {
    m_apploader.resize(0x20);
    // Make sure BS2 HLE doesn't try to run the apploader
    Write32(static_cast<u32>(-1), 0x10, &m_apploader);
  }

  constexpr u64 APPLOADER_ADDRESS = 0x2440;

  m_contents.emplace(APPLOADER_ADDRESS, m_apploader.size(), m_apploader.data());

  // Return DOL address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(APPLOADER_ADDRESS + m_apploader.size() + 0x20, 0x20ull);
}

u64 DirectoryBlobPartition::SetDOL(u64 dol_address)
{
  const DiscContent& dol =
      AddFileToContents(&m_contents, m_root_directory + "sys/main.dol", dol_address);

  Write32(static_cast<u32>(dol_address >> m_address_shift), 0x0420, &m_disk_header);

  // Return FST address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(dol_address + dol.GetSize() + 0x20, 0x20ull);
}

void DirectoryBlobPartition::BuildFST(u64 fst_address)
{
  m_fst_data.clear();

  File::FSTEntry rootEntry = File::ScanDirectoryTree(m_root_directory + "files/", true);

  ConvertUTF8NamesToSHIFTJIS(rootEntry);

  u32 name_table_size = Common::AlignUp(ComputeNameSize(rootEntry), 1ull << m_address_shift);
  u64 total_entries = rootEntry.size + 1;  // The root entry itself isn't counted in rootEntry.size

  const u64 name_table_offset = total_entries * ENTRY_SIZE;
  m_fst_data.resize(name_table_offset + name_table_size);

  // 32 KiB aligned start of data on disk
  u64 current_data_address = Common::AlignUp(fst_address + m_fst_data.size(), 0x8000ull);

  u32 fst_offset = 0;   // Offset within FST data
  u32 name_offset = 0;  // Offset within name table
  u32 root_offset = 0;  // Offset of root of FST

  // write root entry
  WriteEntryData(&fst_offset, DIRECTORY_ENTRY, 0, 0, total_entries, m_address_shift);

  WriteDirectory(rootEntry, &fst_offset, &name_offset, &current_data_address, root_offset,
                 name_table_offset);

  // overflow check, compare the aligned name offset with the aligned name table size
  _assert_(Common::AlignUp(name_offset, 1ull << m_address_shift) == name_table_size);

  // write FST size and location
  Write32((u32)(fst_address >> m_address_shift), 0x0424, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x0428, &m_disk_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x042c, &m_disk_header);

  m_contents.emplace(fst_address, m_fst_data.size(), m_fst_data.data());

  m_data_size = current_data_address;
}

void DirectoryBlobPartition::WriteEntryData(u32* entry_offset, u8 type, u32 name_offset,
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

void DirectoryBlobPartition::WriteEntryName(u32* name_offset, const std::string& name,
                                            u64 name_table_offset)
{
  strncpy((char*)&m_fst_data[*name_offset + name_table_offset], name.c_str(), name.length() + 1);

  *name_offset += (u32)(name.length() + 1);
}

void DirectoryBlobPartition::WriteDirectory(const File::FSTEntry& parent_entry, u32* fst_offset,
                                            u32* name_offset, u64* data_offset,
                                            u32 parent_entry_index, u64 name_table_offset)
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
      WriteEntryName(name_offset, entry.virtualName, name_table_offset);

      WriteDirectory(entry, fst_offset, name_offset, data_offset, entry_index, name_table_offset);
    }
    else
    {
      // put entry in FST
      WriteEntryData(fst_offset, FILE_ENTRY, *name_offset, *data_offset, entry.size,
                     m_address_shift);
      WriteEntryName(name_offset, entry.virtualName, name_table_offset);

      // write entry to virtual disc
      auto result = m_contents.emplace(*data_offset, entry.size, entry.physicalName);
      _dbg_assert_(DISCIO, result.second);  // Check that this offset wasn't already occupied

      // 32 KiB aligned - many games are fine with less alignment, but not all
      *data_offset = Common::AlignUp(*data_offset + std::max<u64>(entry.size, 1ull), 0x8000ull);
    }
  }
}

static const DiscContent& AddFileToContents(std::set<DiscContent>* contents,
                                            const std::string& path, u64 offset, u64 max_size)
{
  return *(contents->emplace(offset, std::min(File::GetSize(path), max_size), path).first);
}

static size_t ReadFileToVector(const std::string& path, std::vector<u8>* vector)
{
  File::IOFile file(path, "rb");
  size_t bytes_read;
  file.ReadArray<u8>(vector->data(), std::min<u64>(file.GetSize(), vector->size()), &bytes_read);
  return bytes_read;
}

static void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer)
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

static void Write32(u32 data, u32 offset, std::vector<u8>* buffer)
{
  (*buffer)[offset++] = (data >> 24);
  (*buffer)[offset++] = (data >> 16) & 0xff;
  (*buffer)[offset++] = (data >> 8) & 0xff;
  (*buffer)[offset] = (data)&0xff;
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

}  // namespace
