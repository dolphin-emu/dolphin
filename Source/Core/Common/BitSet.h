// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <bit>
#include <cstddef>
#include <initializer_list>
#include <type_traits>

#include "Common/CommonTypes.h"

namespace Common
{
// Similar to std::bitset, this is a class which encapsulates a bitset, i.e.
// using the set bits of an integer to represent a set of integers.  Like that
// class, it acts like an array of bools:
//     BitSet32 bs;
//     bs[1] = true;
// but also like the underlying integer ([0] = least significant bit):
//     BitSet32 bs2 = ...;
//     bs = (bs ^ bs2) & BitSet32(0xffff);
// The following additional functionality is provided:
// - Construction using an initializer list.
//     BitSet bs { 1, 2, 4, 8 };
// - Efficiently iterating through the set bits:
//     for (int i : bs)
//         [i is the *index* of a set bit]
//   (This uses the appropriate CPU instruction to find the next set bit in one
//   operation.)
// - Counting set bits using .Count() - see comment on that method.

template <typename IntTy>
class BitSet
{
  static_assert(!std::is_signed<IntTy>::value, "BitSet should not be used with signed types");

public:
  // A reference to a particular bit, returned from operator[].
  class Ref
  {
  public:
    constexpr Ref(Ref&& other) : m_bs(other.m_bs), m_mask(other.m_mask) {}
    constexpr Ref(BitSet* bs, IntTy mask) : m_bs(bs), m_mask(mask) {}
    constexpr operator bool() const { return (m_bs->m_val & m_mask) != 0; }
    bool operator=(bool set)
    {
      m_bs->m_val = (m_bs->m_val & ~m_mask) | (set ? m_mask : 0);
      return set;
    }

  private:
    BitSet* m_bs;
    IntTy m_mask;
  };

  // A STL-like iterator is required to be able to use range-based for loops.
  class Iterator
  {
  public:
    constexpr Iterator(const Iterator& other) : m_val(other.m_val), m_bit(other.m_bit) {}
    constexpr Iterator(IntTy val, int bit) : m_val(val), m_bit(bit) {}
    Iterator& operator=(Iterator other)
    {
      new (this) Iterator(other);
      return *this;
    }
    Iterator& operator++()
    {
      if (m_val == 0)
      {
        m_bit = -1;
      }
      else
      {
        int bit = std::countr_zero(m_val);
        m_val &= ~(IntTy{1} << bit);
        m_bit = bit;
      }
      return *this;
    }
    Iterator operator++(int)
    {
      Iterator other(*this);
      ++*this;
      return other;
    }
    constexpr int operator*() const { return m_bit; }
    constexpr bool operator==(Iterator other) const { return m_bit == other.m_bit; }
    constexpr bool operator!=(Iterator other) const { return m_bit != other.m_bit; }

  private:
    IntTy m_val;
    int m_bit;
  };

  constexpr BitSet() = default;
  constexpr explicit BitSet(IntTy val) : m_val(val) {}
  constexpr BitSet(std::initializer_list<int> init)
  {
    for (int bit : init)
      m_val |= IntTy{1} << bit;
  }

  constexpr static BitSet AllTrue(size_t count)
  {
    return BitSet(count == sizeof(IntTy) * 8 ? ~IntTy{0} : ((IntTy{1} << count) - 1));
  }

  Ref operator[](size_t bit) { return Ref(this, IntTy{1} << bit); }
  constexpr const Ref operator[](size_t bit) const { return (*const_cast<BitSet*>(this))[bit]; }
  constexpr bool operator==(BitSet other) const { return m_val == other.m_val; }
  constexpr bool operator!=(BitSet other) const { return m_val != other.m_val; }
  constexpr bool operator<(BitSet other) const { return m_val < other.m_val; }
  constexpr bool operator>(BitSet other) const { return m_val > other.m_val; }
  constexpr BitSet operator|(BitSet other) const { return BitSet(m_val | other.m_val); }
  constexpr BitSet operator&(BitSet other) const { return BitSet(m_val & other.m_val); }
  constexpr BitSet operator^(BitSet other) const { return BitSet(m_val ^ other.m_val); }
  constexpr BitSet operator~() const { return BitSet(~m_val); }
  constexpr BitSet operator<<(IntTy shift) const { return BitSet(m_val << shift); }
  constexpr BitSet operator>>(IntTy shift) const { return BitSet(m_val >> shift); }
  constexpr explicit operator bool() const { return m_val != 0; }
  BitSet& operator|=(BitSet other) { return *this = *this | other; }
  BitSet& operator&=(BitSet other) { return *this = *this & other; }
  BitSet& operator^=(BitSet other) { return *this = *this ^ other; }
  BitSet& operator<<=(IntTy shift) { return *this = *this << shift; }
  BitSet& operator>>=(IntTy shift) { return *this = *this >> shift; }
  // Warning: Even though on modern CPUs this is a single fast instruction,
  // Dolphin's official builds do not currently assume POPCNT support on x86,
  // so slower explicit bit twiddling is generated.  Still should generally
  // be faster than a loop.
  constexpr unsigned int Count() const { return std::popcount(m_val); }
  constexpr Iterator begin() const { return ++Iterator(m_val, 0); }
  constexpr Iterator end() const { return Iterator(m_val, -1); }
  IntTy m_val{};
};
}  // namespace Common

using BitSet8 = Common::BitSet<u8>;
using BitSet16 = Common::BitSet<u16>;
using BitSet32 = Common::BitSet<u32>;
using BitSet64 = Common::BitSet<u64>;
