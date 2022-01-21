// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexCache.h"

#include <algorithm>
#include <array>

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"

// Reference: https://patents.google.com/patent/US6717577B1/en
// Section "Vertex Cache Implementation", and figures 4-6.  Line numbers refer to the PDF at
// https://patentimages.storage.googleapis.com/d8/72/19/4df8b097a84204/US6717577.pdf
// Note that this is a very early patent, and details of it may be wrong.
// But hopefully it's close enough.
// A Japanese version of this patent also exists at https://patents.google.com/patent/JP4644353B2/ja

namespace VertexCache
{
// 8 kilobyte cache memory 400, as 512×128-bit dual ported RAM (p. 9 lines 46-47)
constexpr u32 CACHE_RAM_SIZE = 1024 * 8;
// For an "eight set-associative cache including eight tag lines 402" (p. 9 line 51), this gives one
// kilobyte of memory per set, and thus 128 (0x80) bytes of memory per line.
//
// Libogc instead claims that a cache line is 32 (0x20) bytes:
// https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h#L1917-L1920
// and also claims that indexed load commands run through the vertex cache:
// https://github.com/devkitPro/libogc/blob/master/gc/ogc/gx.h#L2387-L2389
// (though it doesn't explain how those work with the perf counter)

constexpr u32 NUM_CACHE_SETS = 8;
constexpr u32 NUM_CACHE_LINES = 8 * 4;
constexpr u32 CACHE_LINE_SIZE = 0x20;

constexpr u32 CACHE_LINE_OFFSET_BITS = 5;
static_assert(CACHE_LINE_SIZE == 1 << CACHE_LINE_OFFSET_BITS);
constexpr u32 CACHE_LINE_OFFSET_SHIFT = 0;
constexpr u32 CACHE_LINE_OFFSET_MASK = (1 << CACHE_LINE_OFFSET_BITS) - 1;

constexpr u32 SET_NUMBER_BITS = 3;
static_assert(NUM_CACHE_SETS == 1 << SET_NUMBER_BITS);
constexpr u32 SET_NUMBER_SHIFT = CACHE_LINE_OFFSET_SHIFT + CACHE_LINE_OFFSET_BITS;
constexpr u32 SET_NUMBER_MASK = (1 << SET_NUMBER_BITS) - 1;

constexpr u32 TAG_BITS = 32 - CACHE_LINE_OFFSET_BITS - SET_NUMBER_BITS;
constexpr u32 TAG_SHIFT = SET_NUMBER_SHIFT + SET_NUMBER_BITS;
constexpr u32 TAG_MASK = (1 << TAG_BITS) - 1;

// This is a guess, but the size should be proportional to memory latency
constexpr u32 REQUEST_QUEUE_SIZE = 20;

// The patent describes (Figure 5, pp. 9-10 lines 63-5) the format of a memory address, but without
// giving the number of bits.  It says a memory address 450 is passed from the stream parser 208 to
// the vertex cache 212, and is composed of:
//
// - a byte offset into the cache line (452)
// - a tag RAM address (454)
// - a main memory address (456, but not noted in the most obvious place), which is used for
//   comparison with the tag RAM contents (the figure makes this a bit confusing, as the English
//   figure names it as "Compared with Tag RAM Contents", but figure 10 (図１０) in the Japanese
//   patent labels it explictly as "ﾒｲﾝﾒﾓﾘｱﾄﾞﾚｽ" (literally main memory address))
//
// Earlier text says that the vertex cache can be addressed as if it were the entirety of main
// memory (p. 7 lines 51-53), so this is just the way a memory address is split up when being
// interpreted by the vertex cache.
//
// For our purposes we can assign clearer names; instead of "tag RAM address" the middle field is
// "set number" (s), and instead of "main memory address" the last field is "tag" (t).  The byte
// offset is a clear enough name, and will be designated with o.
// Thus, for 7 bits of cache line offset data and 3 bits for set number, an address is like this:
// tttt tttt tttt tttt tttt ttss sooo oooo
// Of course, on a real console there isn't enough RAM for all of the bits in the tag to matter,
// but for ease of implementation, we'll use all of the bits, and also include the set bits,
// since all RAM has the same speed and this will make debugging slightly easier.
// (we can include the set bits since within a given set, the set bits will always be the same,
// so all tags in the given set will have the same set bits)
namespace
{
u32 GetCacheLineOffset(u32 address)
{
  return (address >> CACHE_LINE_OFFSET_SHIFT) & CACHE_LINE_OFFSET_MASK;
}

u32 GetSetNumber(u32 address)
{
  return (address >> SET_NUMBER_SHIFT) & SET_NUMBER_MASK;
}

u32 GetAlignedAddress(u32 address)
{
  return address & ~(CACHE_LINE_OFFSET_MASK << CACHE_LINE_OFFSET_SHIFT);
}

u32 GetTag(u32 address)
{
  // return (address >> TAG_SHIFT) & TAG_MASK;
  return GetAlignedAddress(address);
}
}  // namespace

// Tag RAM 404, 32×16 bit dual ported, one for each tag line (p. 9 lines 52-53)
// "eight set-associative cache including eight tag lines 402" (p. 9 line 51)
//
// I take this to mean that each cache set has eight tag lines, but the actual size seems odd (64
// bytes per tag line, which I have to assume are shared for each set, giving 8 bytes per tag
// line/set pair, which is still more than is needed.)  Even if we use 4 bytes for the tag,
// and inclue the tag status register 406 (p. 9 lines 52-54) (which has no size specified),
// it seems like this is an excessive amount of memory.
// This is also slightly confused by part of the main memory address being treated as the tag RAM
// address (what we are calling the set number), since that implies that all of tag RAM is
// addressable (rather than there being eight separate instances, one for each tag line/each set).

struct TagLine
{
  u32 tag = 0;
  // Tag status register 406, no RAM size given (p. 9 lines 52-54)
  bool valid = false;
  // The patent uses a counter to determine how many queued reads will depend on the contents of
  // this tag line, but we don't have memory latency implemented, so this counter makes less sense.
  // Perhaps a timestamp for when the data in this tag will be populated/no longer needed would work
  // better?
  // u8 counter = 0;

  // Compared to a counter in VertexCache.  Probably not the right implementation.
  u32 last_used = 0;

  // The main cache memory 400 (put here since we don't have different
  // RAM with different speeds, so this makes indexing easier)
  std::array<u8, CACHE_LINE_SIZE> cache_data{};
};

struct CacheSet
{
  std::array<TagLine, NUM_CACHE_LINES> lines{};
  // Returns the line with the given tag, or whichever line has the least references if no line has
  // the given tag.
  TagLine& FindLine(u32 tag);
  void Invalidate();
};

struct VertexCache
{
  std::array<CacheSet, NUM_CACHE_SETS> sets{};

  // Data required to process a particular component is stored within a queue 410 having a depth
  // that is proportional to memory latency.
  // We don't emulate memory latency, but we do keep a queue of recent requests, to keep track of
  // what things have been most recently used.
  // std::array<TagLine*, REQUEST_QUEUE_SIZE> request_queue{};
  // u32 request_queue_index = 0;

  u32 tick = 0;

  u8 ReadByte(u32 addr);
  void ReadData(u32 addr, u32 count, u8* dest);

  void Invalidate();
};

TagLine& CacheSet::FindLine(u32 tag)
{
  // Check if there's already a line with a matching tag (=> hit)
  for (TagLine& line : lines)
  {
    if (line.valid && line.tag == tag)
      return line;
  }
  // Check if there's a line we can populate (=> miss)
  for (TagLine& line : lines)
  {
    if (!line.valid)
      return line;
  }
  // Return whichever line has the lowest last_used
  return *std::min_element(lines.begin(), lines.end(), [](const TagLine& a, const TagLine& b) {
    return a.last_used < b.last_used;
  });
}

u8 VertexCache::ReadByte(u32 addr)
{
  tick++;

  const u32 tag = GetTag(addr);
  TagLine& line = sets[GetSetNumber(addr)].FindLine(tag);
  bool valid = (line.valid && line.tag == tag);
  bool populatable = !line.valid;

  line.last_used = tick;

  if (valid)
  {
    // Happy path: just read from the cache.
    return line.cache_data[GetCacheLineOffset(addr)];
  }
  else if (populatable)
  {
    // Miss, but we can fill the data for an unused line.
    Memory::CopyFromEmu(line.cache_data.data(), GetAlignedAddress(addr), CACHE_LINE_SIZE);

    line.valid = true;
    line.tag = tag;
    return line.cache_data[GetCacheLineOffset(addr)];
  }
  else
  {
    // Miss, and we need to evict a line (already determined by FindLine,
    // so this case isn't special here (but might have timing implications))
    Memory::CopyFromEmu(line.cache_data.data(), GetAlignedAddress(addr), CACHE_LINE_SIZE);

    line.valid = true;
    line.tag = tag;
    return line.cache_data[GetCacheLineOffset(addr)];
  }
}

void VertexCache::ReadData(u32 addr, u32 count, u8* dest)
{
  for (u32 i = 0; i < count; i++)
  {
    dest[i] = ReadByte(addr + i);
  }
}

void CacheSet::Invalidate()
{
  for (TagLine& line : lines)
  {
    line.valid = false;
    // Probably don't actually need to do this, since we only check last_used if valid is false for
    // all lines.  But it doesn't hurt.
    line.last_used = 0;
  }
}

void VertexCache::Invalidate()
{
  tick = 0;

  for (CacheSet& set : sets)
    set.Invalidate();
}

// XXX get rid of these
static VertexCache s_vertex_cache{};

void ReadData(u32 addr, u32 count, u8* dest)
{
  s_vertex_cache.ReadData(addr, count, dest);
}

void Invalidate()
{
  s_vertex_cache.Invalidate();
}
}  // namespace VertexCache
