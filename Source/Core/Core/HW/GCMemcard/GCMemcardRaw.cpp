// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/GCMemcard/GCMemcardRaw.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Timer.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/Sram.h"

#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000

MemoryCard::MemoryCard(const std::string& filename, int card_index, u16 size_mbits)
    : MemoryCardBase(card_index, size_mbits), m_filename(filename)
{
  File::IOFile file(m_filename, "rb");
  if (file)
  {
    // Measure size of the existing memcard file.
    m_memory_card_size = (u32)file.GetSize();
    m_nintendo_card_id = m_memory_card_size / SIZE_TO_Mb;
    m_memcard_data = std::make_unique<u8[]>(m_memory_card_size);
    memset(&m_memcard_data[0], 0xFF, m_memory_card_size);

    INFO_LOG(EXPANSIONINTERFACE, "Reading memory card %s", m_filename.c_str());
    file.ReadBytes(&m_memcard_data[0], m_memory_card_size);
  }
  else
  {
    // Create a new 128Mb memcard
    m_nintendo_card_id = size_mbits;
    m_memory_card_size = size_mbits * SIZE_TO_Mb;

    m_memcard_data = std::make_unique<u8[]>(m_memory_card_size);

    // Fills in the first 5 blocks (MC_HDR_SIZE bytes)
    const CardFlashId& flash_id = g_SRAM.settings_ex.flash_id[Memcard::SLOT_A];
    const bool shift_jis = m_filename.find(".JAP.raw") != std::string::npos;
    const u32 rtc_bias = g_SRAM.settings.rtc_bias;
    const u32 sram_language = static_cast<u32>(g_SRAM.settings.language);
    const u64 format_time =
        Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;
    Memcard::GCMemcard::Format(&m_memcard_data[0], flash_id, size_mbits, shift_jis, rtc_bias,
                               sram_language, format_time);

    // Fills in the remaining blocks
    memset(&m_memcard_data[MC_HDR_SIZE], 0xFF, m_memory_card_size - MC_HDR_SIZE);

    INFO_LOG(EXPANSIONINTERFACE, "No memory card found. A new one was created instead.");
  }

  // Class members (including inherited ones) have now been initialized, so
  // it's safe to startup the flush thread (which reads them).
  m_flush_buffer = std::make_unique<u8[]>(m_memory_card_size);
  m_flush_thread = std::thread(&MemoryCard::FlushThread, this);
}

MemoryCard::~MemoryCard()
{
  if (m_flush_thread.joinable())
  {
    m_flush_trigger.Set();

    m_flush_thread.join();
  }
}

void MemoryCard::CheckPath(std::string& memcardPath, const std::string& gameRegion, bool isSlotA)
{
  std::string ext("." + gameRegion + ".raw");
  if (memcardPath.empty())
  {
    // Use default memcard path if there is no user defined name
    std::string defaultFilename = isSlotA ? GC_MEMCARDA : GC_MEMCARDB;
    memcardPath = File::GetUserPath(D_GCUSER_IDX) + defaultFilename + ext;
  }
  else
  {
    std::string filename = memcardPath;
    std::string region = filename.substr(filename.size() - 7, 3);
    bool hasregion = false;
    hasregion |= region.compare(USA_DIR) == 0;
    hasregion |= region.compare(JAP_DIR) == 0;
    hasregion |= region.compare(EUR_DIR) == 0;
    if (!hasregion)
    {
      // filename doesn't have region in the extension
      if (File::Exists(filename))
      {
        // If the old file exists we are polite and ask if we should copy it
        std::string oldFilename = filename;
        filename.replace(filename.size() - 4, 4, ext);
        if (PanicYesNoT("Memory Card filename in Slot %c is incorrect\n"
                        "Region not specified\n\n"
                        "Slot %c path was changed to\n"
                        "%s\n"
                        "Would you like to copy the old file to this new location?\n",
                        isSlotA ? 'A' : 'B', isSlotA ? 'A' : 'B', filename.c_str()))
        {
          if (!File::Copy(oldFilename, filename))
            PanicAlertT("Copy failed");
        }
      }
      memcardPath = filename;  // Always correct the path!
    }
    else if (region.compare(gameRegion) != 0)
    {
      // filename has region, but it's not == gameRegion
      // Just set the correct filename, the EXI Device will create it if it doesn't exist
      memcardPath = filename.replace(filename.size() - ext.size(), ext.size(), ext);
    }
  }
}

void MemoryCard::FlushThread()
{
  if (!SConfig::GetInstance().bEnableMemcardSdWriting)
  {
    return;
  }

  Common::SetCurrentThreadName(fmt::format("Memcard {} flushing thread", m_card_index).c_str());

  const auto flush_interval = std::chrono::seconds(15);

  while (true)
  {
    // If triggered, we're exiting.
    // If timed out, check if we need to flush.
    bool do_exit = m_flush_trigger.WaitFor(flush_interval);
    if (!do_exit)
    {
      bool is_dirty = m_dirty.TestAndClear();
      if (!is_dirty)
      {
        continue;
      }
    }

    // Opening the file is purposefully done each iteration to ensure the
    // file doesn't disappear out from under us after the first check.
    File::IOFile file(m_filename, "r+b");

    if (!file)
    {
      std::string dir;
      SplitPath(m_filename, &dir, nullptr, nullptr);
      if (!File::IsDirectory(dir))
      {
        File::CreateFullPath(dir);
      }
      file.Open(m_filename, "wb");
    }

    // Note - file may have changed above, after ctor
    if (!file)
    {
      PanicAlertT(
          "Could not write memory card file %s.\n\n"
          "Are you running Dolphin from a CD/DVD, or is the save file maybe write protected?\n\n"
          "Are you receiving this after moving the emulator directory?\nIf so, then you may "
          "need to re-specify your memory card location in the options.",
          m_filename.c_str());

      // Exit the flushing thread - further flushes will be ignored unless
      // the thread is recreated.
      return;
    }

    {
      std::unique_lock<std::mutex> l(m_flush_mutex);
      memcpy(&m_flush_buffer[0], &m_memcard_data[0], m_memory_card_size);
    }
    file.WriteBytes(&m_flush_buffer[0], m_memory_card_size);

    if (do_exit)
      return;

    Core::DisplayMessage(
        fmt::format("Wrote memory card {} contents to {}", m_card_index ? 'B' : 'A', m_filename),
        4000);
  }
}

void MemoryCard::MakeDirty()
{
  m_dirty.Set();
}

s32 MemoryCard::Read(u32 src_address, s32 length, u8* dest_address)
{
  if (!IsAddressInBounds(src_address))
  {
    PanicAlertT("MemoryCard: Read called with invalid source address (0x%x)", src_address);
    return -1;
  }

  memcpy(dest_address, &m_memcard_data[src_address], length);
  return length;
}

s32 MemoryCard::Write(u32 dest_address, s32 length, const u8* src_address)
{
  if (!IsAddressInBounds(dest_address))
  {
    PanicAlertT("MemoryCard: Write called with invalid destination address (0x%x)", dest_address);
    return -1;
  }

  {
    std::unique_lock<std::mutex> l(m_flush_mutex);
    memcpy(&m_memcard_data[dest_address], src_address, length);
  }
  MakeDirty();
  return length;
}

void MemoryCard::ClearBlock(u32 address)
{
  if (address & (Memcard::BLOCK_SIZE - 1) || !IsAddressInBounds(address))
  {
    PanicAlertT("MemoryCard: ClearBlock called on invalid address (0x%x)", address);
    return;
  }
  else
  {
    std::unique_lock<std::mutex> l(m_flush_mutex);
    memset(&m_memcard_data[address], 0xFF, Memcard::BLOCK_SIZE);
  }
  MakeDirty();
}

void MemoryCard::ClearAll()
{
  {
    std::unique_lock<std::mutex> l(m_flush_mutex);
    memset(&m_memcard_data[0], 0xFF, m_memory_card_size);
  }
  MakeDirty();
}

void MemoryCard::DoState(PointerWrap& p)
{
  p.Do(m_card_index);
  p.Do(m_memory_card_size);
  p.DoArray(&m_memcard_data[0], m_memory_card_size);
}
