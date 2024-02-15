// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/GCMemcardDirectory.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCMemcard/GCMemcardUtils.h"
#include "Core/HW/Sram.h"
#include "Core/NetPlayProto.h"

static const char* MC_HDR = "MC_SYSTEM_AREA";

static std::string GenerateDefaultGCIFilename(const Memcard::DEntry& entry,
                                              bool card_encoding_is_shift_jis)
{
  const auto string_decoder = card_encoding_is_shift_jis ? SHIFTJISToUTF8 : CP1252ToUTF8;
  const auto strip_null = [](const std::string_view& s) {
    auto offset = s.find('\0');
    if (offset == std::string_view::npos)
      return s;
    return s.substr(0, offset);
  };

  const std::string_view makercode(reinterpret_cast<const char*>(entry.m_makercode.data()),
                                   entry.m_makercode.size());
  const std::string_view gamecode(reinterpret_cast<const char*>(entry.m_gamecode.data()),
                                  entry.m_gamecode.size());
  const std::string_view filename(reinterpret_cast<const char*>(entry.m_filename.data()),
                                  entry.m_filename.size());
  return Common::EscapeFileName(fmt::format("{}-{}-{}.gci", strip_null(string_decoder(makercode)),
                                            strip_null(string_decoder(gamecode)),
                                            strip_null(string_decoder(filename))));
}

bool GCMemcardDirectory::LoadGCI(Memcard::GCIFile gci)
{
  // check if any already loaded file has the same internal name as the new file
  for (const Memcard::GCIFile& already_loaded_gci : m_saves)
  {
    if (HasSameIdentity(gci.m_gci_header, already_loaded_gci.m_gci_header))
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE,
                    "{}\nwas not loaded because it has the same internal filename as previously "
                    "loaded save\n{}",
                    gci.m_filename, already_loaded_gci.m_filename);
      return false;
    }
  }

  // check if this file has a valid block size
  // 2043 is the largest number of free blocks on a memory card
  // in reality, there are not likely any valid gci files > 251 blocks
  const u16 num_blocks = gci.m_gci_header.m_block_count;
  if (num_blocks > 2043)
  {
    ERROR_LOG_FMT(
        EXPANSIONINTERFACE,
        "{}\nwas not loaded because it is an invalid GCI.\nNumber of blocks claimed to be {}",
        gci.m_filename, num_blocks);
    return false;
  }

  if (!gci.LoadSaveBlocks())
  {
    ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to load data of {}", gci.m_filename);
    return false;
  }

  // reserve storage for the save file in the BAT
  const u16 first_block = m_bat1.AssignBlocksContiguous(num_blocks);
  if (first_block == 0xFFFF)
  {
    ERROR_LOG_FMT(
        EXPANSIONINTERFACE,
        "{}\nwas not loaded because there are not enough free blocks on the virtual memory card",
        gci.m_filename);
    return false;
  }
  gci.m_gci_header.m_first_block = first_block;

  if (gci.HasCopyProtection())
  {
    Memcard::GCMemcard::PSO_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
    Memcard::GCMemcard::FZEROGX_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
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
    game_code = Common::swap32(reinterpret_cast<const u8*>(game_id.c_str()));

  std::vector<Memcard::DEntry> loaded_saves;
  for (const std::string& file_name : Common::DoFileSearch({directory}, {".gci"}))
  {
    File::IOFile gci_file(file_name, "rb");
    if (!gci_file)
      continue;

    Memcard::GCIFile gci;
    gci.m_filename = file_name;
    gci.m_dirty = false;
    if (!gci_file.ReadBytes(&gci.m_gci_header, Memcard::DENTRY_SIZE))
      continue;

    const auto same_identity_save_it = std::find_if(
        loaded_saves.begin(), loaded_saves.end(), [&gci](const Memcard::DEntry& entry) {
          return Memcard::HasSameIdentity(gci.m_gci_header, entry);
        });
    if (same_identity_save_it != loaded_saves.end())
      continue;

    const u16 num_blocks = gci.m_gci_header.m_block_count;
    // largest number of free blocks on a memory card
    // in reality, there are not likely any valid gci files > 251 blocks
    if (num_blocks > 2043)
      continue;

    const u32 size = num_blocks * Memcard::BLOCK_SIZE;
    const u64 file_size = gci_file.GetSize();
    if (file_size != size + Memcard::DENTRY_SIZE)
      continue;

    // There's technically other available block checks to prevent overfilling the virtual memory
    // card (see above method), but since we're only loading the saves for one GameID here, we're
    // definitely not going to run out of space.

    if (game_code == Common::swap32(gci.m_gci_header.m_gamecode.data()))
    {
      loaded_saves.push_back(gci.m_gci_header);
      filenames.push_back(file_name);
    }
  }

  return filenames;
}

GCMemcardDirectory::GCMemcardDirectory(const std::string& directory, ExpansionInterface::Slot slot,
                                       const Memcard::HeaderData& header_data, u32 game_id)
    : MemoryCardBase(slot, header_data.m_size_mb), m_game_id(game_id), m_last_block(-1),
      m_hdr(header_data), m_bat1(header_data.m_size_mb), m_saves(0), m_save_directory(directory),
      m_exiting(false)
{
  // Use existing header data if available
  {
    File::IOFile((m_save_directory + MC_HDR), "rb").ReadBytes(&m_hdr, Memcard::BLOCK_SIZE);
  }

  const bool current_game_only = Config::Get(Config::SESSION_GCI_FOLDER_CURRENT_GAME_ONLY);
  std::vector<std::string> filenames = Common::DoFileSearch({m_save_directory}, {".gci"});

  // split up into files for current games we should definitely load,
  // and files for other games that we don't care too much about
  std::vector<Memcard::GCIFile> gci_current_game;
  std::vector<Memcard::GCIFile> gci_other_games;
  for (const std::string& filename : filenames)
  {
    Memcard::GCIFile gci;
    gci.m_filename = filename;
    gci.m_dirty = false;
    if (!gci.LoadHeader())
    {
      ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to load header of {}", filename);
      continue;
    }

    if (m_game_id == Common::swap32(gci.m_gci_header.m_gamecode.data()))
      gci_current_game.emplace_back(std::move(gci));
    else if (!current_game_only)
      gci_other_games.emplace_back(std::move(gci));
  }

  m_saves.reserve(Memcard::DIRLEN);

  // load files for current game
  size_t failed_loads_current_game = 0;
  for (Memcard::GCIFile& gci : gci_current_game)
  {
    if (!LoadGCI(std::move(gci)))
    {
      // keep track of how many files failed to load for the current game so we can display a
      // message to the user informing them why some of their saves may not be loaded
      ++failed_loads_current_game;
    }
  }

  // leave about 10% of free space on the card if possible
  const int total_blocks = Memcard::MbitToFreeBlocks(m_hdr.m_data.m_size_mb);
  const int reserved_blocks = total_blocks / 10;

  // load files for other games
  for (Memcard::GCIFile& gci : gci_other_games)
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
  if (!Config::Get(Config::SESSION_SAVE_DATA_WRITABLE))
  {
    return;
  }

  Common::SetCurrentThreadName(fmt::format("Memcard {} flushing thread", m_card_slot).c_str());

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
  s32 block = src_address / Memcard::BLOCK_SIZE;
  u32 offset = src_address % Memcard::BLOCK_SIZE;
  s32 extra = 0;  // used for read calls that are across multiple blocks

  if (offset + length > Memcard::BLOCK_SIZE)
  {
    extra = length + offset - Memcard::BLOCK_SIZE;
    length -= extra;

    // verify that we haven't calculated a length beyond BLOCK_SIZE
    DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, (src_address + length) % Memcard::BLOCK_SIZE == 0,
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
  std::unique_lock l(m_write_mutex);
  if (length != 0x80)
    INFO_LOG_FMT(EXPANSIONINTERFACE, "Writing to {:#x}. Length: {:#x}", dest_address, length);
  s32 block = dest_address / Memcard::BLOCK_SIZE;
  u32 offset = dest_address % Memcard::BLOCK_SIZE;
  s32 extra = 0;  // used for write calls that are across multiple blocks

  if (offset + length > Memcard::BLOCK_SIZE)
  {
    extra = length + offset - Memcard::BLOCK_SIZE;
    length -= extra;

    // verify that we haven't calculated a length beyond BLOCK_SIZE
    DEBUG_ASSERT_MSG(EXPANSIONINTERFACE, (dest_address + length) % Memcard::BLOCK_SIZE == 0,
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
        s32 to_write = std::min<s32>(Memcard::DENTRY_SIZE, length);
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
        PanicAlertFmtT("Report: GCIFolder Writing to unallocated block {0:#x}", block);
        exit(0);
      }
    }
  }

  memcpy(m_last_block_address + offset, src_address, length);

  l.unlock();
  if (extra)
    extra = Write(dest_address + length, extra, src_address + length);
  if (offset + length == Memcard::BLOCK_SIZE)
    m_flush_trigger.Set();
  return length + extra;
}

void GCMemcardDirectory::ClearBlock(u32 address)
{
  if (address % Memcard::BLOCK_SIZE)
  {
    PanicAlertFmtT("GCMemcardDirectory: ClearBlock called with invalid block address");
    return;
  }

  const u32 block = address / Memcard::BLOCK_SIZE;
  INFO_LOG_FMT(EXPANSIONINTERFACE, "Clearing block {}", block);
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
  std::memset(m_last_block_address, 0xFF, Memcard::BLOCK_SIZE);
}

inline void GCMemcardDirectory::SyncSaves()
{
  Memcard::Directory* current = &m_dir2;

  if (m_dir1.m_update_counter > m_dir2.m_update_counter)
  {
    current = &m_dir1;
  }

  for (u32 i = 0; i < Memcard::DIRLEN; ++i)
  {
    if (current->m_dir_entries[i].m_gamecode != Memcard::DEntry::UNINITIALIZED_GAMECODE)
    {
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Syncing save {:#x}",
                   Common::swap32(current->m_dir_entries[i].m_gamecode.data()));
      bool added = false;
      while (i >= m_saves.size())
      {
        Memcard::GCIFile temp;
        m_saves.push_back(temp);
        added = true;
      }

      if (added || memcmp((u8*)&(m_saves[i].m_gci_header), (u8*)&(current->m_dir_entries[i]),
                          Memcard::DENTRY_SIZE))
      {
        m_saves[i].m_dirty = true;
        const u32 gamecode = Common::swap32(m_saves[i].m_gci_header.m_gamecode.data());
        const u32 new_gamecode = Common::swap32(current->m_dir_entries[i].m_gamecode.data());
        const u32 old_start = m_saves[i].m_gci_header.m_first_block;
        const u32 new_start = current->m_dir_entries[i].m_first_block;

        if ((gamecode != 0xFFFFFFFF) && (gamecode != new_gamecode))
        {
          PanicAlertFmtT(
              "Game overwrote with another games save. Data corruption ahead {0:#x}, {1:#x}",
              Common::swap32(m_saves[i].m_gci_header.m_gamecode.data()),
              Common::swap32(current->m_dir_entries[i].m_gamecode.data()));
        }
        memcpy((u8*)&(m_saves[i].m_gci_header), (u8*)&(current->m_dir_entries[i]),
               Memcard::DENTRY_SIZE);
        if (old_start != new_start)
        {
          INFO_LOG_FMT(EXPANSIONINTERFACE, "Save moved from {:#x} to {:#x}", old_start, new_start);
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
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Clearing and/or deleting save {:#x}",
                   Common::swap32(m_saves[i].m_gci_header.m_gamecode.data()));
      m_saves[i].m_gci_header.m_gamecode = Memcard::DEntry::UNINITIALIZED_GAMECODE;
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
    if (m_saves[i].m_gci_header.m_gamecode != Memcard::DEntry::UNINITIALIZED_GAMECODE)
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
  u32 block = dest_address / Memcard::BLOCK_SIZE;
  u32 offset = dest_address % Memcard::BLOCK_SIZE;
  Memcard::Directory* dest = (block == 1) ? &m_dir1 : &m_dir2;
  u16 Dnum = offset / Memcard::DENTRY_SIZE;

  if (Dnum == Memcard::DIRLEN)
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
  Memcard::BlockAlloc* current_bat;
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
      PanicAlertFmtT("BAT incorrect. Dolphin will now exit");
      exit(0);
    }
  }

  const u16 num_blocks = m_saves[save_index].m_gci_header.m_block_count;
  const u16 blocks_from_bat = static_cast<u16>(m_saves[save_index].m_used_blocks.size());
  if (blocks_from_bat != num_blocks)
  {
    PanicAlertFmtT(
        "Warning: Number of blocks indicated by the BAT ({0}) does not match that of the "
        "loaded file header ({1})",
        blocks_from_bat, num_blocks);
    return false;
  }

  return true;
}

void GCMemcardDirectory::FlushToFile()
{
  std::unique_lock l(m_write_mutex);
  Memcard::DEntry invalid;
  for (Memcard::GCIFile& save : m_saves)
  {
    if (save.m_dirty)
    {
      if (save.m_gci_header.m_gamecode != Memcard::DEntry::UNINITIALIZED_GAMECODE)
      {
        save.m_dirty = false;
        if (save.m_save_data.empty())
        {
          // The save's header has been changed but the actual save blocks haven't been read/written
          // to
          // skip flushing this file until actual save data is modified
          ERROR_LOG_FMT(EXPANSIONINTERFACE,
                        "GCI header modified without corresponding save data changes");
          continue;
        }
        if (save.m_filename.empty())
        {
          std::string default_save_name =
              m_save_directory + GenerateDefaultGCIFilename(save.m_gci_header, m_hdr.IsShiftJIS());

          // Check to see if another file is using the same name
          // This seems unlikely except in the case of file corruption
          // otherwise what user would name another file this way?
          for (int j = 0; File::Exists(default_save_name) && j < 10; ++j)
          {
            default_save_name.insert(default_save_name.end() - 4, '0');
          }
          if (File::Exists(default_save_name))
          {
            PanicAlertFmtT("Failed to find new filename.\n{0}\n will be overwritten",
                           default_save_name);
          }
          save.m_filename = default_save_name;
        }
        File::IOFile gci(save.m_filename, "wb");
        if (gci)
        {
          gci.WriteBytes(&save.m_gci_header, Memcard::DENTRY_SIZE);
          for (const Memcard::GCMBlock& block : save.m_save_data)
            gci.WriteBytes(block.m_block.data(), Memcard::BLOCK_SIZE);

          if (gci.IsGood())
          {
            Core::DisplayMessage("Wrote save contents to GCI Folder", 4000);
          }
          else
          {
            Core::DisplayMessage(
                fmt::format("Failed to write save contents to {}", save.m_filename), 10000);
            ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to save data to {}", save.m_filename);
          }
        }
        else
        {
          Core::DisplayMessage(
              fmt::format("Failed to open file at {} for writing", save.m_filename), 10000);
          ERROR_LOG_FMT(EXPANSIONINTERFACE, "Failed to open file at {} for writing",
                        save.m_filename);
        }
      }
      else if (save.m_filename.length() != 0)
      {
        save.m_dirty = false;
        std::string& old_name = save.m_filename;
        std::string deleted_name = old_name + ".deleted";
        if (File::Exists(deleted_name))
          File::Delete(deleted_name);
        File::Rename(old_name, deleted_name);
        save.m_filename.clear();
        save.m_save_data.clear();
        save.m_used_blocks.clear();
      }
    }

    // Unload the save data for any game that is not running
    // we could use !m_dirty, but some games have multiple gci files and may not write to them
    // simultaneously
    // this ensures that the save data for all of the current games gci files are stored in the
    // savestate
    const u32 gamecode = Common::swap32(save.m_gci_header.m_gamecode.data());
    if (gamecode != m_game_id && gamecode != 0xFFFFFFFF && !save.m_save_data.empty())
    {
      INFO_LOG_FMT(EXPANSIONINTERFACE, "Flushing savedata to disk for {}", save.m_filename);
      save.m_save_data.clear();
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
  std::unique_lock l(m_write_mutex);
  m_last_block = -1;
  m_last_block_address = nullptr;
  p.Do(m_save_directory);
  p.Do(m_hdr);
  p.Do(m_dir1);
  p.Do(m_dir2);
  p.Do(m_bat1);
  p.Do(m_bat2);
  p.DoEachElement(m_saves, [](PointerWrap& p_, Memcard::GCIFile& save) { save.DoState(p_); });
}

void MigrateFromMemcardFile(const std::string& directory_name, ExpansionInterface::Slot card_slot,
                            DiscIO::Region region)
{
  File::CreateFullPath(directory_name);
  const std::string ini_memcard = Config::GetMemcardPath(card_slot, region);
  if (File::Exists(ini_memcard))
  {
    auto [error_code, memcard] = Memcard::GCMemcard::Open(ini_memcard.c_str());
    if (!error_code.HasCriticalErrors() && memcard && memcard->IsValid())
    {
      for (u8 i = 0; i < Memcard::DIRLEN; i++)
      {
        const auto savefile = memcard->ExportFile(i);
        if (!savefile)
          continue;

        std::string filepath =
            directory_name + DIR_SEP + Memcard::GenerateFilename(savefile->dir_entry) + ".gci";
        Memcard::WriteSavefile(filepath, *savefile, Memcard::SavefileFormat::GCI);
      }
    }
  }
}
