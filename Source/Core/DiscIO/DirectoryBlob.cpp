// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/DirectoryBlob.h"

#include <algorithm>
#include <array>
#include <cinttypes>
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
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Boot/DolReader.h"
#include "DiscIO/Blob.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
// Reads as many bytes as the vector fits (or less, if the file is smaller).
// Returns the number of bytes read.
static size_t ReadFileToVector(const std::string& path, std::vector<u8>* vector);

static void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer);
static void Write32(u32 data, u32 offset, std::vector<u8>* buffer);

static u32 ComputeNameSize(const File::FSTEntry& parent_entry);
static std::string ASCIIToUppercase(std::string str);
static void ConvertUTF8NamesToSHIFTJIS(File::FSTEntry* parent_entry);

enum class PartitionType : u32
{
  Game = 0,
  Update = 1,
  Channel = 2,
  // There are more types used by Super Smash Bros. Brawl, but they don't have special names
};

// 0xFF is an arbitrarily picked value. Note that we can't use 0x00, because that means NTSC-J
constexpr u32 INVALID_REGION = 0xFF;

constexpr u32 PARTITION_DATA_OFFSET = 0x20000;

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

u64 DiscContent::GetEndOffset() const
{
  return m_offset + m_size;
}

u64 DiscContent::GetSize() const
{
  return m_size;
}

bool DiscContent::Read(u64* offset, u64* length, u8** buffer) const
{
  if (m_size == 0)
    return true;

  DEBUG_ASSERT(*offset >= m_offset);
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

void DiscContentContainer::Add(u64 offset, u64 size, const std::string& path)
{
  if (size != 0)
    m_contents.emplace(offset, size, path);
}

void DiscContentContainer::Add(u64 offset, u64 size, const u8* data)
{
  if (size != 0)
    m_contents.emplace(offset, size, data);
}

u64 DiscContentContainer::CheckSizeAndAdd(u64 offset, const std::string& path)
{
  const u64 size = File::GetSize(path);
  Add(offset, size, path);
  return size;
}

u64 DiscContentContainer::CheckSizeAndAdd(u64 offset, u64 max_size, const std::string& path)
{
  const u64 size = std::min(File::GetSize(path), max_size);
  Add(offset, size, path);
  return size;
}

bool DiscContentContainer::Read(u64 offset, u64 length, u8* buffer) const
{
  // Determine which DiscContent the offset refers to
  std::set<DiscContent>::const_iterator it = m_contents.upper_bound(DiscContent(offset));

  while (it != m_contents.end() && length > 0)
  {
    // Zero fill to start of DiscContent data
    PadToAddress(it->GetOffset(), &offset, &length, &buffer);

    if (!it->Read(&offset, &length, &buffer))
      return false;

    ++it;
    DEBUG_ASSERT(it == m_contents.end() || it->GetOffset() >= offset);
  }

  // Zero fill if we went beyond the last DiscContent
  std::fill_n(buffer, static_cast<size_t>(length), 0);

  return true;
}

static std::optional<PartitionType> ParsePartitionDirectoryName(const std::string& name)
{
  if (name.size() < 2)
    return {};

  if (!strcasecmp(name.c_str(), "DATA"))
    return PartitionType::Game;
  if (!strcasecmp(name.c_str(), "UPDATE"))
    return PartitionType::Update;
  if (!strcasecmp(name.c_str(), "CHANNEL"))
    return PartitionType::Channel;

  if (name[0] == 'P' || name[0] == 'p')
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

static bool IsDirectorySeparator(char c)
{
  return c == '/'
#ifdef _WIN32
         || c == '\\'
#endif
      ;
}

static bool PathCharactersEqual(char a, char b)
{
  return a == b || (IsDirectorySeparator(a) && IsDirectorySeparator(b));
}

static bool PathEndsWith(const std::string& path, const std::string& suffix)
{
  if (suffix.size() > path.size())
    return false;

  std::string::const_iterator path_iterator = path.cend() - suffix.size();
  std::string::const_iterator suffix_iterator = suffix.cbegin();
  while (path_iterator != path.cend())
  {
    if (!PathCharactersEqual(*path_iterator, *suffix_iterator))
      return false;
    path_iterator++;
    suffix_iterator++;
  }

  return true;
}

static bool IsValidDirectoryBlob(const std::string& dol_path, std::string* partition_root,
                                 std::string* true_root = nullptr)
{
  if (!PathEndsWith(dol_path, "/sys/main.dol"))
    return false;

  const size_t chars_to_remove = std::string("sys/main.dol").size();
  *partition_root = dol_path.substr(0, dol_path.size() - chars_to_remove);

  if (File::GetSize(*partition_root + "sys/boot.bin") < 0x20)
    return false;

#ifdef _WIN32
  constexpr const char* dir_separator = "/\\";
#else
  constexpr char dir_separator = '/';
#endif
  if (true_root)
  {
    *true_root =
        dol_path.substr(0, dol_path.find_last_of(dir_separator, partition_root->size() - 2) + 1);
  }

  return true;
}

static bool ExistsAndIsValidDirectoryBlob(const std::string& dol_path)
{
  std::string partition_root;
  return File::Exists(dol_path) && IsValidDirectoryBlob(dol_path, &partition_root);
}

static bool IsInFilesDirectory(const std::string& path)
{
  size_t files_pos = std::string::npos;
  while (true)
  {
    files_pos = path.rfind("files", files_pos);
    if (files_pos == std::string::npos)
      return false;

    const size_t slash_before_pos = files_pos - 1;
    const size_t slash_after_pos = files_pos + 5;
    if ((files_pos == 0 || IsDirectorySeparator(path[slash_before_pos])) &&
        (slash_after_pos == path.size() || (IsDirectorySeparator(path[slash_after_pos]))) &&
        ExistsAndIsValidDirectoryBlob(path.substr(0, files_pos) + "sys/main.dol"))
    {
      return true;
    }

    --files_pos;
  }
}

static bool IsMainDolForNonGamePartition(const std::string& path)
{
  std::string partition_root, true_root;
  if (!IsValidDirectoryBlob(path, &partition_root, &true_root))
    return false;  // This is not a /sys/main.dol

  std::string partition_directory_name = partition_root.substr(true_root.size());
  partition_directory_name.pop_back();  // Remove trailing slash
  const std::optional<PartitionType> partition_type =
      ParsePartitionDirectoryName(partition_directory_name);
  if (!partition_type || *partition_type == PartitionType::Game)
    return false;  // volume_path is the game partition's /sys/main.dol

  const File::FSTEntry true_root_entry = File::ScanDirectoryTree(true_root, false);
  for (const File::FSTEntry& entry : true_root_entry.children)
  {
    if (entry.isDirectory &&
        ParsePartitionDirectoryName(entry.virtualName) == PartitionType::Game &&
        ExistsAndIsValidDirectoryBlob(entry.physicalName + "/sys/main.dol"))
    {
      return true;  // volume_path is the /sys/main.dol for a non-game partition
    }
  }

  return false;  // volume_path is the game partition's /sys/main.dol
}

bool ShouldHideFromGameList(const std::string& volume_path)
{
  return IsInFilesDirectory(volume_path) || IsMainDolForNonGamePartition(volume_path);
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

bool DirectoryBlobReader::Read(u64 offset, u64 length, u8* buffer)
{
  // TODO: We don't handle raw access to the encrypted area of Wii discs correctly.
  return (m_is_wii ? m_nonpartition_contents : m_gamecube_pseudopartition.GetContents())
      .Read(offset, length, buffer);
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

  return it->second.GetContents().Read(offset, size, buffer);
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
  constexpr u64 NONPARTITION_DISCHEADER_ADDRESS = 0;
  constexpr u64 NONPARTITION_DISCHEADER_SIZE = 0x100;

  m_disc_header_nonpartition.resize(NONPARTITION_DISCHEADER_SIZE);
  const size_t header_bin_bytes_read =
      ReadFileToVector(game_partition_root + "disc/header.bin", &m_disc_header_nonpartition);

  // If header.bin is missing or smaller than expected, use the content of sys/boot.bin instead
  std::copy(partition_header.data() + header_bin_bytes_read,
            partition_header.data() + m_disc_header_nonpartition.size(),
            m_disc_header_nonpartition.data() + header_bin_bytes_read);

  // 0x60 and 0x61 are the only differences between the partition and non-partition headers
  if (header_bin_bytes_read < 0x60)
    m_disc_header_nonpartition[0x60] = 0;
  if (header_bin_bytes_read < 0x61)
    m_disc_header_nonpartition[0x61] = 0;

  m_nonpartition_contents.Add(NONPARTITION_DISCHEADER_ADDRESS, m_disc_header_nonpartition);
}

void DirectoryBlobReader::SetWiiRegionData(const std::string& game_partition_root)
{
  m_wii_region_data.resize(0x10, 0x00);
  m_wii_region_data.resize(0x20, 0x80);
  Write32(INVALID_REGION, 0, &m_wii_region_data);

  const std::string region_bin_path = game_partition_root + "disc/region.bin";
  const size_t bytes_read = ReadFileToVector(region_bin_path, &m_wii_region_data);
  if (bytes_read < 0x4)
    ERROR_LOG(DISCIO, "Couldn't read region from %s", region_bin_path.c_str());
  else if (bytes_read < 0x20)
    ERROR_LOG(DISCIO, "Couldn't read age ratings from %s", region_bin_path.c_str());

  constexpr u64 WII_REGION_DATA_ADDRESS = 0x4E000;
  constexpr u64 WII_REGION_DATA_SIZE = 0x20;
  m_nonpartition_contents.Add(WII_REGION_DATA_ADDRESS, m_wii_region_data);
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

  u32 subtable_1_size = 0;
  while (subtable_1_size < partitions.size() && subtable_1_size < 3 &&
         partitions[subtable_1_size].type <= PartitionType::Channel)
  {
    ++subtable_1_size;
  }
  const u32 subtable_2_size = static_cast<u32>(partitions.size() - subtable_1_size);

  constexpr u32 PARTITION_TABLE_ADDRESS = 0x40000;
  constexpr u32 PARTITION_SUBTABLE1_OFFSET = 0x20;
  constexpr u32 PARTITION_SUBTABLE2_OFFSET = 0x40;
  m_partition_table.resize(PARTITION_SUBTABLE2_OFFSET + subtable_2_size * 8);

  Write32(subtable_1_size, 0x0, &m_partition_table);
  Write32((PARTITION_TABLE_ADDRESS + PARTITION_SUBTABLE1_OFFSET) >> 2, 0x4, &m_partition_table);
  if (subtable_2_size != 0)
  {
    Write32(subtable_2_size, 0x8, &m_partition_table);
    Write32((PARTITION_TABLE_ADDRESS + PARTITION_SUBTABLE2_OFFSET) >> 2, 0xC, &m_partition_table);
  }

  constexpr u64 STANDARD_UPDATE_PARTITION_ADDRESS = 0x50000;
  constexpr u64 STANDARD_GAME_PARTITION_ADDRESS = 0xF800000;
  u64 partition_address = STANDARD_UPDATE_PARTITION_ADDRESS;
  u64 offset_in_table = PARTITION_SUBTABLE1_OFFSET;
  for (size_t i = 0; i < partitions.size(); ++i)
  {
    if (i == subtable_1_size)
      offset_in_table = PARTITION_SUBTABLE2_OFFSET;

    if (partitions[i].type == PartitionType::Game)
      partition_address = std::max(partition_address, STANDARD_GAME_PARTITION_ADDRESS);

    Write32(static_cast<u32>(partition_address >> 2), offset_in_table, &m_partition_table);
    offset_in_table += 4;
    Write32(static_cast<u32>(partitions[i].type), offset_in_table, &m_partition_table);
    offset_in_table += 4;

    SetPartitionHeader(partitions[i].partition, partition_address);

    const u64 partition_data_size = partitions[i].partition.GetDataSize();
    m_partitions.emplace(partition_address, std::move(partitions[i].partition));
    const u64 unaligned_next_partition_address = VolumeWii::EncryptedPartitionOffsetToRawOffset(
        partition_data_size, Partition(partition_address), PARTITION_DATA_OFFSET);
    partition_address = Common::AlignUp(unaligned_next_partition_address, 0x10000ull);
  }
  m_data_size = partition_address;

  m_nonpartition_contents.Add(PARTITION_TABLE_ADDRESS, m_partition_table);
}

// This function sets the header that's shortly before the start of the encrypted
// area, not the header that's right at the beginning of the encrypted area
void DirectoryBlobReader::SetPartitionHeader(const DirectoryBlobPartition& partition,
                                             u64 partition_address)
{
  constexpr u32 TICKET_OFFSET = 0x0;
  constexpr u32 TICKET_SIZE = 0x2a4;
  constexpr u32 TMD_OFFSET = 0x2c0;
  constexpr u32 MAX_TMD_SIZE = 0x49e4;
  constexpr u32 H3_OFFSET = 0x4000;
  constexpr u32 H3_SIZE = 0x18000;

  const std::string& partition_root = partition.GetRootDirectory();

  m_nonpartition_contents.CheckSizeAndAdd(partition_address + TICKET_OFFSET, TICKET_SIZE,
                                          partition_root + "ticket.bin");

  const u64 tmd_size = m_nonpartition_contents.CheckSizeAndAdd(
      partition_address + TMD_OFFSET, MAX_TMD_SIZE, partition_root + "tmd.bin");

  const u64 cert_offset = Common::AlignUp(TMD_OFFSET + tmd_size, 0x20ull);
  const u64 max_cert_size = H3_OFFSET - cert_offset;
  const u64 cert_size = m_nonpartition_contents.CheckSizeAndAdd(
      partition_address + cert_offset, max_cert_size, partition_root + "cert.bin");

  m_nonpartition_contents.CheckSizeAndAdd(partition_address + H3_OFFSET, H3_SIZE,
                                          partition_root + "h3.bin");

  constexpr u32 PARTITION_HEADER_SIZE = 0x1c;
  const u64 data_size = Common::AlignUp(partition.GetDataSize(), 0x7c00) / 0x7c00 * 0x8000;
  m_partition_headers.emplace_back(PARTITION_HEADER_SIZE);
  std::vector<u8>& partition_header = m_partition_headers.back();
  Write32(static_cast<u32>(tmd_size), 0x0, &partition_header);
  Write32(TMD_OFFSET >> 2, 0x4, &partition_header);
  Write32(static_cast<u32>(cert_size), 0x8, &partition_header);
  Write32(static_cast<u32>(cert_offset >> 2), 0x0C, &partition_header);
  Write32(H3_OFFSET >> 2, 0x10, &partition_header);
  Write32(PARTITION_DATA_OFFSET >> 2, 0x14, &partition_header);
  Write32(static_cast<u32>(data_size >> 2), 0x18, &partition_header);

  m_nonpartition_contents.Add(partition_address + TICKET_SIZE, partition_header);
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
  constexpr u64 DISCHEADER_ADDRESS = 0;
  constexpr u64 DISCHEADER_SIZE = 0x440;

  m_disc_header.resize(DISCHEADER_SIZE);
  const std::string boot_bin_path = m_root_directory + "sys/boot.bin";
  if (ReadFileToVector(boot_bin_path, &m_disc_header) < 0x20)
    ERROR_LOG(DISCIO, "%s doesn't exist or is too small", boot_bin_path.c_str());

  m_contents.Add(DISCHEADER_ADDRESS, m_disc_header);

  if (is_wii.has_value())
  {
    m_is_wii = *is_wii;
  }
  else
  {
    m_is_wii = Common::swap32(&m_disc_header[0x18]) == 0x5d1c9ea3;
    const bool is_gc = Common::swap32(&m_disc_header[0x1c]) == 0xc2339f3d;
    if (m_is_wii == is_gc)
      ERROR_LOG(DISCIO, "Couldn't detect disc type based on %s", boot_bin_path.c_str());
  }

  m_address_shift = m_is_wii ? 2 : 0;
}

void DirectoryBlobPartition::SetBI2()
{
  constexpr u64 BI2_ADDRESS = 0x440;
  constexpr u64 BI2_SIZE = 0x2000;
  m_bi2.resize(BI2_SIZE);

  if (!m_is_wii)
    Write32(INVALID_REGION, 0x18, &m_bi2);

  const std::string bi2_path = m_root_directory + "sys/bi2.bin";
  const size_t bytes_read = ReadFileToVector(bi2_path, &m_bi2);
  if (!m_is_wii && bytes_read < 0x1C)
    ERROR_LOG(DISCIO, "Couldn't read region from %s", bi2_path.c_str());

  m_contents.Add(BI2_ADDRESS, m_bi2);
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

  m_contents.Add(APPLOADER_ADDRESS, m_apploader);

  // Return DOL address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(APPLOADER_ADDRESS + m_apploader.size() + 0x20, 0x20ull);
}

u64 DirectoryBlobPartition::SetDOL(u64 dol_address)
{
  const u64 dol_size = m_contents.CheckSizeAndAdd(dol_address, m_root_directory + "sys/main.dol");

  Write32(static_cast<u32>(dol_address >> m_address_shift), 0x0420, &m_disc_header);

  // Return FST address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(dol_address + dol_size + 0x20, 0x20ull);
}

void DirectoryBlobPartition::BuildFST(u64 fst_address)
{
  m_fst_data.clear();

  File::FSTEntry rootEntry = File::ScanDirectoryTree(m_root_directory + "files/", true);

  ConvertUTF8NamesToSHIFTJIS(&rootEntry);

  u32 name_table_size = Common::AlignUp(ComputeNameSize(rootEntry), 1ull << m_address_shift);
  u64 total_entries = rootEntry.size + 1;  // The root entry itself isn't counted in rootEntry.size

  const u64 name_table_offset = total_entries * ENTRY_SIZE;
  m_fst_data.resize(name_table_offset + name_table_size);

  // 32 KiB aligned start of data on disc
  u64 current_data_address = Common::AlignUp(fst_address + m_fst_data.size(), 0x8000ull);

  u32 fst_offset = 0;   // Offset within FST data
  u32 name_offset = 0;  // Offset within name table
  u32 root_offset = 0;  // Offset of root of FST

  // write root entry
  WriteEntryData(&fst_offset, DIRECTORY_ENTRY, 0, 0, total_entries, m_address_shift);

  WriteDirectory(rootEntry, &fst_offset, &name_offset, &current_data_address, root_offset,
                 name_table_offset);

  // overflow check, compare the aligned name offset with the aligned name table size
  ASSERT(Common::AlignUp(name_offset, 1ull << m_address_shift) == name_table_size);

  // write FST size and location
  Write32((u32)(fst_address >> m_address_shift), 0x0424, &m_disc_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x0428, &m_disc_header);
  Write32((u32)(m_fst_data.size() >> m_address_shift), 0x042c, &m_disc_header);

  m_contents.Add(fst_address, m_fst_data);

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
  std::sort(sorted_entries.begin(), sorted_entries.end(),
            [](const File::FSTEntry& one, const File::FSTEntry& two) {
              const std::string one_upper = ASCIIToUppercase(one.virtualName);
              const std::string two_upper = ASCIIToUppercase(two.virtualName);
              return one_upper == two_upper ? one.virtualName < two.virtualName :
                                              one_upper < two_upper;
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
      m_contents.Add(*data_offset, entry.size, entry.physicalName);

      // 32 KiB aligned - many games are fine with less alignment, but not all
      *data_offset = Common::AlignUp(*data_offset + entry.size, 0x8000ull);
    }
  }
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
  (*buffer)[offset] = data & 0xff;
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

static void ConvertUTF8NamesToSHIFTJIS(File::FSTEntry* parent_entry)
{
  for (File::FSTEntry& entry : parent_entry->children)
  {
    if (entry.isDirectory)
      ConvertUTF8NamesToSHIFTJIS(&entry);

    entry.virtualName = UTF8ToSHIFTJIS(entry.virtualName);
  }
}

static std::string ASCIIToUppercase(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(),
                 [](char c) { return std::toupper(c, std::locale::classic()); });
  return str;
}

}  // namespace DiscIO
