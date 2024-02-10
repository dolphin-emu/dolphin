// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/GCMemcardUtils.h"

#include <array>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/NandPaths.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

namespace Memcard
{
constexpr u32 GCI_HEADER_SIZE = DENTRY_SIZE;
constexpr std::array<u8, 12> SAV_MAGIC = {0x44, 0x41, 0x54, 0x45, 0x4C, 0x47,
                                          0x43, 0x5F, 0x53, 0x41, 0x56, 0x45};  // "DATELGC_SAVE"
constexpr u32 SAV_HEADER_SIZE = 0xC0;
constexpr u32 SAV_DENTRY_OFFSET = 0x80;
constexpr std::array<u8, 6> GCS_MAGIC = {0x47, 0x43, 0x53, 0x41, 0x56, 0x45};  // "GCSAVE"
constexpr u32 GCS_HEADER_SIZE = 0x150;
constexpr u32 GCS_DENTRY_OFFSET = 0x110;

bool HasSameIdentity(const DEntry& lhs, const DEntry& rhs)
{
  // The GameCube BIOS identifies two files as being 'the same' (that is, disallows copying from one
  // card to another when both contain a file like it) when the full array of all of m_gamecode,
  // m_makercode, and m_filename match between them.

  // However, despite that, it seems like the m_filename should be treated as a nullterminated
  // string instead, because:
  // - Games seem to identify their saves regardless of what bytes appear after the first null byte.
  // - If you have two files that match except for bytes after the first null in m_filename, the
  //   BIOS behaves oddly if you attempt to copy the files, as it seems to clear out those extra
  //   non-null bytes. See below for details.

  // Specifically, the following chain of actions fails with a rather vague 'The data may not have
  // been copied.' error message:
  // - Have two memory cards with one save file each.
  // - The two save files should have identical gamecode and makercode, as well as an equivalent
  //   filename up until and including the first null byte.
  // - On Card A have all remaining bytes of the filename also be null.
  // - On Card B have at least one of the remaining bytes be non-null.
  // - Copy the file on Card B to Card A.
  // The BIOS will abort halfway through the copy process and declare Card B as unusable until you
  // eject and reinsert it, and leave a "Broken File000" file on Card A, though note that the file
  // is not visible and will be cleaned up when reinserting the card while still within the BIOS.
  // Additionally, either during or after the copy process, the bytes after the first null on Card B
  // are changed to null, which is presumably why the copy process ends up failing as Card A would
  // then have two identical files. For reference, the Wii System Menu behaves exactly the same.

  // With all that in mind, even if it mismatches the comparison behavior of the BIOS, we treat
  // m_filename as a nullterminated string for determining if two files identify as the same, as not
  // doing so would cause more harm and confusion than good in practice.

  if (lhs.m_gamecode != rhs.m_gamecode)
    return false;
  if (lhs.m_makercode != rhs.m_makercode)
    return false;

  for (size_t i = 0; i < lhs.m_filename.size(); ++i)
  {
    const u8 a = lhs.m_filename[i];
    const u8 b = rhs.m_filename[i];
    if (a == 0)
      return b == 0;
    if (a != b)
      return false;
  }

  return true;
}

bool HasDuplicateIdentity(std::span<const Savefile> savefiles)
{
  for (size_t i = 0; i < savefiles.size(); ++i)
  {
    for (size_t j = i + 1; j < savefiles.size(); ++j)
    {
      if (HasSameIdentity(savefiles[i].dir_entry, savefiles[j].dir_entry))
        return true;
    }
  }
  return false;
}

static void ByteswapDEntrySavHeader(std::array<u8, DENTRY_SIZE>& entry)
{
  // several bytes in SAV are swapped compared to the internal memory card format
  for (size_t p : {0x06, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E})
    std::swap(entry[p], entry[p + 1]);
}

static DEntry ExtractDEntryFromSavHeader(const std::array<u8, SAV_HEADER_SIZE>& sav_header)
{
  std::array<u8, DENTRY_SIZE> entry;
  std::memcpy(entry.data(), &sav_header[SAV_DENTRY_OFFSET], DENTRY_SIZE);
  ByteswapDEntrySavHeader(entry);

  DEntry dir_entry;
  std::memcpy(&dir_entry, entry.data(), DENTRY_SIZE);
  return dir_entry;
}

static void InjectDEntryToSavHeader(std::array<u8, SAV_HEADER_SIZE>& sav_header,
                                    const DEntry& dir_entry)
{
  std::array<u8, DENTRY_SIZE> entry;
  std::memcpy(entry.data(), &dir_entry, DENTRY_SIZE);
  ByteswapDEntrySavHeader(entry);
  std::memcpy(&sav_header[SAV_DENTRY_OFFSET], entry.data(), DENTRY_SIZE);
}

static bool ReadBlocksFromIOFile(File::IOFile& file, std::vector<GCMBlock>& blocks,
                                 size_t block_count)
{
  blocks.reserve(block_count);
  for (size_t i = 0; i < block_count; ++i)
  {
    GCMBlock& block = blocks.emplace_back();
    if (!file.ReadBytes(block.m_block.data(), block.m_block.size()))
      return false;
  }
  return true;
}

static std::variant<ReadSavefileErrorCode, Savefile> ReadSavefileInternalGCI(File::IOFile& file,
                                                                             u64 filesize)
{
  Savefile savefile;
  if (!file.ReadBytes(&savefile.dir_entry, DENTRY_SIZE))
    return ReadSavefileErrorCode::IOError;

  const size_t block_count = savefile.dir_entry.m_block_count;
  const u64 expected_size = DENTRY_SIZE + block_count * BLOCK_SIZE;
  if (expected_size != filesize)
    return ReadSavefileErrorCode::DataCorrupted;

  if (!ReadBlocksFromIOFile(file, savefile.blocks, block_count))
    return ReadSavefileErrorCode::IOError;

  return savefile;
}

static std::variant<ReadSavefileErrorCode, Savefile> ReadSavefileInternalGCS(File::IOFile& file,
                                                                             u64 filesize)
{
  std::array<u8, GCS_HEADER_SIZE> gcs_header;
  if (!file.ReadBytes(gcs_header.data(), gcs_header.size()))
    return ReadSavefileErrorCode::IOError;

  if (std::memcmp(gcs_header.data(), GCS_MAGIC.data(), GCS_MAGIC.size()) != 0)
    return ReadSavefileErrorCode::DataCorrupted;

  Savefile savefile;
  std::memcpy(&savefile.dir_entry, &gcs_header[GCS_DENTRY_OFFSET], DENTRY_SIZE);

  // field containing the Block count as displayed within
  // the GameSaves software is not stored in the GCS file.
  // It is stored only within the corresponding GSV file.
  // If the GCS file is added without using the GameSaves software,
  // the value stored is always "1"

  // to get the actual block count calculate backwards from the filesize
  const u64 total_block_size = filesize - GCS_HEADER_SIZE;
  if ((total_block_size % BLOCK_SIZE) != 0)
    return ReadSavefileErrorCode::DataCorrupted;

  const size_t block_count = total_block_size / BLOCK_SIZE;
  savefile.dir_entry.m_block_count = static_cast<u16>(block_count);

  if (!ReadBlocksFromIOFile(file, savefile.blocks, block_count))
    return ReadSavefileErrorCode::IOError;

  return savefile;
}

static std::variant<ReadSavefileErrorCode, Savefile> ReadSavefileInternalSAV(File::IOFile& file,
                                                                             u64 filesize)
{
  std::array<u8, SAV_HEADER_SIZE> sav_header;
  if (!file.ReadBytes(sav_header.data(), sav_header.size()))
    return ReadSavefileErrorCode::IOError;

  if (std::memcmp(sav_header.data(), SAV_MAGIC.data(), SAV_MAGIC.size()) != 0)
    return ReadSavefileErrorCode::DataCorrupted;

  Savefile savefile;
  savefile.dir_entry = ExtractDEntryFromSavHeader(sav_header);

  const size_t block_count = savefile.dir_entry.m_block_count;
  const u64 expected_size = SAV_HEADER_SIZE + block_count * BLOCK_SIZE;
  if (expected_size != filesize)
    return ReadSavefileErrorCode::DataCorrupted;

  if (!ReadBlocksFromIOFile(file, savefile.blocks, block_count))
    return ReadSavefileErrorCode::IOError;

  return savefile;
}

std::variant<ReadSavefileErrorCode, Savefile> ReadSavefile(const std::string& filename)
{
  File::IOFile file(filename, "rb");
  if (!file)
    return ReadSavefileErrorCode::OpenFileFail;

  // Since GCI, GCS and SAV all have different header lengths but the block size is always the same,
  // we can detect the type from the filesize.
  const u64 filesize = file.GetSize();
  const u64 header_size = filesize % BLOCK_SIZE;

  switch (header_size)
  {
  case GCI_HEADER_SIZE:
    return ReadSavefileInternalGCI(file, filesize);
  case GCS_HEADER_SIZE:
    return ReadSavefileInternalGCS(file, filesize);
  case SAV_HEADER_SIZE:
    return ReadSavefileInternalSAV(file, filesize);
  default:
    return ReadSavefileErrorCode::DataCorrupted;
  }
}

static bool WriteSavefileInternalGCI(File::IOFile& file, const Savefile& savefile)
{
  if (!file.WriteBytes(&savefile.dir_entry, DENTRY_SIZE))
    return false;

  for (const GCMBlock& block : savefile.blocks)
  {
    if (!file.WriteBytes(block.m_block.data(), block.m_block.size()))
      return false;
  }

  return file.IsGood();
}

static bool WriteSavefileInternalGCS(File::IOFile& file, const Savefile& savefile)
{
  std::array<u8, GCS_HEADER_SIZE> header;
  std::memset(header.data(), 0, header.size());
  std::memcpy(header.data(), GCS_MAGIC.data(), GCS_MAGIC.size());

  DEntry gcs_entry = savefile.dir_entry;
  gcs_entry.m_block_count = 1;  // always stored as 1 in GCS files
  std::memcpy(&header[GCS_DENTRY_OFFSET], &gcs_entry, DENTRY_SIZE);

  if (!file.WriteBytes(header.data(), header.size()))
    return false;

  for (const GCMBlock& block : savefile.blocks)
  {
    if (!file.WriteBytes(block.m_block.data(), block.m_block.size()))
      return false;
  }

  return file.IsGood();
}

static bool WriteSavefileInternalSAV(File::IOFile& file, const Savefile& savefile)
{
  std::array<u8, SAV_HEADER_SIZE> header;
  std::memset(header.data(), 0, header.size());
  std::memcpy(header.data(), SAV_MAGIC.data(), SAV_MAGIC.size());

  InjectDEntryToSavHeader(header, savefile.dir_entry);

  if (!file.WriteBytes(header.data(), header.size()))
    return false;

  for (const GCMBlock& block : savefile.blocks)
  {
    if (!file.WriteBytes(block.m_block.data(), block.m_block.size()))
      return false;
  }

  return file.IsGood();
}

bool WriteSavefile(const std::string& filename, const Savefile& savefile, SavefileFormat format)
{
  File::IOFile file(filename, "wb");
  if (!file)
    return false;

  switch (format)
  {
  case SavefileFormat::GCI:
    return WriteSavefileInternalGCI(file, savefile);
  case SavefileFormat::GCS:
    return WriteSavefileInternalGCS(file, savefile);
  case SavefileFormat::SAV:
    return WriteSavefileInternalSAV(file, savefile);
  default:
    return false;
  }
}

std::string GenerateFilename(const DEntry& entry)
{
  std::string maker(reinterpret_cast<const char*>(entry.m_makercode.data()),
                    entry.m_makercode.size());
  std::string gamecode(reinterpret_cast<const char*>(entry.m_gamecode.data()),
                       entry.m_gamecode.size());

  // prevent going out of bounds when all bytes of m_filename are non-null
  size_t length = 0;
  for (size_t i = 0; i < entry.m_filename.size(); ++i)
  {
    if (entry.m_filename[i] == 0)
      break;
    ++length;
  }
  std::string filename(reinterpret_cast<const char*>(entry.m_filename.data()), length);

  return Common::EscapeFileName(maker + '-' + gamecode + '-' + filename);
}

std::string GetDefaultExtension(SavefileFormat format)
{
  switch (format)
  {
  case SavefileFormat::GCI:
    return ".gci";
  case SavefileFormat::GCS:
    return ".gcs";
  case SavefileFormat::SAV:
    return ".sav";
  default:
    ASSERT(false);
    return ".gci";
  }
}

std::vector<Savefile> GetSavefiles(const GCMemcard& card, std::span<const u8> file_indices)
{
  std::vector<Savefile> files;
  files.reserve(file_indices.size());
  for (const u8 index : file_indices)
  {
    std::optional<Savefile> file = card.ExportFile(index);
    if (!file)
      return {};
    files.emplace_back(std::move(*file));
  }
  return files;
}

size_t GetBlockCount(std::span<const Savefile> savefiles)
{
  size_t block_count = 0;
  for (const Savefile& savefile : savefiles)
    block_count += savefile.blocks.size();
  return block_count;
}
}  // namespace Memcard
