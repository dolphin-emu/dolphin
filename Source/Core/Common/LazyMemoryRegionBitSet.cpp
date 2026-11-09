// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/LazyMemoryRegionBitSet.h"

#include <cstring>

#include "Common/Align.h"
#include "Common/MsgHandler.h"

namespace Common
{
LazyMemoryRegionBitSet::LazyMemoryRegionBitSet(size_t bits) : m_bits(bits), m_valid(Create(bits))
{
}

bool LazyMemoryRegionBitSet::Create(size_t bits)
{
  m_pointer = reinterpret_cast<u32*>(m_region.Create(Common::AlignUp(bits, 32) / 8));
  if (!m_pointer)
  {
    PanicAlertFmt("LazyMemoryRegionBitSet failed to create backing region");
    return false;
  }

  return true;
}

void LazyMemoryRegionBitSet::Clear()
{
  m_region.Clear();
}

void LazyMemoryRegionBitSet::CheckBitIndicesWriteable(size_t bits_start, size_t bits_end)
{
  CheckBitIndexReadable(bits_end - 1);
  const size_t start_byte = Common::AlignDown(bits_start, 32) / 8;
  const size_t end_byte = Common::AlignUp(bits_end, 32) / 8;
  m_region.EnsureMemoryPagesWritable(start_byte, end_byte - start_byte);
}

void LazyMemoryRegionBitSet::SetBits(size_t bits_start, size_t bits_end)
{
  if (bits_start >= bits_end)
    return;

  CheckBitIndicesWriteable(bits_start, bits_end);

  const u32 extra_first_word = 0xFFFFFFFF << bits_start % 32;
  const u32 extra_last_word = ~(0xFFFFFFFF << bits_end % 32);

  if (bits_start / 32 == bits_end / 32)
  {
    m_pointer[bits_start / 32] |= extra_first_word & extra_last_word;
    return;
  }

  if (bits_start % 32 != 0)
    m_pointer[bits_start / 32] |= extra_first_word;

  const size_t start_index = Common::DivideRoundingUp(bits_start, 32);
  const size_t end_index = bits_end / 32;
  std::memset(m_pointer + start_index, 0xFF, (end_index - start_index) * 4);

  if (bits_end % 32 != 0)
    m_pointer[bits_end / 32] |= extra_last_word;
}

void LazyMemoryRegionBitSet::ClearBits(size_t bits_start, size_t bits_end)
{
  if (bits_start >= bits_end)
    return;

  CheckBitIndicesWriteable(bits_start, bits_end);

  const u32 extra_first_word = ~(0xFFFFFFFF << bits_start % 32);
  const u32 extra_last_word = 0xFFFFFFFF << bits_end % 32;

  if (bits_start / 32 == bits_end / 32)
  {
    m_pointer[bits_start / 32] &= extra_first_word | extra_last_word;
    return;
  }

  if (bits_start % 32 != 0)
    m_pointer[bits_start / 32] &= extra_first_word;

  const size_t start_index = Common::DivideRoundingUp(bits_start, 32);
  const size_t end_index = bits_end / 32;
  std::memset(m_pointer + start_index, 0x00, (end_index - start_index) * sizeof(u32));

  if (bits_end % 32 != 0)
    m_pointer[bits_end / 32] &= extra_last_word;
}

}  // namespace Common
