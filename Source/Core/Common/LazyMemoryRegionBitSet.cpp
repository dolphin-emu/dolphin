// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/LazyMemoryRegionBitSet.h"

#include <cstring>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/MsgHandler.h"

namespace Common
{
LazyMemoryRegionBitSet::LazyMemoryRegionBitSet(size_t bits) : m_bits(bits), m_valid(Create(bits))
{
}

bool LazyMemoryRegionBitSet::Create(size_t bits)
{
  const size_t page_size = MemArena().GetPageSize();
  if (bits % (page_size * 8) != 0)
  {
    PanicAlertFmt(
        "LazyMemoryRegionBitSet size of {} bits is not aligned with page size of {} bytes", bits,
        page_size);
    return false;
  }

  m_pointer = reinterpret_cast<u8*>(m_region.Create(bits / 8));
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

void LazyMemoryRegionBitSet::SetBits(size_t bits_start, size_t bits_end)
{
  if (bits_start == bits_end)
    return;

  RangeCheck(bits_end);

  m_region.EnsureMemoryPagesWritable(bits_start / 8, (bits_end - bits_start) / 8);

  const size_t aligned_start = Common::AlignUp(bits_start, 8);
  const size_t aligned_end = Common::AlignDown(bits_end, 8);

  const u8 extra_first_byte = 0xFF << (bits_start - (aligned_start - 8));
  const u8 extra_last_byte = 0xFF >> ((aligned_end + 8) - bits_end);

  if (bits_start / 8 == bits_end / 8)
  {
    m_pointer[bits_start / 8] |= extra_first_byte & extra_last_byte;
    return;
  }

  if (extra_first_byte != 0)
    m_pointer[bits_start / 8] |= extra_first_byte;

  std::memset(m_pointer + (aligned_start / 8), 0xFF, (aligned_end - aligned_start) / 8);

  if (extra_last_byte != 0)
    m_pointer[bits_end / 8] |= extra_last_byte;
}

void LazyMemoryRegionBitSet::ClearBits(size_t bits_start, size_t bits_end)
{
  if (bits_start == bits_end)
    return;

  RangeCheck(bits_end);

  m_region.EnsureMemoryPagesWritable(bits_start / 8, (bits_end - bits_start) / 8);

  const size_t aligned_start = Common::AlignUp(bits_start, 8);
  const size_t aligned_end = Common::AlignDown(bits_end, 8);

  const u8 extra_first_byte = 0xFF << (bits_start - (aligned_start - 8));
  const u8 extra_last_byte = 0xFF >> ((aligned_end + 8) - bits_end);

  if (bits_start / 8 == bits_end / 8)
  {
    m_pointer[bits_start / 8] &= ~(extra_first_byte & extra_last_byte);
    return;
  }

  if (extra_first_byte != 0)
    m_pointer[bits_start / 8] &= ~extra_first_byte;

  std::memset(m_pointer + (aligned_start / 8), 0x00, (aligned_end - aligned_start) / 8);

  if (extra_last_byte != 0)
    m_pointer[bits_end / 8] &= ~extra_last_byte;
}

}  // namespace Common
