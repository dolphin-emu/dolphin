// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TmemBase.h"

namespace TmemBase
{
// On TMEM configuration changed:
// 1. invalidate stage.

struct texture_unit_state
{
  enum State
  {
    INVALID, // Configuration has changed
    VALID, // But not cached due to either being too big, or overlapping with another valid texture unit
    CACHED,
  };

  TexImage1 even;
  TexImage2 odd;
  State state;

  int even_base;
  int even_size;
  int odd_base;
  int odd_size;
};
static std::array<texture_unit_state, 8> s_unit;

void ConfigurationChanged(int bp_addr, int config)
{
  // Extract bits encoding texture unit
  u8 unit = ((bp_addr & 0x20) >> 3) | (bp_addr & 3);
  bool even = bp_addr < BPMEM_TX_SETIMAGE2;

  texture_unit_state& unit_state = s_unit[unit];

  if (even)
  {
    unit_state.even.hex = config;
  }
  else
  {
    unit_state.odd.hex = config;
  }
  unit_state.state = texture_unit_state::INVALID;
}

// On invalidate cache
// 1. invalidate all

void Invalidate(u32 param)
{
  // The excat arguments of Invalidate commands is currently unknown.
  // It appears to contain the TMEM address and a size.

  // For simplicity, we will just invalidate everything
  for (auto& unit : s_unit)
  {
    unit.state = texture_unit_state::INVALID;
  }
}

// On bind:
// 1. store the memory region(s) bound
// 2. if texture size is small enough to fit in region AND there is no overlap, mark as cached.
// 3. If there is cached overlap, invalidate.

static inline u32 calculate_unit_size(TexImage1 config)
{
  // These are the only cache sizes supported by the sdk
  if (config.cache_width == config.cache_height)
  {
    switch (config.cache_width)
    {
    case 3:  // 32KB
      return 32 * 1024;
    case 4:  // 128KB
      return 128 * 1024;
    case 5:  // 512KB
      return 512 * 1024;
    default:
      break;
    }
  }

  // However, the registers allow a much larger amount of configurablity.
  // Maybe other sizes are broken?
  // Until hardware tests are done, this is a guess at the size algorithm

  return 512 * (1 << config.cache_width) * (1 << config.cache_height);
}

static inline u32 calculate_unit_size(TexImage2 config)
{
  TexImage1 casted_config;
  casted_config.hex = config.hex;
  return calculate_unit_size(casted_config);
}

static bool overlap(int a, int a_size, int b, int b_size)
{
  return (a <= b && (a + a_size) > b) || (b <= a && (b + b_size) > a);
}

static bool process_overlaps_single(u8 unit_id, int base, int size)
{
  bool has_overlap = false;
  // We are just going to do a linear search
  for (int i = 0; i < s_unit.size(); i++)
  {
    texture_unit_state& other_unit = s_unit[i];
    if (i != unit_id && other_unit.state != texture_unit_state::INVALID)
    {
      if (overlap(base, size, other_unit.even_base, other_unit.even_size) ||
          (other_unit.odd_size && overlap(base, size, other_unit.odd_base, other_unit.odd_size)))
      {
        has_overlap = true;
        other_unit.state = texture_unit_state::VALID;
      }
    }
  }
  return has_overlap;
}

// Scans though all other texture units that are currently enabled.
// If there are any overlaps, 
static void process_overlaps(u8 unit_id)
{
  texture_unit_state& unit = s_unit[unit_id];

  // First, check for self-overlaps.
  bool overlaps = unit.odd_size && overlap(unit.even_base, unit.even_size, unit.odd_base, unit.odd_size);

  // Check for overlaps with even bank 
  overlaps |= process_overlaps_single(unit_id, unit.even_base, unit.even_size);
  if (unit.odd_size)
  {
    overlaps |= process_overlaps_single(unit_id, unit.odd_base, unit.odd_size);
  }

  if (overlaps)
    unit.state = texture_unit_state::VALID;
}

void Bind(u8 unit, int width, int height, bool is_mipmapped, bool is_32_bit)
{
  texture_unit_state& unit_state = s_unit[unit];

  // All textures use the even bank.
  // It holds the level 1 mipmap
  unit_state.even_base = unit_state.even.tmem_even << 5;
  unit_state.even_size = calculate_unit_size(unit_state.even);
  unit_state.odd_size = 0;  // reset

  bool fits = (width * height * 32) <= unit_state.even_size;

  if (is_mipmapped || is_32_bit)
  {
    // And the odd bank is enabled when either mipmapping is enabled
    // It holds the Alpha and Red channels of 32 bit textures or the odd layers of a mipmapped
    // texture
    unit_state.odd_base = unit_state.odd.tmem_odd << 5;
    unit_state.odd_size = calculate_unit_size(unit_state.odd);

    fits = fits && (width * height * 32) <= unit_state.odd_size;
  }
  else
  {
    unit_state.odd_size = 0;
  }

  if (is_mipmapped)
  {
    // When mipmapping is enabled, the even back is doubled in size
    // The extended region holds the remaining even mipmap layers
    unit_state.even_size *= 2;

    if (is_32_bit)
    {
      // When a 32bit texture is mipmapped, the odd bank is also doubled in size
      unit_state.odd_size *= 2;
    }
  }

  unit_state.state = fits ? texture_unit_state::CACHED : texture_unit_state::VALID;


  // check for overlaps, which might mark a texture as uncacheable
  process_overlaps(unit);
}

bool IsCached(u8 unit)
{
  return s_unit[unit].state == texture_unit_state::CACHED;
}

}  // namespace TmemBase
