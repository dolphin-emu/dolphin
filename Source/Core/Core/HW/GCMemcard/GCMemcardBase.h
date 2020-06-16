// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "Core/HW/GCMemcard/GCMemcard.h"

class PointerWrap;

class MemoryCardBase
{
public:
  explicit MemoryCardBase(int card_index = 0, int size_mbits = Memcard::MBIT_SIZE_MEMORY_CARD_2043)
      : m_card_index(card_index), m_nintendo_card_id(size_mbits)
  {
  }
  virtual ~MemoryCardBase() = default;
  virtual s32 Read(u32 src_address, s32 length, u8* dest_address) = 0;
  virtual s32 Write(u32 dest_address, s32 length, const u8* src_address) = 0;
  virtual void ClearBlock(u32 address) = 0;
  virtual void ClearAll() = 0;
  virtual void DoState(PointerWrap& p) = 0;
  u32 GetCardId() const { return m_nintendo_card_id; }
  bool IsAddressInBounds(u32 address) const { return address <= (m_memory_card_size - 1); }

protected:
  int m_card_index;
  u16 m_nintendo_card_id;
  u32 m_memory_card_size;
};
