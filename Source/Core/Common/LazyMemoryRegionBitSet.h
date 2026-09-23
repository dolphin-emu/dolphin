// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>

#include "Common/Assert.h"
#include "Common/MemArena.h"

namespace Common
{
// A bit set backed by a LazyMemoryRegion.
class LazyMemoryRegionBitSet final
{
public:
  // Allocates backing memory and creates a bit set.
  // Allocating backing memory may fail; use IsValid to check for success.
  LazyMemoryRegionBitSet(size_t bits);

  bool IsValid() const { return m_valid; }

  void Clear();

  bool TestBit(size_t bit) const
  {
    CheckBitIndexReadable(bit);
    return (m_pointer[bit / 32] & (1 << (bit % 32))) != 0;
  }

  void SetBit(size_t bit)
  {
    CheckBitIndexWriteable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
    m_pointer[bit / 32] |= (1 << (bit % 32));
  }

  void ClearBit(size_t bit)
  {
    CheckBitIndexWriteable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
    m_pointer[bit / 32] &= ~(1 << (bit % 32));
  }

  void SetBits(size_t bits_start, size_t bits_end);

  void ClearBits(size_t bits_start, size_t bits_end);

  const u32* GetRawMemory() const { return m_pointer; }

private:
  LazyMemoryRegion m_region;
  u32* m_pointer;
  size_t m_bits;
  bool m_valid;

  bool Create(size_t bits);

  void CheckBitIndexReadable(size_t bit) const
  {
    DEBUG_ASSERT_MSG(COMMON, m_valid, "LazyMemoryRegionBitSet isn't valid");
    DEBUG_ASSERT_MSG(COMMON, bit < m_bits,
                     "Tried to access bit {} in a LazyMemoryRegionBitSet with only {} bits", bit,
                     m_bits);
  }

  void CheckBitIndexWriteable(size_t bit)
  {
    CheckBitIndexReadable(bit);
    m_region.EnsureMemoryPageWritable(bit / 8);
  }

  void CheckBitIndicesWriteable(size_t bits_start, size_t bits_end);
};

}  // namespace Common
