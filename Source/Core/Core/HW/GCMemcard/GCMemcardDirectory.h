// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/Event.h"
#include "Core/HW/GCMemcard/GCMemcard.h"

// Uncomment this to write the system data of the memorycard from directory to disc
//#define _WRITE_MC_HEADER 1
void MigrateFromMemcardFile(const std::string& directory_name, int card_index);

class GCMemcardDirectory : public MemoryCardBase
{
public:
  GCMemcardDirectory(const std::string& directory, int slot, u16 size_mbits, bool shift_jis,
                     int game_id);
  ~GCMemcardDirectory();

  GCMemcardDirectory(const GCMemcardDirectory&) = delete;
  GCMemcardDirectory& operator=(const GCMemcardDirectory&) = delete;
  GCMemcardDirectory(GCMemcardDirectory&&) = default;
  GCMemcardDirectory& operator=(GCMemcardDirectory&&) = default;

  void FlushToFile();
  void FlushThread();
  s32 Read(u32 src_address, s32 length, u8* dest_address) override;
  s32 Write(u32 dest_address, s32 length, const u8* src_address) override;
  void ClearBlock(u32 address) override;
  void ClearAll() override {}
  void DoState(PointerWrap& p) override;

private:
  int LoadGCI(const std::string& file_name, bool current_game_only);
  inline s32 SaveAreaRW(u32 block, bool writing = false);
  // s32 DirectoryRead(u32 offset, u32 length, u8* dest_address);
  s32 DirectoryWrite(u32 dest_address, u32 length, const u8* src_address);
  inline void SyncSaves();
  bool SetUsedBlocks(int save_index);

  u32 m_game_id;
  s32 m_last_block;
  u8* m_last_block_address;

  Header m_hdr;
  Directory m_dir1, m_dir2;
  BlockAlloc m_bat1, m_bat2;
  std::vector<GCIFile> m_saves;

  std::vector<std::string> m_loaded_saves;
  std::string m_save_directory;
  Common::Event m_flush_trigger;
  std::mutex m_write_mutex;
  Common::Flag m_exiting;
  std::thread m_flush_thread;
};
