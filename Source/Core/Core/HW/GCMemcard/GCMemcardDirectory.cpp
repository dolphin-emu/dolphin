// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GCMemcard/GCMemcardDirectory.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/File.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayProto.h"

const int NO_INDEX = -1;
static const char* MC_HDR = "MC_SYSTEM_AREA";

bool GCMemcardDirectory::LoadGCI(GCIFile gci)
{
  // check if any already loaded file has the same internal name as the new file
  for (const GCIFile& already_loaded_gci : m_saves)
  {
    if (gci.m_gci_header.GCI_FileName() == already_loaded_gci.m_gci_header.GCI_FileName())
    {
      ERROR_LOG(EXPANSIONINTERFACE,
                "%s\nwas not loaded because it has the same internal filename as previously "
                "loaded save\n%s",
                gci.m_filename.c_str(), already_loaded_gci.m_filename.c_str());
      return false;
    }
  }

  // check if this file has a valid block size
  // 2043 is the largest number of free blocks on a memory card
  // in reality, there are not likely any valid gci files > 251 blocks
  const u16 num_blocks = gci.m_gci_header.m_block_count;
  if (num_blocks > 2043)
  {
    ERROR_LOG(EXPANSIONINTERFACE,
              "%s\nwas not loaded because it is an invalid GCI.\nNumber of blocks claimed to be %u",
              gci.m_filename.c_str(), num_blocks);
    return false;
  }

  if (!gci.LoadSaveBlocks())
  {
    ERROR_LOG(EXPANSIONINTERFACE, "Failed to load data of %s", gci.m_filename.c_str());
    return false;
  }

  // reserve storage for the save file in the BAT
  u16 first_block = m_bat1.AssignBlocksContiguous(num_blocks);
  if (first_block == 0xFFFF)
  {
    ERROR_LOG(
        EXPANSIONINTERFACE,
        "%s\nwas not loaded because there are not enough free blocks on the virtual memory card",
        gci.m_filename.c_str());
    return false;
  }
  gci.m_gci_header.m_first_block = first_block;

  if (gci.HasCopyProtection())
  {
    GCMemcard::PSO_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
    GCMemcard::FZEROGX_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
  }

  // actually load save file into memory card
  int idx = (int)m_saves.size();
  m_dir1.Replace(gci.m_gci_header, idx);
  m_saves.push_back(std::move(gci));
  SetUsedBlocks(idx);

  return true;
}

// This is only used by NetPlay but it made sense to put it here to keep the relevant code together
std::vector<std::string> GCMemcardDirectory::GetFileNamesForGameID(const std::string& directory,
                                                                   const std::string& game_id)
{
  std::vector<std::string> filenames;

  u32 game_code = 0;
  if (game_id.length() >= 4 && game_id != "00000000")
    game_code = BE32(reinterpret_cast<const u8*>(game_id.c_str()));

  std::vector<std::string> loaded_saves;
  for (const std::string& file_name : Common::DoFileSearch({directory}, {".gci"}))
  {
    File::IOFile gci_file(file_name, "rb");
    if (!gci_file)
      continue;

    GCIFile gci;
    gci.m_filename = file_name;
    gci.m_dirty = false;
    if (!gci_file.ReadBytes(&gci.m_gci_header, DENTRY_SIZE))
      continue;

    const std::string gci_filename = gci.m_gci_header.GCI_FileName();
    if (std::find(loaded_saves.begin(), loaded_saves.end(), gci_filename) != loaded_saves.end())
      continue;

    const u16 num_blocks = gci.m_gci_header.m_block_count;
    // largest number of free blocks on a memory card
    // in reality, there are not likely any valid gci files > 251 blocks
    if (num_blocks > 2043)
      continue;

    const u32 size = num_blocks * BLOCK_SIZE;
    const u64 file_size = gci_file.GetSize();
    if (file_size != size + DENTRY_SIZE)
      continue;

    // There's technically other available block checks to prevent overfilling the virtual memory
    // card (see above method), but since we're only loading the saves for one GameID here, we're
    // definitely not going to run out of space.

    if (game_code == BE32(gci.m_gci_header.m_gamecode.data()))
    {
      loaded_saves.push_back(gci_filename);
      filenames.push_back(file_name);
    }
  }

  return filenames;
}

GCMemcardDirectory::GCMemcardDirectory(const std::string& directory, int slot, u16 size_mbits,
                                       bool shift_jis, int game_id)
    : MemoryCardBase(slot, size_mbits), m_game_id(game_id), m_last_block(-1),
      m_hdr(slot, size_mbits, shift_jis), m_bat1(size_mbits), m_saves(0),
      m_save_directory(directory), m_exiting(false)
{
  // Use existing header data if available
  {
    File::IOFile((m_save_directory + MC_HDR), "rb").ReadBytes(&m_hdr, BLOCK_SIZE);
  }

  const bool current_game_only = Config::Get(Config::MAIN_GCI_FOLDER_CURRENT_GAME_ONLY);
  std::vector<std::string> filenames = Common::DoFileSearch({m_save_directory}, {".gci"});

  // split up into files for current games we should definitely load,
  // and files for other games that we don't care too much about
  std::vector<GCIFile> gci_current_game;
  std::vector<GCIFile> gci_other_games;
  for (const std::string& filename : filenames)
  {
    GCIFile gci;
    gci.m_filename = filename;
    gci.m_dirty = false;
    if (!gci.LoadHeader())
    {
      ERROR_LOG(EXPANSIONINTERFACE, "Failed to load header of %s", filename.c_str());
      continue;
    }

    if (m_game_id == BE32(gci.m_gci_header.m_gamecode.data()))
      gci_current_game.emplace_back(std::move(gci));
    else if (!current_game_only)
      gci_other_games.emplace_back(std::move(gci));
  }

  m_saves.reserve(DIRLEN);

  // load files for current game
  size_t failed_loads_current_game = 0;
  for (GCIFile& gci : gci_current_game)
  {
    if (!LoadGCI(std::move(gci)))
    {
      // keep track of how many files failed to load for the current game so we can display a
      // message to the user informing them why some of their saves may not be loaded
      ++failed_loads_current_game;
    }
  }

  // leave about 10% of free space on the card if possible
  const int total_blocks = m_hdr.m_size_mb * MBIT_TO_BLOCKS - MC_FST_BLOCKS;
  const int reserved_blocks = total_blocks / 10;

  // load files for other games
  for (GCIFile& gci : gci_other_games)
  {
    // leave some free file entries for new saves that might be created
    if (m_saves.size() > 112)
      break;

    // leave some free blocks for new saves that might be created
    const int free_blocks = m_bat1.m_free_blocks;
    const int gci_blocks = gci.m_gci_header.m_block_count;
    if (free_blocks - gci_blocks < reserved_blocks)
      continue;

    LoadGCI(std::move(gci));
  }

  if (failed_loads_current_game > 0)
  {
    Core::DisplayMessage("Warning: Save file(s) of the current game failed to load.", 10000);
  }

  m_dir1.FixChecksums();
  m_dir2 = m_dir1;
  m_bat2 = m_bat1;

  m_flush_thread = std::thread(&GCMemcardDirectory::FlushThread, this);
}

void GCMemcardDirectory::FlushThread()
{
  if (!SConfig::GetInstance().bEnableMemcardSdWriting)
  {
    return;
  }

  Common::SetCurrentThreadName(
      StringFromFormat("Memcard %d flushing thread", m_card_index).c_str());

  constexpr std::chrono::seconds flush_interval{1};
  while (true)
  {
    // no-op until signalled
    m_flush_trigger.Wait();

    if (m_exiting.TestAndClear())
      return;
    // no-op as long as signalled within flush_interval
    while (m_flush_trigger.WaitFor(flush_interval))
    {
      if (m_exiting.TestAndClear())
        return;
    }

    FlushToFile();
  }
}

GCMemcardDirectory::~GCMemcardDirectory()
{
  m_exiting.Set();
  m_flush_trigger.Set();
  m_flush_thread.join();

  FlushToFile();
}

s32 GCMemcardDirectory::Read(u32 src_address, s32 length, u8* dest_address)
{
  s32 block = src_address / BLOCK_SIZE;
  u32 offset = src_address % BLOCK_SIZE;
  s32 extra = 0;  // used for read calls that are across multiple blocks

  if (offset + length > BLOCK_SIZE)
  {
    extra = length + offset - BLOCK_SIZE;
    length -= extra;

    // verify that we haven't calculated a length beyond BLOCK_SIZE
    DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, (src_address + length) % BLOCK_SIZE == 0,
                     "Memcard directory Read Logic Error");
  }

  if (m_last_block != block)
  {
    switch (block)
    {
    case 0:
      m_last_block = block;
      m_last_block_address = (u8*)&m_hdr;
      break;
    case 1:
      m_last_block = -1;
      m_last_block_address = (u8*)&m_dir1;
      break;
    case 2:
      m_last_block = -1;
      m_last_block_address = (u8*)&m_dir2;
      break;
    case 3:
      m_last_block = block;
      m_last_block_address = (u8*)&m_bat1;
      break;
    case 4:
      m_last_block = block;
      m_last_block_address = (u8*)&m_bat2;
      break;
    default:
      m_last_block = SaveAreaRW(block);

      if (m_last_block == -1)
      {
        memset(dest_address, 0xFF, length);
        return 0;
      }
    }
  }

  memcpy(dest_address, m_last_block_address + offset, length);
  if (extra)
    extra = Read(src_address + length, extra, dest_address + length);
  return length + extra;
}

s32 GCMemcardDirectory::Write(u32 dest_address, s32 length, const u8* src_address)
{
  std::unique_lock<std::mutex> l(m_write_mutex);
  if (length != 0x80)
    INFO_LOG(EXPANSIONINTERFACE, "Writing to 0x%x. Length: 0x%x", dest_address, length);
  s32 block = dest_address / BLOCK_SIZE;
  u32 offset = dest_address % BLOCK_SIZE;
  s32 extra = 0;  // used for write calls that are across multiple blocks

  if (offset + length > BLOCK_SIZE)
  {
    extra = length + offset - BLOCK_SIZE;
    length -= extra;

    // verify that we haven't calculated a length beyond BLOCK_SIZE
    DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, (dest_address + length) % BLOCK_SIZE == 0,
                     "Memcard directory Write Logic Error");
  }
  if (m_last_block != block)
  {
    switch (block)
    {
    case 0:
      m_last_block = block;
      m_last_block_address = (u8*)&m_hdr;
      break;
    case 1:
    case 2:
    {
      m_last_block = -1;
      s32 bytes_written = 0;
      while (length > 0)
      {
        s32 to_write = std::min<s32>(DENTRY_SIZE, length);
        bytes_written +=
            DirectoryWrite(dest_address + bytes_written, to_write, src_address + bytes_written);
        length -= to_write;
      }
      return bytes_written;
    }
    case 3:
      m_last_block = block;
      m_last_block_address = (u8*)&m_bat1;
      break;
    case 4:
      m_last_block = block;
      m_last_block_address = (u8*)&m_bat2;
      break;
    default:
      m_last_block = SaveAreaRW(block, true);
      if (m_last_block == -1)
      {
        PanicAlertT("Report: GCIFolder Writing to unallocated block 0x%x", block);
        exit(0);
      }
    }
  }

  memcpy(m_last_block_address + offset, src_address, length);

  l.unlock();
  if (extra)
    extra = Write(dest_address + length, extra, src_address + length);
  if (offset + length == BLOCK_SIZE)
    m_flush_trigger.Set();
  return length + extra;
}

void GCMemcardDirectory::ClearBlock(u32 address)
{
  if (address % BLOCK_SIZE)
  {
    PanicAlertT("GCMemcardDirectory: ClearBlock called with invalid block address");
    return;
  }

  u32 block = address / BLOCK_SIZE;
  INFO_LOG(EXPANSIONINTERFACE, "Clearing block %u", block);
  switch (block)
  {
  case 0:
    m_last_block = block;
    m_last_block_address = (u8*)&m_hdr;
    break;
  case 1:
    m_last_block = -1;
    m_last_block_address = (u8*)&m_dir1;
    break;
  case 2:
    m_last_block = -1;
    m_last_block_address = (u8*)&m_dir2;
    break;
  case 3:
    m_last_block = block;
    m_last_block_address = (u8*)&m_bat1;
    break;
  case 4:
    m_last_block = block;
    m_last_block_address = (u8*)&m_bat2;
    break;
  default:
    m_last_block = SaveAreaRW(block, true);
    if (m_last_block == -1)
      return;
  }
  ((GCMBlock*)m_last_block_address)->Erase();
}

inline void GCMemcardDirectory::SyncSaves()
{
  Directory* current = &m_dir2;

  if (m_dir1.m_update_counter > m_dir2.m_update_counter)
  {
    current = &m_dir1;
  }

  for (u32 i = 0; i < DIRLEN; ++i)
  {
    if (current->m_dir_entries[i].m_gamecode != DEntry::UNINITIALIZED_GAMECODE)
    {
      INFO_LOG(EXPANSIONINTERFACE, "Syncing save 0x%x",
               BE32(current->m_dir_entries[i].m_gamecode.data()));
      bool added = false;
      while (i >= m_saves.size())
      {
        GCIFile temp;
        m_saves.push_back(temp);
        added = true;
      }

      if (added ||
          memcmp((u8*)&(m_saves[i].m_gci_header), (u8*)&(current->m_dir_entries[i]), DENTRY_SIZE))
      {
        m_saves[i].m_dirty = true;
        u32 gamecode = BE32(m_saves[i].m_gci_header.m_gamecode.data());
        u32 new_gamecode = BE32(current->m_dir_entries[i].m_gamecode.data());
        u32 old_start = m_saves[i].m_gci_header.m_first_block;
        u32 new_start = current->m_dir_entries[i].m_first_block;

        if ((gamecode != 0xFFFFFFFF) && (gamecode != new_gamecode))
        {
          PanicAlertT("Game overwrote with another games save. Data corruption ahead 0x%x, 0x%x",
                      BE32(m_saves[i].m_gci_header.m_gamecode.data()),
                      BE32(current->m_dir_entries[i].m_gamecode.data()));
        }
        memcpy((u8*)&(m_saves[i].m_gci_header), (u8*)&(current->m_dir_entries[i]), DENTRY_SIZE);
        if (old_start != new_start)
        {
          INFO_LOG(EXPANSIONINTERFACE, "Save moved from 0x%x to 0x%x", old_start, new_start);
          m_saves[i].m_used_blocks.clear();
          m_saves[i].m_save_data.clear();
        }
        if (m_saves[i].m_used_blocks.empty())
        {
          SetUsedBlocks(i);
        }
      }
    }
    else if ((i < m_saves.size()) && (*(u32*)&(m_saves[i].m_gci_header) != 0xFFFFFFFF))
    {
      INFO_LOG(EXPANSIONINTERFACE, "Clearing and/or deleting save 0x%x",
               BE32(m_saves[i].m_gci_header.m_gamecode.data()));
      m_saves[i].m_gci_header.m_gamecode = DEntry::UNINITIALIZED_GAMECODE;
      m_saves[i].m_save_data.clear();
      m_saves[i].m_used_blocks.clear();
      m_saves[i].m_dirty = true;
    }
  }
}
inline s32 GCMemcardDirectory::SaveAreaRW(u32 block, bool writing)
{
  for (u16 i = 0; i < m_saves.size(); ++i)
  {
    if (m_saves[i].m_gci_header.m_gamecode != DEntry::UNINITIALIZED_GAMECODE)
    {
      if (m_saves[i].m_used_blocks.empty())
      {
        SetUsedBlocks(i);
      }

      int idx = m_saves[i].UsesBlock(block);
      if (idx != -1)
      {
        if (!m_saves[i].LoadSaveBlocks())
        {
          int num_blocks = m_saves[i].m_gci_header.m_block_count;
          while (num_blocks)
          {
            m_saves[i].m_save_data.emplace_back();
            num_blocks--;
          }
        }

        if (writing)
        {
          m_saves[i].m_dirty = true;
        }

        m_last_block = block;
        m_last_block_address = m_saves[i].m_save_data[idx].m_block.data();
        return m_last_block;
      }
    }
  }
  return -1;
}

s32 GCMemcardDirectory::DirectoryWrite(u32 dest_address, u32 length, const u8* src_address)
{
  u32 block = dest_address / BLOCK_SIZE;
  u32 offset = dest_address % BLOCK_SIZE;
  Directory* dest = (block == 1) ? &m_dir1 : &m_dir2;
  u16 Dnum = offset / DENTRY_SIZE;

  if (Dnum == DIRLEN)
  {
    // first 58 bytes should always be 0xff
    // needed to update the update ctr, checksums
    // could check for writes to the 6 important bytes but doubtful that it improves performance
    // noticably
    memcpy((u8*)(dest) + offset, src_address, length);
    SyncSaves();
  }
  else
    memcpy((u8*)(dest) + offset, src_address, length);
  return length;
}

bool GCMemcardDirectory::SetUsedBlocks(int save_index)
{
  BlockAlloc* current_bat;
  if (m_bat2.m_update_counter > m_bat1.m_update_counter)
    current_bat = &m_bat2;
  else
    current_bat = &m_bat1;

  u16 block = m_saves[save_index].m_gci_header.m_first_block;
  while (block != 0xFFFF)
  {
    m_saves[save_index].m_used_blocks.push_back(block);
    block = current_bat->GetNextBlock(block);
    if (block == 0)
    {
      PanicAlertT("BAT incorrect. Dolphin will now exit");
      exit(0);
    }
  }

  u16 num_blocks = m_saves[save_index].m_gci_header.m_block_count;
  u16 blocks_from_bat = (u16)m_saves[save_index].m_used_blocks.size();
  if (blocks_from_bat != num_blocks)
  {
    PanicAlertT("Warning: Number of blocks indicated by the BAT (%u) does not match that of the "
                "loaded file header (%u)",
                blocks_from_bat, num_blocks);
    return false;
  }

  return true;
}

void GCMemcardDirectory::FlushToFile()
{
  std::unique_lock<std::mutex> l(m_write_mutex);
  int errors = 0;
  DEntry invalid;
  for (u16 i = 0; i < m_saves.size(); ++i)
  {
    if (m_saves[i].m_dirty)
    {
      if (m_saves[i].m_gci_header.m_gamecode != DEntry::UNINITIALIZED_GAMECODE)
      {
        m_saves[i].m_dirty = false;
        if (m_saves[i].m_save_data.empty())
        {
          // The save's header has been changed but the actual save blocks haven't been read/written
          // to
          // skip flushing this file until actual save data is modified
          ERROR_LOG(EXPANSIONINTERFACE,
                    "GCI header modified without corresponding save data changes");
          continue;
        }
        if (m_saves[i].m_filename.empty())
        {
          std::string default_save_name = m_save_directory + m_saves[i].m_gci_header.GCI_FileName();

          // Check to see if another file is using the same name
          // This seems unlikely except in the case of file corruption
          // otherwise what user would name another file this way?
          for (int j = 0; File::Exists(default_save_name) && j < 10; ++j)
          {
            default_save_name.insert(default_save_name.end() - 4, '0');
          }
          if (File::Exists(default_save_name))
            PanicAlertT("Failed to find new filename.\n%s\n will be overwritten",
                        default_save_name.c_str());
          m_saves[i].m_filename = default_save_name;
        }
        File::IOFile gci(m_saves[i].m_filename, "wb");
        if (gci)
        {
          gci.WriteBytes(&m_saves[i].m_gci_header, DENTRY_SIZE);
          gci.WriteBytes(m_saves[i].m_save_data.data(), BLOCK_SIZE * m_saves[i].m_save_data.size());

          if (gci.IsGood())
          {
            Core::DisplayMessage(
                StringFromFormat("Wrote save contents to %s", m_saves[i].m_filename.c_str()), 4000);
          }
          else
          {
            ++errors;
            Core::DisplayMessage(StringFromFormat("Failed to write save contents to %s",
                                                  m_saves[i].m_filename.c_str()),
                                 4000);
            ERROR_LOG(EXPANSIONINTERFACE, "Failed to save data to %s",
                      m_saves[i].m_filename.c_str());
          }
        }
      }
      else if (m_saves[i].m_filename.length() != 0)
      {
        m_saves[i].m_dirty = false;
        std::string& old_name = m_saves[i].m_filename;
        std::string deleted_name = old_name + ".deleted";
        if (File::Exists(deleted_name))
          File::Delete(deleted_name);
        File::Rename(old_name, deleted_name);
        m_saves[i].m_filename.clear();
        m_saves[i].m_save_data.clear();
        m_saves[i].m_used_blocks.clear();
      }
    }

    // Unload the save data for any game that is not running
    // we could use !m_dirty, but some games have multiple gci files and may not write to them
    // simultaneously
    // this ensures that the save data for all of the current games gci files are stored in the
    // savestate
    u32 gamecode = BE32(m_saves[i].m_gci_header.m_gamecode.data());
    if (gamecode != m_game_id && gamecode != 0xFFFFFFFF && !m_saves[i].m_save_data.empty())
    {
      INFO_LOG(EXPANSIONINTERFACE, "Flushing savedata to disk for %s",
               m_saves[i].m_filename.c_str());
      m_saves[i].m_save_data.clear();
    }
  }
#if _WRITE_MC_HEADER
  u8 mc[BLOCK_SIZE * MC_FST_BLOCKS];
  Read(0, BLOCK_SIZE * MC_FST_BLOCKS, mc);
  File::IOFile hdrfile(m_save_directory + MC_HDR, "wb");
  hdrfile.WriteBytes(mc, BLOCK_SIZE * MC_FST_BLOCKS);
#endif
}

void GCMemcardDirectory::DoState(PointerWrap& p)
{
  std::unique_lock<std::mutex> l(m_write_mutex);
  m_last_block = -1;
  m_last_block_address = nullptr;
  p.Do(m_save_directory);
  p.DoPOD<Header>(m_hdr);
  p.DoPOD<Directory>(m_dir1);
  p.DoPOD<Directory>(m_dir2);
  p.DoPOD<BlockAlloc>(m_bat1);
  p.DoPOD<BlockAlloc>(m_bat2);
  int num_saves = (int)m_saves.size();
  p.Do(num_saves);
  m_saves.resize(num_saves);
  for (auto itr = m_saves.begin(); itr != m_saves.end(); ++itr)
  {
    itr->DoState(p);
  }
}

void MigrateFromMemcardFile(const std::string& directory_name, int card_index)
{
  File::CreateFullPath(directory_name);
  std::string ini_memcard = (card_index == 0) ? Config::Get(Config::MAIN_MEMCARD_A_PATH) :
                                                Config::Get(Config::MAIN_MEMCARD_B_PATH);
  if (File::Exists(ini_memcard))
  {
    GCMemcard memcard(ini_memcard.c_str());
    if (memcard.IsValid())
    {
      for (u8 i = 0; i < DIRLEN; i++)
      {
        memcard.ExportGci(i, "", directory_name);
      }
    }
  }
}
