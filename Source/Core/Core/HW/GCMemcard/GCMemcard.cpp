// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GCMemcard/GCMemcard.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

static void ByteSwap(u8* valueA, u8* valueB)
{
  u8 tmp = *valueA;
  *valueA = *valueB;
  *valueB = tmp;
}

GCMemcard::GCMemcard(const std::string& filename, bool forceCreation, bool shift_jis)
    : m_valid(false), m_fileName(filename)
{
  // Currently there is a string freeze. instead of adding a new message about needing r/w
  // open file read only, if write is denied the error will be reported at that point
  File::IOFile mcdFile(m_fileName, "rb");
  if (!mcdFile.IsOpen())
  {
    if (!forceCreation)
    {
      if (!AskYesNoT("\"%s\" does not exist.\n Create a new 16MB Memory Card?", filename.c_str()))
        return;
      shift_jis =
          AskYesNoT("Format as Shift JIS (Japanese)?\nChoose no for Windows-1252 (Western)");
    }
    Format(shift_jis);
    return;
  }
  else
  {
    // This function can be removed once more about hdr is known and we can check for a valid header
    std::string fileType;
    SplitPath(filename, nullptr, nullptr, &fileType);
    if (strcasecmp(fileType.c_str(), ".raw") && strcasecmp(fileType.c_str(), ".gcp"))
    {
      PanicAlertT("File has the extension \"%s\".\nValid extensions are (.raw/.gcp)",
                  fileType.c_str());
      return;
    }
    auto size = mcdFile.GetSize();
    if (size < MC_FST_BLOCKS * BLOCK_SIZE)
    {
      PanicAlertT("%s failed to load as a memory card.\nFile is not large enough to be a valid "
                  "memory card file (0x%x bytes)",
                  filename.c_str(), (unsigned)size);
      return;
    }
    if (size % BLOCK_SIZE)
    {
      PanicAlertT("%s failed to load as a memory card.\nCard file size is invalid (0x%x bytes)",
                  filename.c_str(), (unsigned)size);
      return;
    }

    m_sizeMb = (u16)((size / BLOCK_SIZE) / MBIT_TO_BLOCKS);
    switch (m_sizeMb)
    {
    case MemCard59Mb:
    case MemCard123Mb:
    case MemCard251Mb:
    case Memcard507Mb:
    case MemCard1019Mb:
    case MemCard2043Mb:
      break;
    default:
      PanicAlertT("%s failed to load as a memory card.\nCard size is invalid (0x%x bytes)",
                  filename.c_str(), (unsigned)size);
      return;
    }
  }

  mcdFile.Seek(0, SEEK_SET);
  if (!mcdFile.ReadBytes(&hdr, BLOCK_SIZE))
  {
    PanicAlertT("Failed to read header correctly\n(0x0000-0x1FFF)");
    return;
  }
  if (m_sizeMb != BE16(hdr.SizeMb))
  {
    PanicAlertT("Memory card file size does not match the header size");
    return;
  }

  if (!mcdFile.ReadBytes(&dir, BLOCK_SIZE))
  {
    PanicAlertT("Failed to read directory correctly\n(0x2000-0x3FFF)");
    return;
  }

  if (!mcdFile.ReadBytes(&dir_backup, BLOCK_SIZE))
  {
    PanicAlertT("Failed to read directory backup correctly\n(0x4000-0x5FFF)");
    return;
  }

  if (!mcdFile.ReadBytes(&bat, BLOCK_SIZE))
  {
    PanicAlertT("Failed to read block allocation table correctly\n(0x6000-0x7FFF)");
    return;
  }

  if (!mcdFile.ReadBytes(&bat_backup, BLOCK_SIZE))
  {
    PanicAlertT("Failed to read block allocation table backup correctly\n(0x8000-0x9FFF)");
    return;
  }

  u32 csums = TestChecksums();

  if (csums & 0x1)
  {
    // header checksum error!
    // invalid files do not always get here
    PanicAlertT("Header checksum failed");
    return;
  }

  if (csums & 0x2)  // directory checksum error!
  {
    if (csums & 0x4)
    {
      // backup is also wrong!
      PanicAlertT("Directory checksum and directory backup checksum failed");
      return;
    }
    else
    {
      // backup is correct, restore
      dir = dir_backup;
      bat = bat_backup;

      // update checksums
      csums = TestChecksums();
    }
  }

  if (csums & 0x8)  // BAT checksum error!
  {
    if (csums & 0x10)
    {
      // backup is also wrong!
      PanicAlertT("Block Allocation Table checksum failed");
      return;
    }
    else
    {
      // backup is correct, restore
      dir = dir_backup;
      bat = bat_backup;

      // update checksums
      csums = TestChecksums();
    }
    // It seems that the backup having a larger counter doesn't necessarily mean
    // the backup should be copied?
    //	}
    //
    //	if (BE16(dir_backup.UpdateCounter) > BE16(dir.UpdateCounter)) //check if the backup is newer
    //	{
    //		dir = dir_backup;
    //		bat = bat_backup; // needed?
  }

  mcdFile.Seek(0xa000, SEEK_SET);

  maxBlock = (u32)m_sizeMb * MBIT_TO_BLOCKS;
  mc_data_blocks.reserve(maxBlock - MC_FST_BLOCKS);

  m_valid = true;
  for (u32 i = MC_FST_BLOCKS; i < maxBlock; ++i)
  {
    GCMBlock b;
    if (mcdFile.ReadBytes(b.block, BLOCK_SIZE))
    {
      mc_data_blocks.push_back(b);
    }
    else
    {
      PanicAlertT("Failed to read block %u of the save data\nMemory card may be truncated\nFile "
                  "position: 0x%" PRIx64,
                  i, mcdFile.Tell());
      m_valid = false;
      break;
    }
  }

  mcdFile.Close();

  InitDirBatPointers();
}

void GCMemcard::InitDirBatPointers()
{
  if (BE16(dir.UpdateCounter) > (BE16(dir_backup.UpdateCounter)))
  {
    CurrentDir = &dir;
    PreviousDir = &dir_backup;
  }
  else
  {
    CurrentDir = &dir_backup;
    PreviousDir = &dir;
  }
  if (BE16(bat.UpdateCounter) > BE16(bat_backup.UpdateCounter))
  {
    CurrentBat = &bat;
    PreviousBat = &bat_backup;
  }
  else
  {
    CurrentBat = &bat_backup;
    PreviousBat = &bat;
  }
}

bool GCMemcard::IsShiftJIS() const
{
  return hdr.Encoding != 0;
}

bool GCMemcard::Save()
{
  File::IOFile mcdFile(m_fileName, "wb");
  mcdFile.Seek(0, SEEK_SET);

  mcdFile.WriteBytes(&hdr, BLOCK_SIZE);
  mcdFile.WriteBytes(&dir, BLOCK_SIZE);
  mcdFile.WriteBytes(&dir_backup, BLOCK_SIZE);
  mcdFile.WriteBytes(&bat, BLOCK_SIZE);
  mcdFile.WriteBytes(&bat_backup, BLOCK_SIZE);
  for (unsigned int i = 0; i < maxBlock - MC_FST_BLOCKS; ++i)
  {
    mcdFile.WriteBytes(mc_data_blocks[i].block, BLOCK_SIZE);
  }

  return mcdFile.Close();
}

void calc_checksumsBE(const u16* buf, u32 length, u16* csum, u16* inv_csum)
{
  *csum = *inv_csum = 0;

  for (u32 i = 0; i < length; ++i)
  {
    // weird warnings here
    *csum += BE16(buf[i]);
    *inv_csum += BE16((u16)(buf[i] ^ 0xffff));
  }
  *csum = BE16(*csum);
  *inv_csum = BE16(*inv_csum);
  if (*csum == 0xffff)
  {
    *csum = 0;
  }
  if (*inv_csum == 0xffff)
  {
    *inv_csum = 0;
  }
}

u32 GCMemcard::TestChecksums() const
{
  u16 csum = 0, csum_inv = 0;

  u32 results = 0;

  calc_checksumsBE((u16*)&hdr, 0xFE, &csum, &csum_inv);
  if ((hdr.Checksum != csum) || (hdr.Checksum_Inv != csum_inv))
    results |= 1;

  calc_checksumsBE((u16*)&dir, 0xFFE, &csum, &csum_inv);
  if ((dir.Checksum != csum) || (dir.Checksum_Inv != csum_inv))
    results |= 2;

  calc_checksumsBE((u16*)&dir_backup, 0xFFE, &csum, &csum_inv);
  if ((dir_backup.Checksum != csum) || (dir_backup.Checksum_Inv != csum_inv))
    results |= 4;

  calc_checksumsBE((u16*)(((u8*)&bat) + 4), 0xFFE, &csum, &csum_inv);
  if ((bat.Checksum != csum) || (bat.Checksum_Inv != csum_inv))
    results |= 8;

  calc_checksumsBE((u16*)(((u8*)&bat_backup) + 4), 0xFFE, &csum, &csum_inv);
  if ((bat_backup.Checksum != csum) || (bat_backup.Checksum_Inv != csum_inv))
    results |= 16;

  return results;
}

bool GCMemcard::FixChecksums()
{
  if (!m_valid)
    return false;

  calc_checksumsBE((u16*)&hdr, 0xFE, &hdr.Checksum, &hdr.Checksum_Inv);
  calc_checksumsBE((u16*)&dir, 0xFFE, &dir.Checksum, &dir.Checksum_Inv);
  calc_checksumsBE((u16*)&dir_backup, 0xFFE, &dir_backup.Checksum, &dir_backup.Checksum_Inv);
  calc_checksumsBE((u16*)&bat + 2, 0xFFE, &bat.Checksum, &bat.Checksum_Inv);
  calc_checksumsBE((u16*)&bat_backup + 2, 0xFFE, &bat_backup.Checksum, &bat_backup.Checksum_Inv);

  return true;
}

u8 GCMemcard::GetNumFiles() const
{
  if (!m_valid)
    return 0;

  u8 j = 0;
  for (int i = 0; i < DIRLEN; i++)
  {
    if (BE32(CurrentDir->Dir[i].Gamecode) != 0xFFFFFFFF)
      j++;
  }
  return j;
}

u8 GCMemcard::GetFileIndex(u8 fileNumber) const
{
  if (m_valid)
  {
    u8 j = 0;
    for (u8 i = 0; i < DIRLEN; i++)
    {
      if (BE32(CurrentDir->Dir[i].Gamecode) != 0xFFFFFFFF)
      {
        if (j == fileNumber)
        {
          return i;
        }
        j++;
      }
    }
  }
  return 0xFF;
}

u16 GCMemcard::GetFreeBlocks() const
{
  if (!m_valid)
    return 0;

  return BE16(CurrentBat->FreeBlocks);
}

u8 GCMemcard::TitlePresent(const DEntry& d) const
{
  if (!m_valid)
    return DIRLEN;

  u8 i = 0;
  while (i < DIRLEN)
  {
    if ((BE32(CurrentDir->Dir[i].Gamecode) == BE32(d.Gamecode)) &&
        (!memcmp(CurrentDir->Dir[i].Filename, d.Filename, 32)))
    {
      break;
    }
    i++;
  }
  return i;
}

bool GCMemcard::GCI_FileName(u8 index, std::string& filename) const
{
  if (!m_valid || index >= DIRLEN || (BE32(CurrentDir->Dir[index].Gamecode) == 0xFFFFFFFF))
    return false;

  filename = CurrentDir->Dir[index].GCI_FileName();
  return true;
}

// DEntry functions, all take u8 index < DIRLEN (127)
// Functions that have ascii output take a char *buffer

std::string GCMemcard::DEntry_GameCode(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  return std::string((const char*)CurrentDir->Dir[index].Gamecode, 4);
}

std::string GCMemcard::DEntry_Makercode(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  return std::string((const char*)CurrentDir->Dir[index].Makercode, 2);
}

std::string GCMemcard::DEntry_BIFlags(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  std::string flags;
  int x = CurrentDir->Dir[index].BIFlags;
  for (int i = 0; i < 8; i++)
  {
    flags.push_back((x & 0x80) ? '1' : '0');
    x = x << 1;
  }
  return flags;
}

std::string GCMemcard::DEntry_FileName(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  return std::string((const char*)CurrentDir->Dir[index].Filename, DENTRY_STRLEN);
}

u32 GCMemcard::DEntry_ModTime(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFFFFFFFF;

  return BE32(CurrentDir->Dir[index].ModTime);
}

u32 GCMemcard::DEntry_ImageOffset(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFFFFFFFF;

  return BE32(CurrentDir->Dir[index].ImageOffset);
}

std::string GCMemcard::DEntry_IconFmt(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  int x = CurrentDir->Dir[index].IconFmt[0];
  std::string format;
  for (int i = 0; i < 16; i++)
  {
    if (i == 8)
      x = CurrentDir->Dir[index].IconFmt[1];
    format.push_back((x & 0x80) ? '1' : '0');
    x = x << 1;
  }
  return format;
}

std::string GCMemcard::DEntry_AnimSpeed(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  int x = CurrentDir->Dir[index].AnimSpeed[0];
  std::string speed;
  for (int i = 0; i < 16; i++)
  {
    if (i == 8)
      x = CurrentDir->Dir[index].AnimSpeed[1];
    speed.push_back((x & 0x80) ? '1' : '0');
    x = x << 1;
  }
  return speed;
}

std::string GCMemcard::DEntry_Permissions(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  u8 Permissions = CurrentDir->Dir[index].Permissions;
  std::string permissionsString;
  permissionsString.push_back((Permissions & 16) ? 'x' : 'M');
  permissionsString.push_back((Permissions & 8) ? 'x' : 'C');
  permissionsString.push_back((Permissions & 4) ? 'P' : 'x');
  return permissionsString;
}

u8 GCMemcard::DEntry_CopyCounter(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFF;

  return CurrentDir->Dir[index].CopyCounter;
}

u16 GCMemcard::DEntry_FirstBlock(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFFFF;

  u16 block = BE16(CurrentDir->Dir[index].FirstBlock);
  if (block > (u16)maxBlock)
    return 0xFFFF;
  return block;
}

u16 GCMemcard::DEntry_BlockCount(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFFFF;

  u16 blocks = BE16(CurrentDir->Dir[index].BlockCount);
  if (blocks > (u16)maxBlock)
    return 0xFFFF;
  return blocks;
}

u32 GCMemcard::DEntry_CommentsAddress(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return 0xFFFF;

  return BE32(CurrentDir->Dir[index].CommentsAddr);
}

std::string GCMemcard::GetSaveComment1(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  u32 Comment1 = BE32(CurrentDir->Dir[index].CommentsAddr);
  u32 DataBlock = BE16(CurrentDir->Dir[index].FirstBlock) - MC_FST_BLOCKS;
  if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
  {
    return "";
  }
  return std::string((const char*)mc_data_blocks[DataBlock].block + Comment1, DENTRY_STRLEN);
}

std::string GCMemcard::GetSaveComment2(u8 index) const
{
  if (!m_valid || index >= DIRLEN)
    return "";

  u32 Comment1 = BE32(CurrentDir->Dir[index].CommentsAddr);
  u32 Comment2 = Comment1 + DENTRY_STRLEN;
  u32 DataBlock = BE16(CurrentDir->Dir[index].FirstBlock) - MC_FST_BLOCKS;
  if ((DataBlock > maxBlock) || (Comment1 == 0xFFFFFFFF))
  {
    return "";
  }
  return std::string((const char*)mc_data_blocks[DataBlock].block + Comment2, DENTRY_STRLEN);
}

bool GCMemcard::GetDEntry(u8 index, DEntry& dest) const
{
  if (!m_valid || index >= DIRLEN)
    return false;

  dest = CurrentDir->Dir[index];
  return true;
}

u16 BlockAlloc::GetNextBlock(u16 Block) const
{
  if ((Block < MC_FST_BLOCKS) || (Block > 4091))
    return 0;

  return Common::swap16(Map[Block - MC_FST_BLOCKS]);
}

u16 BlockAlloc::NextFreeBlock(u16 MaxBlock, u16 StartingBlock) const
{
  if (FreeBlocks)
  {
    MaxBlock = std::min<u16>(MaxBlock, BAT_SIZE);
    for (u16 i = StartingBlock; i < MaxBlock; ++i)
      if (Map[i - MC_FST_BLOCKS] == 0)
        return i;

    for (u16 i = MC_FST_BLOCKS; i < StartingBlock; ++i)
      if (Map[i - MC_FST_BLOCKS] == 0)
        return i;
  }
  return 0xFFFF;
}

bool BlockAlloc::ClearBlocks(u16 FirstBlock, u16 BlockCount)
{
  std::vector<u16> blocks;
  while (FirstBlock != 0xFFFF && FirstBlock != 0)
  {
    blocks.push_back(FirstBlock);
    FirstBlock = GetNextBlock(FirstBlock);
  }
  if (FirstBlock > 0)
  {
    size_t length = blocks.size();
    if (length != BlockCount)
    {
      return false;
    }
    for (unsigned int i = 0; i < length; ++i)
      Map[blocks.at(i) - MC_FST_BLOCKS] = 0;
    FreeBlocks = BE16(BE16(FreeBlocks) + BlockCount);

    return true;
  }
  return false;
}

u32 GCMemcard::GetSaveData(u8 index, std::vector<GCMBlock>& Blocks) const
{
  if (!m_valid)
    return NOMEMCARD;

  u16 block = DEntry_FirstBlock(index);
  u16 BlockCount = DEntry_BlockCount(index);
  // u16 memcardSize = BE16(hdr.SizeMb) * MBIT_TO_BLOCKS;

  if ((block == 0xFFFF) || (BlockCount == 0xFFFF))
  {
    return FAIL;
  }

  u16 nextBlock = block;
  for (int i = 0; i < BlockCount; ++i)
  {
    if ((!nextBlock) || (nextBlock == 0xFFFF))
      return FAIL;
    Blocks.push_back(mc_data_blocks[nextBlock - MC_FST_BLOCKS]);
    nextBlock = CurrentBat->GetNextBlock(nextBlock);
  }
  return SUCCESS;
}
// End DEntry functions

u32 GCMemcard::ImportFile(const DEntry& direntry, std::vector<GCMBlock>& saveBlocks)
{
  if (!m_valid)
    return NOMEMCARD;

  if (GetNumFiles() >= DIRLEN)
  {
    return OUTOFDIRENTRIES;
  }
  if (BE16(CurrentBat->FreeBlocks) < BE16(direntry.BlockCount))
  {
    return OUTOFBLOCKS;
  }
  if (TitlePresent(direntry) != DIRLEN)
  {
    return TITLEPRESENT;
  }

  // find first free data block
  u16 firstBlock =
      CurrentBat->NextFreeBlock(maxBlock - MC_FST_BLOCKS, BE16(CurrentBat->LastAllocated));
  if (firstBlock == 0xFFFF)
    return OUTOFBLOCKS;
  Directory UpdatedDir = *CurrentDir;

  // find first free dir entry
  for (int i = 0; i < DIRLEN; i++)
  {
    if (BE32(UpdatedDir.Dir[i].Gamecode) == 0xFFFFFFFF)
    {
      UpdatedDir.Dir[i] = direntry;
      *(u16*)&UpdatedDir.Dir[i].FirstBlock = BE16(firstBlock);
      UpdatedDir.Dir[i].CopyCounter = UpdatedDir.Dir[i].CopyCounter + 1;
      break;
    }
  }
  UpdatedDir.UpdateCounter = BE16(BE16(UpdatedDir.UpdateCounter) + 1);
  *PreviousDir = UpdatedDir;
  if (PreviousDir == &dir)
  {
    CurrentDir = &dir;
    PreviousDir = &dir_backup;
  }
  else
  {
    CurrentDir = &dir_backup;
    PreviousDir = &dir;
  }

  int fileBlocks = BE16(direntry.BlockCount);

  FZEROGX_MakeSaveGameValid(hdr, direntry, saveBlocks);
  PSO_MakeSaveGameValid(hdr, direntry, saveBlocks);

  BlockAlloc UpdatedBat = *CurrentBat;
  u16 nextBlock;
  // keep assuming no freespace fragmentation, and copy over all the data
  for (int i = 0; i < fileBlocks; ++i)
  {
    if (firstBlock == 0xFFFF)
      PanicAlert("Fatal Error");
    mc_data_blocks[firstBlock - MC_FST_BLOCKS] = saveBlocks[i];
    if (i == fileBlocks - 1)
      nextBlock = 0xFFFF;
    else
      nextBlock = UpdatedBat.NextFreeBlock(maxBlock - MC_FST_BLOCKS, firstBlock + 1);
    UpdatedBat.Map[firstBlock - MC_FST_BLOCKS] = BE16(nextBlock);
    UpdatedBat.LastAllocated = BE16(firstBlock);
    firstBlock = nextBlock;
  }

  UpdatedBat.FreeBlocks = BE16(BE16(UpdatedBat.FreeBlocks) - fileBlocks);
  UpdatedBat.UpdateCounter = BE16(BE16(UpdatedBat.UpdateCounter) + 1);
  *PreviousBat = UpdatedBat;
  if (PreviousBat == &bat)
  {
    CurrentBat = &bat;
    PreviousBat = &bat_backup;
  }
  else
  {
    CurrentBat = &bat_backup;
    PreviousBat = &bat;
  }

  return SUCCESS;
}

u32 GCMemcard::RemoveFile(u8 index)  // index in the directory array
{
  if (!m_valid)
    return NOMEMCARD;
  if (index >= DIRLEN)
    return DELETE_FAIL;

  u16 startingblock = BE16(dir.Dir[index].FirstBlock);
  u16 numberofblocks = BE16(dir.Dir[index].BlockCount);

  BlockAlloc UpdatedBat = *CurrentBat;
  if (!UpdatedBat.ClearBlocks(startingblock, numberofblocks))
    return DELETE_FAIL;
  UpdatedBat.UpdateCounter = BE16(BE16(UpdatedBat.UpdateCounter) + 1);
  *PreviousBat = UpdatedBat;
  if (PreviousBat == &bat)
  {
    CurrentBat = &bat;
    PreviousBat = &bat_backup;
  }
  else
  {
    CurrentBat = &bat_backup;
    PreviousBat = &bat;
  }

  Directory UpdatedDir = *CurrentDir;
  /*
  // TODO: determine when this is used, even on the same memory card I have seen
  // both update to broken file, and not updated
  *(u32*)&UpdatedDir.Dir[index].Gamecode = 0;
  *(u16*)&UpdatedDir.Dir[index].Makercode = 0;
  memset(UpdatedDir.Dir[index].Filename, 0, 0x20);
  strcpy((char*)UpdatedDir.Dir[index].Filename, "Broken File000");
  *(u16*)&UpdatedDir.UpdateCounter = BE16(BE16(UpdatedDir.UpdateCounter) + 1);

  *PreviousDir = UpdatedDir;
  if (PreviousDir == &dir )
  {
    CurrentDir = &dir;
    PreviousDir = &dir_backup;
  }
  else
  {
    CurrentDir = &dir_backup;
    PreviousDir = &dir;
  }
  */
  memset(&(UpdatedDir.Dir[index]), 0xFF, DENTRY_SIZE);
  UpdatedDir.UpdateCounter = BE16(BE16(UpdatedDir.UpdateCounter) + 1);
  *PreviousDir = UpdatedDir;
  if (PreviousDir == &dir)
  {
    CurrentDir = &dir;
    PreviousDir = &dir_backup;
  }
  else
  {
    CurrentDir = &dir_backup;
    PreviousDir = &dir;
  }

  return SUCCESS;
}

u32 GCMemcard::CopyFrom(const GCMemcard& source, u8 index)
{
  if (!m_valid || !source.m_valid)
    return NOMEMCARD;

  DEntry tempDEntry;
  if (!source.GetDEntry(index, tempDEntry))
    return NOMEMCARD;

  u32 size = source.DEntry_BlockCount(index);
  if (size == 0xFFFF)
    return INVALIDFILESIZE;

  std::vector<GCMBlock> saveData;
  saveData.reserve(size);
  switch (source.GetSaveData(index, saveData))
  {
  case FAIL:
    return FAIL;
  case NOMEMCARD:
    return NOMEMCARD;
  default:
    return ImportFile(tempDEntry, saveData);
  }
}

u32 GCMemcard::ImportGci(const std::string& inputFile, const std::string& outputFile)
{
  if (outputFile.empty() && !m_valid)
    return OPENFAIL;

  File::IOFile gci(inputFile, "rb");
  if (!gci)
    return OPENFAIL;

  return ImportGciInternal(std::move(gci), inputFile, outputFile);
}

u32 GCMemcard::ImportGciInternal(File::IOFile&& gci, const std::string& inputFile,
                                 const std::string& outputFile)
{
  unsigned int offset;
  std::string fileType;
  SplitPath(inputFile, nullptr, nullptr, &fileType);

  if (!strcasecmp(fileType.c_str(), ".gci"))
    offset = GCI;
  else
  {
    char tmp[0xD];
    gci.ReadBytes(tmp, sizeof(tmp));
    if (!strcasecmp(fileType.c_str(), ".gcs"))
    {
      if (!memcmp(tmp, "GCSAVE", 6))  // Header must be uppercase
        offset = GCS;
      else
        return GCSFAIL;
    }
    else if (!strcasecmp(fileType.c_str(), ".sav"))
    {
      if (!memcmp(tmp, "DATELGC_SAVE", 0xC))  // Header must be uppercase
        offset = SAV;
      else
        return SAVFAIL;
    }
    else
      return OPENFAIL;
  }
  gci.Seek(offset, SEEK_SET);

  DEntry tempDEntry;
  gci.ReadBytes(&tempDEntry, DENTRY_SIZE);
  const int fStart = (int)gci.Tell();
  gci.Seek(0, SEEK_END);
  const int length = (int)gci.Tell() - fStart;
  gci.Seek(offset + DENTRY_SIZE, SEEK_SET);

  Gcs_SavConvert(tempDEntry, offset, length);

  if (length != BE16(tempDEntry.BlockCount) * BLOCK_SIZE)
    return LENGTHFAIL;
  if (gci.Tell() != offset + DENTRY_SIZE)  // Verify correct file position
    return OPENFAIL;

  u32 size = BE16((tempDEntry.BlockCount));
  std::vector<GCMBlock> saveData;
  saveData.reserve(size);

  for (unsigned int i = 0; i < size; ++i)
  {
    GCMBlock b;
    gci.ReadBytes(b.block, BLOCK_SIZE);
    saveData.push_back(b);
  }
  u32 ret;
  if (!outputFile.empty())
  {
    File::IOFile gci2(outputFile, "wb");
    bool completeWrite = true;
    if (!gci2)
    {
      return OPENFAIL;
    }
    gci2.Seek(0, SEEK_SET);

    if (!gci2.WriteBytes(&tempDEntry, DENTRY_SIZE))
      completeWrite = false;
    int fileBlocks = BE16(tempDEntry.BlockCount);
    gci2.Seek(DENTRY_SIZE, SEEK_SET);

    for (int i = 0; i < fileBlocks; ++i)
    {
      if (!gci2.WriteBytes(saveData[i].block, BLOCK_SIZE))
        completeWrite = false;
    }

    if (completeWrite)
      ret = GCS;
    else
      ret = WRITEFAIL;
  }
  else
    ret = ImportFile(tempDEntry, saveData);

  return ret;
}

u32 GCMemcard::ExportGci(u8 index, const std::string& fileName, const std::string& directory) const
{
  File::IOFile gci;
  int offset = GCI;

  if (!fileName.length())
  {
    std::string gciFilename;
    // GCI_FileName should only fail if the gamecode is 0xFFFFFFFF
    if (!GCI_FileName(index, gciFilename))
      return SUCCESS;
    gci.Open(directory + DIR_SEP + gciFilename, "wb");
  }
  else
  {
    std::string fileType;
    gci.Open(fileName, "wb");
    SplitPath(fileName, nullptr, nullptr, &fileType);
    if (!strcasecmp(fileType.c_str(), ".gcs"))
    {
      offset = GCS;
    }
    else if (!strcasecmp(fileType.c_str(), ".sav"))
    {
      offset = SAV;
    }
  }

  if (!gci)
    return OPENFAIL;

  gci.Seek(0, SEEK_SET);

  switch (offset)
  {
  case GCS:
    u8 gcsHDR[GCS];
    memset(gcsHDR, 0, GCS);
    memcpy(gcsHDR, "GCSAVE", 6);
    gci.WriteArray(gcsHDR, GCS);
    break;

  case SAV:
    u8 savHDR[SAV];
    memset(savHDR, 0, SAV);
    memcpy(savHDR, "DATELGC_SAVE", 0xC);
    gci.WriteArray(savHDR, SAV);
    break;
  }

  DEntry tempDEntry;
  if (!GetDEntry(index, tempDEntry))
  {
    return NOMEMCARD;
  }

  Gcs_SavConvert(tempDEntry, offset);
  gci.WriteBytes(&tempDEntry, DENTRY_SIZE);

  u32 size = DEntry_BlockCount(index);
  if (size == 0xFFFF)
  {
    return FAIL;
  }

  std::vector<GCMBlock> saveData;
  saveData.reserve(size);

  switch (GetSaveData(index, saveData))
  {
  case FAIL:
    return FAIL;
  case NOMEMCARD:
    return NOMEMCARD;
  }
  gci.Seek(DENTRY_SIZE + offset, SEEK_SET);
  for (unsigned int i = 0; i < size; ++i)
  {
    gci.WriteBytes(saveData[i].block, BLOCK_SIZE);
  }

  if (gci.IsGood())
    return SUCCESS;
  else
    return WRITEFAIL;
}

void GCMemcard::Gcs_SavConvert(DEntry& tempDEntry, int saveType, int length)
{
  switch (saveType)
  {
  case GCS:
  {
    // field containing the Block count as displayed within
    // the GameSaves software is not stored in the GCS file.
    // It is stored only within the corresponding GSV file.
    // If the GCS file is added without using the GameSaves software,
    // the value stored is always "1"
    *(u16*)&tempDEntry.BlockCount = BE16(length / BLOCK_SIZE);
  }
  break;
  case SAV:
    // swap byte pairs
    // 0x2C and 0x2D, 0x2E and 0x2F, 0x30 and 0x31, 0x32 and 0x33,
    // 0x34 and 0x35, 0x36 and 0x37, 0x38 and 0x39, 0x3A and 0x3B,
    // 0x3C and 0x3D,0x3E and 0x3F.
    // It seems that sav files also swap the BIFlags...
    ByteSwap(&tempDEntry.Unused1, &tempDEntry.BIFlags);
    ArrayByteSwap((tempDEntry.ImageOffset));
    ArrayByteSwap(&(tempDEntry.ImageOffset[2]));
    ArrayByteSwap((tempDEntry.IconFmt));
    ArrayByteSwap((tempDEntry.AnimSpeed));
    ByteSwap(&tempDEntry.Permissions, &tempDEntry.CopyCounter);
    ArrayByteSwap((tempDEntry.FirstBlock));
    ArrayByteSwap((tempDEntry.BlockCount));
    ArrayByteSwap((tempDEntry.Unused2));
    ArrayByteSwap((tempDEntry.CommentsAddr));
    ArrayByteSwap(&(tempDEntry.CommentsAddr[2]));
    break;
  default:
    break;
  }
}

bool GCMemcard::ReadBannerRGBA8(u8 index, u32* buffer) const
{
  if (!m_valid || index >= DIRLEN)
    return false;

  int flags = CurrentDir->Dir[index].BIFlags;
  // Timesplitters 2 is the only game that I see this in
  // May be a hack
  if (flags == 0xFB)
    flags = ~flags;

  int bnrFormat = (flags & 3);

  if (bnrFormat == 0)
    return false;

  u32 DataOffset = BE32(CurrentDir->Dir[index].ImageOffset);
  u32 DataBlock = BE16(CurrentDir->Dir[index].FirstBlock) - MC_FST_BLOCKS;

  if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
  {
    return false;
  }

  const int pixels = 96 * 32;

  if (bnrFormat & 1)
  {
    u8* pxdata = (u8*)(mc_data_blocks[DataBlock].block + DataOffset);
    u16* paldata = (u16*)(mc_data_blocks[DataBlock].block + DataOffset + pixels);

    ColorUtil::decodeCI8image(buffer, pxdata, paldata, 96, 32);
  }
  else
  {
    u16* pxdata = (u16*)(mc_data_blocks[DataBlock].block + DataOffset);

    ColorUtil::decode5A3image(buffer, pxdata, 96, 32);
  }
  return true;
}

u32 GCMemcard::ReadAnimRGBA8(u8 index, u32* buffer, u8* delays) const
{
  if (!m_valid || index >= DIRLEN)
    return 0;

  // To ensure only one type of icon is used
  // Sonic Heroes it the only game I have seen that tries to use a CI8 and RGB5A3 icon
  // int fmtCheck = 0;

  int formats = BE16(CurrentDir->Dir[index].IconFmt);
  int fdelays = BE16(CurrentDir->Dir[index].AnimSpeed);

  int flags = CurrentDir->Dir[index].BIFlags;
  // Timesplitters 2 and 3 is the only game that I see this in
  // May be a hack
  // if (flags == 0xFB) flags = ~flags;
  // Batten Kaitos has 0x65 as flag too. Everything but the first 3 bytes seems irrelevant.
  // Something similar happens with Wario Ware Inc. AnimSpeed

  int bnrFormat = (flags & 3);

  u32 DataOffset = BE32(CurrentDir->Dir[index].ImageOffset);
  u32 DataBlock = BE16(CurrentDir->Dir[index].FirstBlock) - MC_FST_BLOCKS;

  if ((DataBlock > maxBlock) || (DataOffset == 0xFFFFFFFF))
  {
    return 0;
  }

  u8* animData = (u8*)(mc_data_blocks[DataBlock].block + DataOffset);

  switch (bnrFormat)
  {
  case 1:
    animData += 96 * 32 + 2 * 256;  // image+palette
    break;
  case 2:
    animData += 96 * 32 * 2;
    break;
  }

  int fmts[8];
  u8* data[8];
  int frames = 0;

  for (int i = 0; i < 8; i++)
  {
    fmts[i] = (formats >> (2 * i)) & 3;
    delays[i] = ((fdelays >> (2 * i)) & 3);
    data[i] = animData;

    if (!delays[i])
    {
      // First icon_speed = 0 indicates there aren't any more icons
      break;
    }
    // If speed is set there is an icon (it can be a "blank frame")
    frames++;
    if (fmts[i] != 0)
    {
      switch (fmts[i])
      {
      case CI8SHARED:  // CI8 with shared palette
        animData += 32 * 32;
        break;
      case RGB5A3:  // RGB5A3
        animData += 32 * 32 * 2;
        break;
      case CI8:  // CI8 with own palette
        animData += 32 * 32 + 2 * 256;
        break;
      }
    }
  }

  const u16* sharedPal = reinterpret_cast<u16*>(animData);

  for (int i = 0; i < 8; i++)
  {
    if (!delays[i])
    {
      // First icon_speed = 0 indicates there aren't any more icons
      break;
    }
    if (fmts[i] != 0)
    {
      switch (fmts[i])
      {
      case CI8SHARED:  // CI8 with shared palette
        ColorUtil::decodeCI8image(buffer, data[i], sharedPal, 32, 32);
        buffer += 32 * 32;
        break;
      case RGB5A3:  // RGB5A3
        ColorUtil::decode5A3image(buffer, (u16*)(data[i]), 32, 32);
        buffer += 32 * 32;
        break;
      case CI8:  // CI8 with own palette
        const u16* paldata = reinterpret_cast<u16*>(data[i] + 32 * 32);
        ColorUtil::decodeCI8image(buffer, data[i], paldata, 32, 32);
        buffer += 32 * 32;
        break;
      }
    }
    else
    {
      // Speed is set but there's no actual icon
      // This is used to reduce animation speed in Pikmin and Luigi's Mansion for example
      // These "blank frames" show the next icon
      for (int j = i; j < 8; ++j)
      {
        if (fmts[j] != 0)
        {
          switch (fmts[j])
          {
          case CI8SHARED:  // CI8 with shared palette
            ColorUtil::decodeCI8image(buffer, data[j], sharedPal, 32, 32);
            break;
          case RGB5A3:  // RGB5A3
            ColorUtil::decode5A3image(buffer, (u16*)(data[j]), 32, 32);
            buffer += 32 * 32;
            break;
          case CI8:  // CI8 with own palette
            const u16* paldata = reinterpret_cast<u16*>(data[j] + 32 * 32);
            ColorUtil::decodeCI8image(buffer, data[j], paldata, 32, 32);
            buffer += 32 * 32;
            break;
          }
        }
      }
    }
  }

  return frames;
}

bool GCMemcard::Format(u8* card_data, bool shift_jis, u16 SizeMb)
{
  if (!card_data)
    return false;
  memset(card_data, 0xFF, BLOCK_SIZE * 3);
  memset(card_data + BLOCK_SIZE * 3, 0, BLOCK_SIZE * 2);

  *((Header*)card_data) = Header(SLOT_A, SizeMb, shift_jis);

  *((Directory*)(card_data + BLOCK_SIZE)) = Directory();
  *((Directory*)(card_data + BLOCK_SIZE * 2)) = Directory();
  *((BlockAlloc*)(card_data + BLOCK_SIZE * 3)) = BlockAlloc(SizeMb);
  *((BlockAlloc*)(card_data + BLOCK_SIZE * 4)) = BlockAlloc(SizeMb);
  return true;
}

bool GCMemcard::Format(bool shift_jis, u16 SizeMb)
{
  memset(&hdr, 0xFF, BLOCK_SIZE);
  memset(&dir, 0xFF, BLOCK_SIZE);
  memset(&dir_backup, 0xFF, BLOCK_SIZE);
  memset(&bat, 0, BLOCK_SIZE);
  memset(&bat_backup, 0, BLOCK_SIZE);

  hdr = Header(SLOT_A, SizeMb, shift_jis);
  dir = dir_backup = Directory();
  bat = bat_backup = BlockAlloc(SizeMb);

  m_sizeMb = SizeMb;
  maxBlock = (u32)m_sizeMb * MBIT_TO_BLOCKS;
  mc_data_blocks.clear();
  mc_data_blocks.resize(maxBlock - MC_FST_BLOCKS);

  InitDirBatPointers();
  m_valid = true;

  return Save();
}

/*************************************************************/
/* FZEROGX_MakeSaveGameValid                                 */
/* (use just before writing a F-Zero GX system .gci file)    */
/*                                                           */
/* Parameters:                                               */
/*    direntry:   [Description needed]                       */
/*    FileBuffer: [Description needed]                       */
/*                                                           */
/* Returns: Error code                                       */
/*************************************************************/

s32 GCMemcard::FZEROGX_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                         std::vector<GCMBlock>& FileBuffer)
{
  u32 i, j;
  u32 serial1, serial2;
  u16 chksum = 0xFFFF;
  int block = 0;

  // check for F-Zero GX system file
  if (strcmp(reinterpret_cast<const char*>(direntry.Filename), "f_zero.dat") != 0)
    return 0;

  // get encrypted destination memory card serial numbers
  cardheader.CARD_GetSerialNo(&serial1, &serial2);

  // set new serial numbers
  *(u16*)&FileBuffer[1].block[0x0066] = BE16(BE32(serial1) >> 16);
  *(u16*)&FileBuffer[3].block[0x1580] = BE16(BE32(serial2) >> 16);
  *(u16*)&FileBuffer[1].block[0x0060] = BE16(BE32(serial1) & 0xFFFF);
  *(u16*)&FileBuffer[1].block[0x0200] = BE16(BE32(serial2) & 0xFFFF);

  // calc 16-bit checksum
  for (i = 0x02; i < 0x8000; i++)
  {
    chksum ^= (FileBuffer[block].block[i - (block * 0x2000)] & 0xFF);
    for (j = 8; j > 0; j--)
    {
      if (chksum & 1)
        chksum = (chksum >> 1) ^ 0x8408;
      else
        chksum >>= 1;
    }
    if (!(i % 0x2000))
      block++;
  }

  // set new checksum
  *(u16*)&FileBuffer[0].block[0x00] = BE16(~chksum);

  return 1;
}

/***********************************************************/
/* PSO_MakeSaveGameValid                                   */
/* (use just before writing a PSO system .gci file)        */
/*                                                         */
/* Parameters:                                             */
/*    direntry:   [Description needed]                     */
/*    FileBuffer: [Description needed]                     */
/*                                                         */
/* Returns: Error code                                     */
/***********************************************************/

s32 GCMemcard::PSO_MakeSaveGameValid(const Header& cardheader, const DEntry& direntry,
                                     std::vector<GCMBlock>& FileBuffer)
{
  u32 i, j;
  u32 chksum;
  u32 crc32LUT[256];
  u32 serial1, serial2;
  u32 pso3offset = 0x00;

  // check for PSO1&2 system file
  if (strcmp(reinterpret_cast<const char*>(direntry.Filename), "PSO_SYSTEM") != 0)
  {
    // check for PSO3 system file
    if (strcmp(reinterpret_cast<const char*>(direntry.Filename), "PSO3_SYSTEM") == 0)
    {
      // PSO3 data block size adjustment
      pso3offset = 0x10;
    }
    else
    {
      // nothing to do
      return 0;
    }
  }

  // get encrypted destination memory card serial numbers
  cardheader.CARD_GetSerialNo(&serial1, &serial2);

  // set new serial numbers
  *(u32*)&FileBuffer[1].block[0x0158] = serial1;
  *(u32*)&FileBuffer[1].block[0x015C] = serial2;

  // generate crc32 LUT
  for (i = 0; i < 256; i++)
  {
    chksum = i;
    for (j = 8; j > 0; j--)
    {
      if (chksum & 1)
        chksum = (chksum >> 1) ^ 0xEDB88320;
      else
        chksum >>= 1;
    }

    crc32LUT[i] = chksum;
  }

  // PSO initial crc32 value
  chksum = 0xDEBB20E3;

  // calc 32-bit checksum
  for (i = 0x004C; i < 0x0164 + pso3offset; i++)
  {
    chksum = ((chksum >> 8) & 0xFFFFFF) ^ crc32LUT[(chksum ^ FileBuffer[1].block[i]) & 0xFF];
  }

  // set new checksum
  *(u32*)&FileBuffer[1].block[0x0048] = BE32(chksum ^ 0xFFFFFFFF);

  return 1;
}
