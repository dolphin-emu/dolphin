// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "Common/FlushThread.h"
#include "Core/HW/GCMemcard/GCMemcard.h"
#include "Core/HW/GCMemcard/GCMemcardBase.h"

class PointerWrap;

class MemoryCard final : public MemoryCardBase
{
public:
  MemoryCard(std::string filename, ExpansionInterface::Slot card_slot,
             u16 size_mbits = Memcard::MBIT_SIZE_MEMORY_CARD_2043);
  ~MemoryCard() override;

  s32 Read(u32 src_address, s32 length, u8* dest_address) override;
  s32 Write(u32 dest_address, s32 length, const u8* src_address) override;
  void ClearBlock(u32 address) override;
  void ClearAll() override;
  void DoState(PointerWrap& p) override;

private:
  void FlushToFile(const std::string& filename);
  void MakeDirty();

  bool IsAddressInBounds(u32 address, u32 length) const
  {
    const u64 end_address = static_cast<u64>(address) + static_cast<u64>(length);
    return end_address <= static_cast<u64>(m_memory_card_size);
  }

  std::unique_ptr<u8[]> m_memcard_data;
  std::unique_ptr<u8[]> m_flush_buffer;
  Common::FlushThread m_flush_thread;
  std::mutex m_flush_mutex;
  u32 m_memory_card_size;
};
