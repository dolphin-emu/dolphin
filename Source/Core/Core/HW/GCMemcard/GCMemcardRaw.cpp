// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/GCMemcard/GCMemcardRaw.h"

#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Timer.h"

#include "Core/Config/SessionSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_DeviceIPL.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/Sram.h"
#include "Core/System.h"

#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000

MemoryCard::MemoryCard(std::string filename, ExpansionInterface::Slot card_slot, u16 size_mbits)
    : MemoryCardBase(card_slot, size_mbits)
{
  File::IOFile file(filename, "rb");
  if (file)
  {
    // Measure size of the existing memcard file.
    m_memory_card_size = (u32)file.GetSize();
    m_nintendo_card_id = m_memory_card_size / SIZE_TO_Mb;
    m_memcard_data = std::make_unique<u8[]>(m_memory_card_size);
    memset(&m_memcard_data[0], 0xFF, m_memory_card_size);

    INFO_LOG_FMT(EXPANSIONINTERFACE, "Reading memory card {}", filename);
    file.ReadBytes(&m_memcard_data[0], m_memory_card_size);
  }
  else
  {
    // Create a new 128Mb memcard
    m_nintendo_card_id = size_mbits;
    m_memory_card_size = size_mbits * SIZE_TO_Mb;

    m_memcard_data = std::make_unique<u8[]>(m_memory_card_size);

    // Fills in the first 5 blocks (MC_HDR_SIZE bytes)
    const auto& sram = Core::System::GetInstance().GetSRAM();
    const CardFlashId& flash_id = sram.settings_ex.flash_id[Memcard::SLOT_A];
    const bool shift_jis = filename.find(".JAP.raw") != std::string::npos;
    const u32 rtc_bias = sram.settings.rtc_bias;
    const u32 sram_language = static_cast<u32>(sram.settings.language);
    const u64 format_time =
        Common::Timer::GetLocalTimeSinceJan1970() - ExpansionInterface::CEXIIPL::GC_EPOCH;
    Memcard::GCMemcard::Format(&m_memcard_data[0], flash_id, size_mbits, shift_jis, rtc_bias,
                               sram_language, format_time);

    // Fills in the remaining blocks
    memset(&m_memcard_data[MC_HDR_SIZE], 0xFF, m_memory_card_size - MC_HDR_SIZE);

    INFO_LOG_FMT(EXPANSIONINTERFACE, "No memory card found. A new one was created instead.");
  }

  m_flush_buffer = std::make_unique<u8[]>(m_memory_card_size);

  if (Config::Get(Config::SESSION_SAVE_DATA_WRITABLE))
  {
    m_flush_thread.Reset(fmt::format("Memcard {} flushing thread", m_card_slot),
                         std::bind_front(&MemoryCard::FlushToFile, this, std::move(filename)));

    m_flush_thread.SetFlushDelay(std::chrono::seconds{1});
  }
}

MemoryCard::~MemoryCard() = default;

void MemoryCard::FlushToFile(const std::string& filename)
{
  File::IOFile file(filename, "r+b");

  if (!file)
  {
    std::string dir;
    SplitPath(filename, &dir, nullptr, nullptr);
    if (!File::IsDirectory(dir))
    {
      File::CreateFullPath(dir);
    }
    file.Open(filename, "wb");
  }

  // Note - file may have changed above, after ctor
  if (!file)
  {
    PanicAlertFmtT(
        "Could not write memory card file {0}.\n\n"
        "Are you running Dolphin from a CD/DVD, or is the save file maybe write protected?\n\n"
        "Are you receiving this after moving the emulator directory?\nIf so, then you may "
        "need to re-specify your memory card location in the options.",
        filename);

    // This flush is unsuccessful.
    return;
  }

  {
    std::unique_lock l(m_flush_mutex);
    memcpy(&m_flush_buffer[0], &m_memcard_data[0], m_memory_card_size);
  }
  file.WriteBytes(&m_flush_buffer[0], m_memory_card_size);

  Core::DisplayMessage(fmt::format("Wrote to Memory Card {}",
                                   m_card_slot == ExpansionInterface::Slot::A ? 'A' : 'B'),
                       4000);
}

void MemoryCard::MakeDirty()
{
  m_flush_thread.SetDirty();
}

s32 MemoryCard::Read(u32 src_address, s32 length, u8* dest_address)
{
  if (!IsAddressInBounds(src_address, length))
  {
    PanicAlertFmtT("MemoryCard: Read called with invalid source address ({0:#x})", src_address);
    return -1;
  }

  memcpy(dest_address, &m_memcard_data[src_address], length);
  return length;
}

s32 MemoryCard::Write(u32 dest_address, s32 length, const u8* src_address)
{
  if (!IsAddressInBounds(dest_address, length))
  {
    PanicAlertFmtT("MemoryCard: Write called with invalid destination address ({0:#x})",
                   dest_address);
    return -1;
  }

  {
    std::unique_lock l(m_flush_mutex);
    memcpy(&m_memcard_data[dest_address], src_address, length);
  }
  MakeDirty();
  return length;
}

void MemoryCard::ClearBlock(u32 address)
{
  if (address & (Memcard::BLOCK_SIZE - 1) || !IsAddressInBounds(address, Memcard::BLOCK_SIZE))
  {
    PanicAlertFmtT("MemoryCard: ClearBlock called on invalid address ({0:#x})", address);
    return;
  }

  {
    std::unique_lock l(m_flush_mutex);
    memset(&m_memcard_data[address], 0xFF, Memcard::BLOCK_SIZE);
  }
  MakeDirty();
}

void MemoryCard::ClearAll()
{
  {
    std::unique_lock l(m_flush_mutex);
    memset(&m_memcard_data[0], 0xFF, m_memory_card_size);
  }
  MakeDirty();
}

void MemoryCard::DoState(PointerWrap& p)
{
  p.Do(m_card_slot);
  p.Do(m_memory_card_size);
  p.DoArray(&m_memcard_data[0], m_memory_card_size);
}
