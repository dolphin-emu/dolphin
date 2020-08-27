// This file is under the public domain.

#pragma once

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include "Common/CommonTypes.h"

#ifdef _WIN32

#include <intrin.h>

namespace Common
{
template <typename T>
constexpr int CountSetBits(T v)
{
  // from https://graphics.stanford.edu/~seander/bithacks.html
  // GCC has this built in, but MSVC's intrinsic will only emit the actual
  // POPCNT instruction, which we're not depending on
  v = v - ((v >> 1) & (T) ~(T)0 / 3);
  v = (v & (T) ~(T)0 / 15 * 3) + ((v >> 2) & (T) ~(T)0 / 15 * 3);
  v = (v + (v >> 4)) & (T) ~(T)0 / 255 * 15;
  return (T)(v * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * 8;
}
inline int LeastSignificantSetBit(u8 val)
{
  unsigned long index;
  _BitScanForward(&index, val);
  return (int)index;
}
inline int LeastSignificantSetBit(u16 val)
{
  unsigned long index;
  _BitScanForward(&index, val);
  return (int)index;
}
inline int LeastSignificantSetBit(u32 val)
{
  unsigned long index;
  _BitScanForward(&index, val);
  return (int)index;
}
inline int LeastSignificantSetBit(u64 val)
{
  unsigned long index;
  _BitScanForward64(&index, val);
  return (int)index;
}
#else
namespace Common
{
constexpr int CountSetBits(u8 val)
{
  return __builtin_popcount(val);
}
constexpr int CountSetBits(u16 val)
{
  return __builtin_popcount(val);
}
constexpr int CountSetBits(u32 val)
{
  return __builtin_popcount(val);
}
constexpr int CountSetBits(u64 val)
{
  return __builtin_popcountll(val);
}
inline int LeastSignificantSetBit(u8 val)
{
  return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u16 val)
{
  return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u32 val)
{
  return __builtin_ctz(val);
}
inline int LeastSignificantSetBit(u64 val)
{
  return __builtin_ctzll(val);
}
#endif

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

// TODO: use constexpr when MSVC gets out of the Dark Ages

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
        int bit = LeastSignificantSetBit(m_val);
        m_val &= ~(1 << bit);
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

  constexpr BitSet() : m_val(0) {}
  constexpr explicit BitSet(IntTy val) : m_val(val) {}
  BitSet(std::initializer_list<int> init)
  {
    m_val = 0;
    for (int bit : init)
      m_val |= (IntTy)1 << bit;
  }

  constexpr static BitSet AllTrue(size_t count)
  {
    return BitSet(count == sizeof(IntTy) * 8 ? ~(IntTy)0 : (((IntTy)1 << count) - 1));
  }

  Ref operator[](size_t bit) { return Ref(this, (IntTy)1 << bit); }
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
  constexpr unsigned int Count() const { return CountSetBits(m_val); }
  constexpr Iterator begin() const { return ++Iterator(m_val, 0); }
  constexpr Iterator end() const { return Iterator(m_val, -1); }
  IntTy m_val;
};
}  // namespace Common

using BitSet8 = Common::BitSet<u8>;
using BitSet16 = Common::BitSet<u16>;
using BitSet32 = Common::BitSet<u32>;
using BitSet64 = Common::BitSet<u64>;
