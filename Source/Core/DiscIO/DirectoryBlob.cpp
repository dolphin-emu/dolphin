// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/DirectoryBlob.h"

#include <algorithm>
#include <array>
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
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"
#include "Core/Boot/DolReader.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/VolumeDisc.h"
#include "DiscIO/VolumeWii.h"
#include "DiscIO/WiiEncryptionCache.h"

namespace DiscIO
{
// Reads as many bytes as the vector fits (or less, if the file is smaller).
// Returns the number of bytes read.
static size_t ReadFileToVector(const std::string& path, std::vector<u8>* vector);

static void PadToAddress(u64 start_address, u64* address, u64* length, u8** buffer);
static void Write32(u32 data, u32 offset, std::vector<u8>* buffer);

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

DiscContent::DiscContent(u64 offset, u64 size, ContentSource source)
    : m_offset(offset), m_size(size), m_content_source(std::move(source))
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

bool DiscContent::Read(u64* offset, u64* length, u8** buffer, DirectoryBlobReader* blob) const
{
  if (m_size == 0)
    return true;

  DEBUG_ASSERT(*offset >= m_offset);
  const u64 offset_in_content = *offset - m_offset;

  if (offset_in_content < m_size)
  {
    const u64 bytes_to_read = std::min(m_size - offset_in_content, *length);

    if (std::holds_alternative<ContentFile>(m_content_source))
    {
      const auto& content = std::get<ContentFile>(m_content_source);
      File::IOFile file(content.m_filename, "rb");
      if (!file.Seek(content.m_offset + offset_in_content, File::SeekOrigin::Begin) ||
          !file.ReadBytes(*buffer, bytes_to_read))
      {
        return false;
      }
    }
    else if (std::holds_alternative<ContentMemory>(m_content_source))
    {
      const auto& content = std::get<ContentMemory>(m_content_source);
      std::copy(content->begin() + offset_in_content,
                content->begin() + offset_in_content + bytes_to_read, *buffer);
    }
    else if (std::holds_alternative<ContentPartition>(m_content_source))
    {
      const auto& content = std::get<ContentPartition>(m_content_source);
      const u64 decrypted_size = m_size * VolumeWii::BLOCK_DATA_SIZE / VolumeWii::BLOCK_TOTAL_SIZE;
      if (!blob->EncryptPartitionData(content.m_offset + offset_in_content, bytes_to_read, *buffer,
                                      content.m_partition_data_offset, decrypted_size))
      {
        return false;
      }
    }
    else if (std::holds_alternative<ContentVolume>(m_content_source))
    {
      const auto& source = std::get<ContentVolume>(m_content_source);
      if (!blob->GetWrappedVolume()->Read(source.m_offset + offset_in_content, bytes_to_read,
                                          *buffer, source.m_partition))
      {
        return false;
      }
    }
    else if (std::holds_alternative<ContentFixedByte>(m_content_source))
    {
      const ContentFixedByte& source = std::get<ContentFixedByte>(m_content_source);
      std::fill_n(*buffer, bytes_to_read, source.m_byte);
    }
    else
    {
      PanicAlertFmt("DirectoryBlob: Invalid content source in DiscContent.");
      return false;
    }

    *length -= bytes_to_read;
    *buffer += bytes_to_read;
    *offset += bytes_to_read;
  }

  return true;
}

void DiscContentContainer::Add(u64 offset, u64 size, ContentSource source)
{
  if (size != 0)
    m_contents.emplace(offset, size, std::move(source));
}

u64 DiscContentContainer::CheckSizeAndAdd(u64 offset, const std::string& path)
{
  const u64 size = File::GetSize(path);
  Add(offset, size, ContentFile{path, 0});
  return size;
}

u64 DiscContentContainer::CheckSizeAndAdd(u64 offset, u64 max_size, const std::string& path)
{
  const u64 size = std::min(File::GetSize(path), max_size);
  Add(offset, size, ContentFile{path, 0});
  return size;
}

bool DiscContentContainer::Read(u64 offset, u64 length, u8* buffer, DirectoryBlobReader* blob) const
{
  // Determine which DiscContent the offset refers to
  std::set<DiscContent>::const_iterator it = m_contents.upper_bound(DiscContent(offset));

  while (it != m_contents.end() && length > 0)
  {
    // Zero fill to start of DiscContent data
    PadToAddress(it->GetOffset(), &offset, &length, &buffer);

    if (length == 0)
      return true;

    if (!it->Read(&offset, &length, &buffer, blob))
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

  if (true_root)
  {
#ifdef _WIN32
    constexpr const char* dir_separator = "/\\";
#else
    constexpr char dir_separator = '/';
#endif
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

std::unique_ptr<DirectoryBlobReader> DirectoryBlobReader::Create(
    std::unique_ptr<DiscIO::VolumeDisc> volume,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
        fst_callback)
{
  if (!volume)
    return nullptr;

  return std::unique_ptr<DirectoryBlobReader>(
      new DirectoryBlobReader(std::move(volume), sys_callback, fst_callback));
}

DirectoryBlobReader::DirectoryBlobReader(const std::string& game_partition_root,
                                         const std::string& true_root)
    : m_encryption_cache(this)
{
  DirectoryBlobPartition game_partition(game_partition_root, {});
  m_is_wii = game_partition.IsWii();

  if (!m_is_wii)
  {
    m_gamecube_pseudopartition = std::move(game_partition);
    m_data_size = m_gamecube_pseudopartition.GetDataSize();
    m_encrypted = false;
  }
  else
  {
    std::vector<u8> disc_header(DISCHEADER_SIZE);
    game_partition.GetContents().Read(DISCHEADER_ADDRESS, DISCHEADER_SIZE, disc_header.data(),
                                      this);
    SetNonpartitionDiscHeaderFromFile(disc_header, game_partition_root);
    SetWiiRegionDataFromFile(game_partition_root);

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

DirectoryBlobReader::DirectoryBlobReader(
    std::unique_ptr<DiscIO::VolumeDisc> volume,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
        fst_callback)
    : m_encryption_cache(this), m_wrapped_volume(std::move(volume))
{
  DirectoryBlobPartition game_partition(m_wrapped_volume.get(),
                                        m_wrapped_volume->GetGamePartition(), std::nullopt,
                                        sys_callback, fst_callback, this);
  m_is_wii = game_partition.IsWii();

  if (!m_is_wii)
  {
    m_gamecube_pseudopartition = std::move(game_partition);
    m_data_size = m_gamecube_pseudopartition.GetDataSize();
    m_encrypted = false;
  }
  else
  {
    std::vector<u8> header_bin(WII_NONPARTITION_DISCHEADER_SIZE);
    if (!m_wrapped_volume->Read(WII_NONPARTITION_DISCHEADER_ADDRESS,
                                WII_NONPARTITION_DISCHEADER_SIZE, header_bin.data(),
                                PARTITION_NONE))
    {
      header_bin.clear();
    }
    std::vector<u8> disc_header(DISCHEADER_SIZE);
    game_partition.GetContents().Read(DISCHEADER_ADDRESS, DISCHEADER_SIZE, disc_header.data(),
                                      this);
    SetNonpartitionDiscHeader(disc_header, std::move(header_bin));

    std::vector<u8> wii_region_data(WII_REGION_DATA_SIZE);
    if (!m_wrapped_volume->Read(WII_REGION_DATA_ADDRESS, WII_REGION_DATA_SIZE,
                                wii_region_data.data(), PARTITION_NONE))
    {
      wii_region_data.clear();
    }
    SetWiiRegionData(wii_region_data, "volume");

    std::vector<PartitionWithType> partitions;
    partitions.emplace_back(std::move(game_partition), PartitionType::Game);

    for (Partition partition : m_wrapped_volume->GetPartitions())
    {
      if (partition == m_wrapped_volume->GetGamePartition())
        continue;

      if (auto type = m_wrapped_volume->GetPartitionType(partition))
      {
        partitions.emplace_back(DirectoryBlobPartition(m_wrapped_volume.get(), partition, m_is_wii,
                                                       nullptr, nullptr, this),
                                static_cast<PartitionType>(*type));
      }
    }

    SetPartitions(std::move(partitions));
  }
}

DirectoryBlobReader::DirectoryBlobReader(const DirectoryBlobReader& rhs)
    : m_gamecube_pseudopartition(rhs.m_gamecube_pseudopartition),
      m_nonpartition_contents(rhs.m_nonpartition_contents), m_partitions(rhs.m_partitions),
      m_encryption_cache(this), m_is_wii(rhs.m_is_wii), m_encrypted(rhs.m_encrypted),
      m_data_size(rhs.m_data_size),
      m_wrapped_volume(rhs.m_wrapped_volume ?
                           CreateDisc(rhs.m_wrapped_volume->GetBlobReader().CopyReader()) :
                           nullptr)
{
}

bool DirectoryBlobReader::Read(u64 offset, u64 length, u8* buffer)
{
  if (offset + length > m_data_size)
    return false;

  return (m_is_wii ? m_nonpartition_contents : m_gamecube_pseudopartition.GetContents())
      .Read(offset, length, buffer, this);
}

const DirectoryBlobPartition* DirectoryBlobReader::GetPartition(u64 offset, u64 size,
                                                                u64 partition_data_offset) const
{
  const auto it = m_partitions.find(partition_data_offset);
  if (it == m_partitions.end())
    return nullptr;

  if (offset + size > it->second.GetDataSize())
    return nullptr;

  return &it->second;
}

bool DirectoryBlobReader::SupportsReadWiiDecrypted(u64 offset, u64 size,
                                                   u64 partition_data_offset) const
{
  return static_cast<bool>(GetPartition(offset, size, partition_data_offset));
}

bool DirectoryBlobReader::ReadWiiDecrypted(u64 offset, u64 size, u8* buffer,
                                           u64 partition_data_offset)
{
  const DirectoryBlobPartition* partition = GetPartition(offset, size, partition_data_offset);
  if (!partition)
    return false;

  return partition->GetContents().Read(offset, size, buffer, this);
}

bool DirectoryBlobReader::EncryptPartitionData(u64 offset, u64 size, u8* buffer,
                                               u64 partition_data_offset,
                                               u64 partition_data_decrypted_size)
{
  auto it = m_partitions.find(partition_data_offset);
  if (it == m_partitions.end())
    return false;

  if (!m_encrypted)
    return it->second.GetContents().Read(offset, size, buffer, this);

  return m_encryption_cache.EncryptGroups(offset, size, buffer, partition_data_offset,
                                          partition_data_decrypted_size, it->second.GetKey());
}

BlobType DirectoryBlobReader::GetBlobType() const
{
  return BlobType::DIRECTORY;
}

std::unique_ptr<BlobReader> DirectoryBlobReader::CopyReader() const
{
  return std::unique_ptr<DirectoryBlobReader>(new DirectoryBlobReader(*this));
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

void DirectoryBlobReader::SetNonpartitionDiscHeaderFromFile(const std::vector<u8>& partition_header,
                                                            const std::string& game_partition_root)
{
  std::vector<u8> header_bin(WII_NONPARTITION_DISCHEADER_SIZE);
  const size_t header_bin_bytes_read =
      ReadFileToVector(game_partition_root + "disc/header.bin", &header_bin);
  header_bin.resize(header_bin_bytes_read);
  SetNonpartitionDiscHeader(partition_header, std::move(header_bin));
}

void DirectoryBlobReader::SetNonpartitionDiscHeader(const std::vector<u8>& partition_header,
                                                    std::vector<u8> header_bin)
{
  const size_t header_bin_size = header_bin.size();
  header_bin.resize(WII_NONPARTITION_DISCHEADER_SIZE);

  // If header.bin is missing or smaller than expected, use the content of sys/boot.bin instead
  if (header_bin_size < header_bin.size())
  {
    std::copy(partition_header.data() + header_bin_size,
              partition_header.data() + header_bin.size(), header_bin.data() + header_bin_size);
  }

  // 0x60 and 0x61 are the only differences between the partition and non-partition headers
  if (header_bin_size < 0x60)
    header_bin[0x60] = 0;
  if (header_bin_size < 0x61)
    header_bin[0x61] = 0;

  m_encrypted =
      std::all_of(header_bin.data() + 0x60, header_bin.data() + 0x64, [](u8 x) { return x == 0; });

  m_nonpartition_contents.Add(WII_NONPARTITION_DISCHEADER_ADDRESS, std::move(header_bin));
}

void DirectoryBlobReader::SetWiiRegionDataFromFile(const std::string& game_partition_root)
{
  std::vector<u8> wii_region_data(WII_REGION_DATA_SIZE);
  const std::string region_bin_path = game_partition_root + "disc/region.bin";
  const size_t bytes_read = ReadFileToVector(region_bin_path, &wii_region_data);
  wii_region_data.resize(bytes_read);
  SetWiiRegionData(wii_region_data, region_bin_path);
}

void DirectoryBlobReader::SetWiiRegionData(const std::vector<u8>& wii_region_data,
                                           const std::string& log_path)
{
  std::vector<u8> region_data(0x10, 0x00);
  region_data.resize(WII_REGION_DATA_SIZE, 0x80);
  Write32(INVALID_REGION, 0, &region_data);

  std::copy_n(wii_region_data.begin(),
              std::min<size_t>(wii_region_data.size(), WII_REGION_DATA_SIZE), region_data.begin());

  if (wii_region_data.size() < 0x4)
    ERROR_LOG_FMT(DISCIO, "Couldn't read region from {}", log_path);
  else if (wii_region_data.size() < 0x20)
    ERROR_LOG_FMT(DISCIO, "Couldn't read age ratings from {}", log_path);

  m_nonpartition_contents.Add(WII_REGION_DATA_ADDRESS, std::move(region_data));
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
  std::vector<u8> partition_table(PARTITION_SUBTABLE2_OFFSET + subtable_2_size * 8);

  Write32(subtable_1_size, 0x0, &partition_table);
  Write32((PARTITION_TABLE_ADDRESS + PARTITION_SUBTABLE1_OFFSET) >> 2, 0x4, &partition_table);
  if (subtable_2_size != 0)
  {
    Write32(subtable_2_size, 0x8, &partition_table);
    Write32((PARTITION_TABLE_ADDRESS + PARTITION_SUBTABLE2_OFFSET) >> 2, 0xC, &partition_table);
  }

  constexpr u64 STANDARD_UPDATE_PARTITION_ADDRESS = 0x50000;
  u64 partition_address = STANDARD_UPDATE_PARTITION_ADDRESS;
  u64 offset_in_table = PARTITION_SUBTABLE1_OFFSET;
  for (size_t i = 0; i < partitions.size(); ++i)
  {
    constexpr u64 STANDARD_GAME_PARTITION_ADDRESS = 0xF800000;
    if (i == subtable_1_size)
      offset_in_table = PARTITION_SUBTABLE2_OFFSET;

    if (partitions[i].type == PartitionType::Game)
      partition_address = std::max(partition_address, STANDARD_GAME_PARTITION_ADDRESS);

    Write32(static_cast<u32>(partition_address >> 2), offset_in_table, &partition_table);
    offset_in_table += 4;
    Write32(static_cast<u32>(partitions[i].type), offset_in_table, &partition_table);
    offset_in_table += 4;

    SetPartitionHeader(&partitions[i].partition, partition_address);

    const u64 data_size =
        Common::AlignUp(partitions[i].partition.GetDataSize(), VolumeWii::BLOCK_DATA_SIZE);
    partitions[i].partition.SetDataSize(data_size);
    const u64 encrypted_data_size =
        (data_size / VolumeWii::BLOCK_DATA_SIZE) * VolumeWii::BLOCK_TOTAL_SIZE;
    const u64 partition_data_offset = partition_address + PARTITION_DATA_OFFSET;
    m_partitions.emplace(partition_data_offset, std::move(partitions[i].partition));
    m_nonpartition_contents.Add(partition_data_offset, encrypted_data_size,
                                ContentPartition{0, partition_data_offset});
    const u64 unaligned_next_partition_address = VolumeWii::OffsetInHashedPartitionToRawOffset(
        data_size, Partition(partition_address), PARTITION_DATA_OFFSET);
    partition_address = Common::AlignUp(unaligned_next_partition_address, 0x10000ull);
  }
  m_data_size = partition_address;

  m_nonpartition_contents.Add(PARTITION_TABLE_ADDRESS, std::move(partition_table));
}

// This function sets the header that's shortly before the start of the encrypted
// area, not the header that's right at the beginning of the encrypted area
void DirectoryBlobReader::SetPartitionHeader(DirectoryBlobPartition* partition,
                                             u64 partition_address)
{
  constexpr u32 TMD_OFFSET = 0x2c0;
  constexpr u32 H3_OFFSET = 0x4000;

  const std::optional<DiscIO::Partition>& wrapped_partition = partition->GetWrappedPartition();
  const std::string& partition_root = partition->GetRootDirectory();

  u64 ticket_size;
  if (wrapped_partition)
  {
    std::vector<u8> new_ticket = m_wrapped_volume->GetTicket(*wrapped_partition).GetBytes();
    if (new_ticket.size() > WII_PARTITION_TICKET_SIZE)
      new_ticket.resize(WII_PARTITION_TICKET_SIZE);
    ticket_size = new_ticket.size();
    m_nonpartition_contents.Add(partition_address + WII_PARTITION_TICKET_ADDRESS,
                                std::move(new_ticket));
  }
  else
  {
    ticket_size = m_nonpartition_contents.CheckSizeAndAdd(
        partition_address + WII_PARTITION_TICKET_ADDRESS, WII_PARTITION_TICKET_SIZE,
        partition_root + "ticket.bin");
  }

  u64 tmd_size;
  if (wrapped_partition)
  {
    std::vector<u8> new_tmd = m_wrapped_volume->GetTMD(*wrapped_partition).GetBytes();
    if (new_tmd.size() > IOS::ES::MAX_TMD_SIZE)
      new_tmd.resize(IOS::ES::MAX_TMD_SIZE);
    tmd_size = new_tmd.size();
    m_nonpartition_contents.Add(partition_address + TMD_OFFSET, std::move(new_tmd));
  }
  else
  {
    tmd_size = m_nonpartition_contents.CheckSizeAndAdd(
        partition_address + TMD_OFFSET, IOS::ES::MAX_TMD_SIZE, partition_root + "tmd.bin");
  }

  const u64 cert_offset = Common::AlignUp(TMD_OFFSET + tmd_size, 0x20ull);
  const u64 max_cert_size = H3_OFFSET - cert_offset;

  u64 cert_size;
  if (wrapped_partition)
  {
    std::vector<u8> new_cert = m_wrapped_volume->GetCertificateChain(*wrapped_partition);
    if (new_cert.size() > max_cert_size)
      new_cert.resize(max_cert_size);
    cert_size = new_cert.size();
    m_nonpartition_contents.Add(partition_address + cert_offset, std::move(new_cert));
  }
  else
  {
    cert_size = m_nonpartition_contents.CheckSizeAndAdd(partition_address + cert_offset,
                                                        max_cert_size, partition_root + "cert.bin");
  }

  if (wrapped_partition)
  {
    if (m_wrapped_volume->HasWiiHashes())
    {
      const std::optional<u64> offset = m_wrapped_volume->ReadSwappedAndShifted(
          wrapped_partition->offset + WII_PARTITION_H3_OFFSET_ADDRESS, PARTITION_NONE);
      if (offset)
      {
        std::vector<u8> new_h3(WII_PARTITION_H3_SIZE);
        if (m_wrapped_volume->Read(wrapped_partition->offset + *offset, new_h3.size(),
                                   new_h3.data(), PARTITION_NONE))
        {
          m_nonpartition_contents.Add(partition_address + H3_OFFSET, std::move(new_h3));
        }
      }
    }
  }
  else
  {
    m_nonpartition_contents.CheckSizeAndAdd(partition_address + H3_OFFSET, WII_PARTITION_H3_SIZE,
                                            partition_root + "h3.bin");
  }

  constexpr u32 PARTITION_HEADER_SIZE = 0x1c;
  const u64 data_size = Common::AlignUp(partition->GetDataSize(), 0x7c00) / 0x7c00 * 0x8000;
  std::vector<u8> partition_header(PARTITION_HEADER_SIZE);
  Write32(static_cast<u32>(tmd_size), 0x0, &partition_header);
  Write32(TMD_OFFSET >> 2, 0x4, &partition_header);
  Write32(static_cast<u32>(cert_size), 0x8, &partition_header);
  Write32(static_cast<u32>(cert_offset >> 2), 0x0C, &partition_header);
  Write32(H3_OFFSET >> 2, 0x10, &partition_header);
  Write32(PARTITION_DATA_OFFSET >> 2, 0x14, &partition_header);
  Write32(static_cast<u32>(data_size >> 2), 0x18, &partition_header);

  m_nonpartition_contents.Add(partition_address + WII_PARTITION_TICKET_SIZE,
                              std::move(partition_header));

  std::vector<u8> ticket_buffer(ticket_size);
  m_nonpartition_contents.Read(partition_address + WII_PARTITION_TICKET_ADDRESS, ticket_size,
                               ticket_buffer.data(), this);
  IOS::ES::TicketReader ticket(std::move(ticket_buffer));
  if (ticket.IsValid())
    partition->SetKey(ticket.GetTitleKey());
}

static void GenerateBuilderNodesFromFileSystem(const DiscIO::VolumeDisc& volume,
                                               const DiscIO::Partition& partition,
                                               std::vector<FSTBuilderNode>* nodes,
                                               const FileInfo& parent_info)
{
  for (const FileInfo& file_info : parent_info)
  {
    if (file_info.IsDirectory())
    {
      std::vector<FSTBuilderNode> child_nodes;
      GenerateBuilderNodesFromFileSystem(volume, partition, &child_nodes, file_info);
      nodes->emplace_back(FSTBuilderNode{file_info.GetName(), file_info.GetTotalChildren(),
                                         std::move(child_nodes)});
    }
    else
    {
      std::vector<BuilderContentSource> source;
      source.emplace_back(BuilderContentSource{0, file_info.GetSize(),
                                               ContentVolume{file_info.GetOffset(), partition}});
      nodes->emplace_back(
          FSTBuilderNode{file_info.GetName(), file_info.GetSize(), std::move(source)});
    }
  }
}

DirectoryBlobPartition::DirectoryBlobPartition(const std::string& root_directory,
                                               std::optional<bool> is_wii)
    : m_root_directory(root_directory)
{
  std::vector<u8> disc_header(DISCHEADER_SIZE);
  if (ReadFileToVector(m_root_directory + "sys/boot.bin", &disc_header) < 0x20)
    ERROR_LOG_FMT(DISCIO, "{} doesn't exist or is too small", m_root_directory + "sys/boot.bin");

  SetDiscType(is_wii, disc_header);
  SetBI2FromFile(m_root_directory + "sys/bi2.bin");
  const u64 dol_address = SetApploaderFromFile(m_root_directory + "sys/apploader.img");
  const u64 fst_address =
      SetDOLFromFile(m_root_directory + "sys/main.dol", dol_address, &disc_header);
  BuildFSTFromFolder(m_root_directory + "files/", fst_address, &disc_header);

  m_contents.Add(DISCHEADER_ADDRESS, disc_header);
}

static void FillSingleFileNode(FSTBuilderNode* node, std::vector<u8> data)
{
  std::vector<BuilderContentSource> contents;
  const size_t size = data.size();
  contents.emplace_back(
      BuilderContentSource{0, size, std::make_shared<std::vector<u8>>(std::move(data))});
  node->m_size = size;
  node->m_content = std::move(contents);
}

static FSTBuilderNode BuildSingleFileNode(std::string filename, std::vector<u8> data,
                                          void* userdata)
{
  FSTBuilderNode node{std::move(filename), 0, {}, userdata};
  FillSingleFileNode(&node, std::move(data));
  return node;
}

static std::vector<u8> ExtractNodeToVector(std::vector<FSTBuilderNode>* nodes, void* userdata,
                                           DirectoryBlobReader* blob)
{
  std::vector<u8> data;
  const auto it =
      std::find_if(nodes->begin(), nodes->end(), [&userdata](const FSTBuilderNode& node) {
        return node.m_user_data == userdata;
      });
  if (it == nodes->end() || !it->IsFile())
    return data;

  DiscContentContainer tmp;
  for (auto& content : it->GetFileContent())
    tmp.Add(content.m_offset, content.m_size, std::move(content.m_source));
  data.resize(it->m_size);
  tmp.Read(0, it->m_size, data.data(), blob);
  return data;
}

DirectoryBlobPartition::DirectoryBlobPartition(
    DiscIO::VolumeDisc* volume, const DiscIO::Partition& partition, std::optional<bool> is_wii,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes)>& sys_callback,
    const std::function<void(std::vector<FSTBuilderNode>* fst_nodes, FSTBuilderNode* dol_node)>&
        fst_callback,
    DirectoryBlobReader* blob)
    : m_wrapped_partition(partition)
{
  std::vector<FSTBuilderNode> sys_nodes;

  std::vector<u8> disc_header(DISCHEADER_SIZE);
  if (!volume->Read(DISCHEADER_ADDRESS, DISCHEADER_SIZE, disc_header.data(), partition))
    disc_header.clear();
  sys_nodes.emplace_back(BuildSingleFileNode("boot.bin", std::move(disc_header), &disc_header));

  std::vector<u8> bi2(BI2_SIZE);
  if (!volume->Read(BI2_ADDRESS, BI2_SIZE, bi2.data(), partition))
    bi2.clear();
  sys_nodes.emplace_back(BuildSingleFileNode("bi2.bin", std::move(bi2), &bi2));

  std::vector<u8> apploader;
  const auto apploader_size = GetApploaderSize(*volume, partition);
  auto& apploader_node = sys_nodes.emplace_back(FSTBuilderNode{"apploader.img", 0, {}, &apploader});
  if (apploader_size)
  {
    apploader.resize(*apploader_size);
    if (!volume->Read(APPLOADER_ADDRESS, *apploader_size, apploader.data(), partition))
      apploader.clear();
    FillSingleFileNode(&apploader_node, std::move(apploader));
  }

  if (sys_callback)
    sys_callback(&sys_nodes);

  disc_header = ExtractNodeToVector(&sys_nodes, &disc_header, blob);
  disc_header.resize(DISCHEADER_SIZE);
  SetDiscType(is_wii, disc_header);
  SetBI2(ExtractNodeToVector(&sys_nodes, &bi2, blob));
  const u64 new_dol_address =
      SetApploader(ExtractNodeToVector(&sys_nodes, &apploader, blob), "apploader");

  FSTBuilderNode dol_node{"main.dol", 0, {}};
  if (const auto dol_offset = GetBootDOLOffset(*volume, partition))
  {
    if (const auto dol_size = GetBootDOLSize(*volume, partition, *dol_offset))
    {
      std::vector<BuilderContentSource> dol_contents;
      dol_contents.emplace_back(
          BuilderContentSource{0, *dol_size, ContentVolume{*dol_offset, partition}});
      dol_node.m_size = *dol_size;
      dol_node.m_content = std::move(dol_contents);
    }
  }

  std::vector<FSTBuilderNode> nodes;

  const FileSystem* fs = volume->GetFileSystem(partition);
  if (fs && fs->IsValid())
    GenerateBuilderNodesFromFileSystem(*volume, partition, &nodes, fs->GetRoot());

  if (fst_callback)
    fst_callback(&nodes, &dol_node);

  const u64 new_fst_address = SetDOL(std::move(dol_node), new_dol_address, &disc_header);
  BuildFST(std::move(nodes), new_fst_address, &disc_header);

  m_contents.Add(DISCHEADER_ADDRESS, disc_header);
}

void DirectoryBlobPartition::SetDiscType(std::optional<bool> is_wii,
                                         const std::vector<u8>& disc_header)
{
  if (is_wii.has_value())
  {
    m_is_wii = *is_wii;
  }
  else
  {
    m_is_wii = Common::swap32(&disc_header[0x18]) == WII_DISC_MAGIC;
    const bool is_gc = Common::swap32(&disc_header[0x1c]) == GAMECUBE_DISC_MAGIC;
    if (m_is_wii == is_gc)
    {
      ERROR_LOG_FMT(DISCIO, "Couldn't detect disc type based on disc header; assuming {}",
                    m_is_wii ? "Wii" : "GameCube");
    }
  }

  m_address_shift = m_is_wii ? 2 : 0;
}

void DirectoryBlobPartition::SetBI2FromFile(const std::string& bi2_path)
{
  std::vector<u8> bi2(BI2_SIZE);

  if (!m_is_wii)
    Write32(INVALID_REGION, 0x18, &bi2);

  const size_t bytes_read = ReadFileToVector(bi2_path, &bi2);
  if (!m_is_wii && bytes_read < 0x1C)
    ERROR_LOG_FMT(DISCIO, "Couldn't read region from {}", bi2_path);

  m_contents.Add(BI2_ADDRESS, std::move(bi2));
}

void DirectoryBlobPartition::SetBI2(std::vector<u8> bi2)
{
  const size_t bi2_size = bi2.size();
  bi2.resize(BI2_SIZE);

  if (!m_is_wii && bi2_size < 0x1C)
    Write32(INVALID_REGION, 0x18, &bi2);

  m_contents.Add(BI2_ADDRESS, std::move(bi2));
}

u64 DirectoryBlobPartition::SetApploaderFromFile(const std::string& path)
{
  File::IOFile file(path, "rb");
  std::vector<u8> apploader(file.GetSize());
  file.ReadBytes(apploader.data(), apploader.size());
  return SetApploader(std::move(apploader), path);
}

u64 DirectoryBlobPartition::SetApploader(std::vector<u8> apploader, const std::string& log_path)
{
  bool success = false;

  if (apploader.size() < 0x20)
  {
    ERROR_LOG_FMT(DISCIO, "{} couldn't be accessed or is too small", log_path);
  }
  else
  {
    const size_t apploader_size =
        0x20 + Common::swap32(*(u32*)&apploader[0x14]) + Common::swap32(*(u32*)&apploader[0x18]);
    if (apploader_size != apploader.size())
      ERROR_LOG_FMT(DISCIO, "{} is the wrong size... Is it really an apploader?", log_path);
    else
      success = true;
  }

  if (!success)
  {
    apploader.resize(0x20);
    // Make sure BS2 HLE doesn't try to run the apploader
    Write32(static_cast<u32>(-1), 0x10, &apploader);
  }

  size_t apploader_size = apploader.size();
  m_contents.Add(APPLOADER_ADDRESS, std::move(apploader));

  // Return DOL address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(APPLOADER_ADDRESS + apploader_size + 0x20, 0x20ull);
}

u64 DirectoryBlobPartition::SetDOLFromFile(const std::string& path, u64 dol_address,
                                           std::vector<u8>* disc_header)
{
  const u64 dol_size = m_contents.CheckSizeAndAdd(dol_address, path);

  Write32(static_cast<u32>(dol_address >> m_address_shift), 0x0420, disc_header);

  // Return FST address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(dol_address + dol_size + 0x20, 0x20ull);
}

u64 DirectoryBlobPartition::SetDOL(FSTBuilderNode dol_node, u64 dol_address,
                                   std::vector<u8>* disc_header)
{
  for (auto& content : dol_node.GetFileContent())
    m_contents.Add(dol_address + content.m_offset, content.m_size, std::move(content.m_source));

  Write32(static_cast<u32>(dol_address >> m_address_shift), 0x0420, disc_header);

  // Return FST address, 32 byte aligned (plus 32 byte padding)
  return Common::AlignUp(dol_address + dol_node.m_size + 0x20, 0x20ull);
}

static std::vector<FSTBuilderNode> ConvertFSTEntriesToBuilderNodes(const File::FSTEntry& parent)
{
  std::vector<FSTBuilderNode> nodes;
  nodes.reserve(parent.children.size());
  for (const File::FSTEntry& entry : parent.children)
  {
    std::variant<std::vector<BuilderContentSource>, std::vector<FSTBuilderNode>> content;
    if (entry.isDirectory)
    {
      content = ConvertFSTEntriesToBuilderNodes(entry);
    }
    else
    {
      content =
          std::vector<BuilderContentSource>{{0, entry.size, ContentFile{entry.physicalName, 0}}};
    }

    nodes.emplace_back(FSTBuilderNode{entry.virtualName, entry.size, std::move(content)});
  }
  return nodes;
}

void DirectoryBlobPartition::BuildFSTFromFolder(const std::string& fst_root_path, u64 fst_address,
                                                std::vector<u8>* disc_header)
{
  auto nodes = ConvertFSTEntriesToBuilderNodes(File::ScanDirectoryTree(fst_root_path, true));
  BuildFST(std::move(nodes), fst_address, disc_header);
}

static void ConvertUTF8NamesToSHIFTJIS(std::vector<FSTBuilderNode>* fst)
{
  for (FSTBuilderNode& entry : *fst)
  {
    if (entry.IsFolder())
      ConvertUTF8NamesToSHIFTJIS(&entry.GetFolderContent());
    entry.m_filename = UTF8ToSHIFTJIS(entry.m_filename);
  }
}

static u32 ComputeNameSize(const std::vector<FSTBuilderNode>& files)
{
  u32 name_size = 0;
  for (const FSTBuilderNode& entry : files)
  {
    if (entry.IsFolder())
      name_size += ComputeNameSize(entry.GetFolderContent());
    name_size += static_cast<u32>(entry.m_filename.length() + 1);
  }
  return name_size;
}

static size_t RecalculateFolderSizes(std::vector<FSTBuilderNode>* fst)
{
  size_t size = 0;
  for (FSTBuilderNode& entry : *fst)
  {
    ++size;
    if (entry.IsFile())
      continue;

    entry.m_size = RecalculateFolderSizes(&entry.GetFolderContent());
    size += entry.m_size;
  }
  return size;
}

void DirectoryBlobPartition::BuildFST(std::vector<FSTBuilderNode> root_nodes, u64 fst_address,
                                      std::vector<u8>* disc_header)
{
  ConvertUTF8NamesToSHIFTJIS(&root_nodes);

  u32 name_table_size = Common::AlignUp(ComputeNameSize(root_nodes), 1ull << m_address_shift);

  // 1 extra for the root entry
  u64 total_entries = RecalculateFolderSizes(&root_nodes) + 1;

  const u64 name_table_offset = total_entries * ENTRY_SIZE;
  std::vector<u8> fst_data(name_table_offset + name_table_size);

  // 32 KiB aligned start of data on disc
  u64 current_data_address = Common::AlignUp(fst_address + fst_data.size(), 0x8000ull);

  u32 fst_offset = 0;   // Offset within FST data
  u32 name_offset = 0;  // Offset within name table
  u32 root_offset = 0;  // Offset of root of FST

  // write root entry
  WriteEntryData(&fst_data, &fst_offset, DIRECTORY_ENTRY, 0, 0, total_entries, m_address_shift);

  WriteDirectory(&fst_data, &root_nodes, &fst_offset, &name_offset, &current_data_address,
                 root_offset, name_table_offset);

  // overflow check, compare the aligned name offset with the aligned name table size
  ASSERT(Common::AlignUp(name_offset, 1ull << m_address_shift) == name_table_size);

  // write FST size and location
  Write32((u32)(fst_address >> m_address_shift), 0x0424, disc_header);
  Write32((u32)(fst_data.size() >> m_address_shift), 0x0428, disc_header);
  Write32((u32)(fst_data.size() >> m_address_shift), 0x042c, disc_header);

  m_contents.Add(fst_address, std::move(fst_data));

  m_data_size = current_data_address;
}

void DirectoryBlobPartition::WriteEntryData(std::vector<u8>* fst_data, u32* entry_offset, u8 type,
                                            u32 name_offset, u64 data_offset, u64 length,
                                            u32 address_shift)
{
  (*fst_data)[(*entry_offset)++] = type;

  (*fst_data)[(*entry_offset)++] = (name_offset >> 16) & 0xff;
  (*fst_data)[(*entry_offset)++] = (name_offset >> 8) & 0xff;
  (*fst_data)[(*entry_offset)++] = (name_offset)&0xff;

  Write32((u32)(data_offset >> address_shift), *entry_offset, fst_data);
  *entry_offset += 4;

  Write32((u32)length, *entry_offset, fst_data);
  *entry_offset += 4;
}

void DirectoryBlobPartition::WriteEntryName(std::vector<u8>* fst_data, u32* name_offset,
                                            const std::string& name, u64 name_table_offset)
{
  strncpy((char*)&(*fst_data)[*name_offset + name_table_offset], name.c_str(), name.length() + 1);

  *name_offset += (u32)(name.length() + 1);
}

void DirectoryBlobPartition::WriteDirectory(std::vector<u8>* fst_data,
                                            std::vector<FSTBuilderNode>* parent_entries,
                                            u32* fst_offset, u32* name_offset, u64* data_offset,
                                            u32 parent_entry_index, u64 name_table_offset)
{
  std::vector<FSTBuilderNode>& sorted_entries = *parent_entries;

  // Sort for determinism
  std::sort(sorted_entries.begin(), sorted_entries.end(),
            [](const FSTBuilderNode& one, const FSTBuilderNode& two) {
              std::string one_upper = one.m_filename;
              std::string two_upper = two.m_filename;
              Common::ToUpper(&one_upper);
              Common::ToUpper(&two_upper);
              return one_upper == two_upper ? one.m_filename < two.m_filename :
                                              one_upper < two_upper;
            });

  for (FSTBuilderNode& entry : sorted_entries)
  {
    if (entry.IsFolder())
    {
      u32 entry_index = *fst_offset / ENTRY_SIZE;
      WriteEntryData(fst_data, fst_offset, DIRECTORY_ENTRY, *name_offset, parent_entry_index,
                     entry_index + entry.m_size + 1, 0);
      WriteEntryName(fst_data, name_offset, entry.m_filename, name_table_offset);

      auto& child_nodes = entry.GetFolderContent();
      WriteDirectory(fst_data, &child_nodes, fst_offset, name_offset, data_offset, entry_index,
                     name_table_offset);
    }
    else
    {
      // put entry in FST
      WriteEntryData(fst_data, fst_offset, FILE_ENTRY, *name_offset, *data_offset, entry.m_size,
                     m_address_shift);
      WriteEntryName(fst_data, name_offset, entry.m_filename, name_table_offset);

      // write entry to virtual disc
      auto& contents = entry.GetFileContent();
      for (BuilderContentSource& content : contents)
      {
        m_contents.Add(*data_offset + content.m_offset, content.m_size,
                       std::move(content.m_source));
      }

      // 32 KiB aligned - many games are fine with less alignment, but not all
      *data_offset = Common::AlignUp(*data_offset + entry.m_size, 0x8000ull);
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
}  // namespace DiscIO
