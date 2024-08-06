// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/TMEM.h"

#include <array>

#include "Common/ChunkFile.h"
#include "VideoCommon/BPMemory.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TMEM emulation tracks which textures should be cached in TMEM on a real console.
// There are two good reasons to do this:
//
// 1. Some games deliberately avoid invalidating a texture, overwrite it with an EFB copy,
//    and then expect the original texture to still be found in TMEM for another draw call.
//    Spyro: A Hero's Tail is known for using such overwritten textures.
//    However, other games like:
//      * Sonic Riders
//      * Metal Arms: Glitch in the System
//      * Godzilla: Destroy All Monsters Melee
//      * NHL Slapshot
//      * Tak and the Power of Juju
//      * Night at the Museum: Battle of the Smithsonian
//      * 428: Fūsa Sareta Shibuya de
//    are known to (accidentally or deliberately) avoid invalidating and then expect the pattern
//    of the draw and the fact that the whole texture doesn't fit in TMEM to self-invalidate the
//    texture. These are usually full-screen efb copies.
//    So we must track the size of the textures as an heuristic to see if they will self-invalidate
//    or not.
//
// 2. It actually improves Dolphin's performance in safer texture hashing modes, by reducing the
//    amount of times a texture needs to be hashed when reused in subsequent draws.
//
// As a side-effect, TMEM emulation also tracks if the texture unit configuration has changed at
// all, which Dolphin's TextureCache takes advantage of.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Checking if a texture fits in TMEM or not is complicated by the fact that Flipper's TMEM is quite
// configurable.
// Each of the eight texture units has two banks (even and odd) that can be pointed at any offset
// and set to any size. It is completely valid to have overlapping banks, and performance can be
// improved by overlapping the caches of texture units that are drawing the same textures.
//
// For trilinear textures, the even/odd banks contain the even/odd LODs of the texture. TMEM has two
// banks of 512KB each, covering the upper and lower halves of TMEM's address space. The two banks
// be accessed simultaneously, allowing a trilinear texture sample to be completed at the same cost
// as a bilinear sample, assuming the even and odd banks are mapped onto different banks.
//
// 32bit textures are actually stored as two 16bit textures in separate banks, allowing a bilinear
// sample of a 32bit texture at the same cost as a 16bit bilinear/trilinear sample. A trilinear
// sample of a 32bit texture costs more.
//
// TODO: I'm not sure if it's valid for a texture unit's even and odd banks to overlap. There might
//       actually be a hard requirement for even and odd banks to live in different banks of TMEM.
//
// Note: This is still very much a heuristic.
//       Actually knowing if a texture is partially or fully cached within TMEM would require
//       extensive software rasterization, or sampler feedback from a hardware backend.
//
////////////////////////////////////////////////////////////////////////////////////////////////////

namespace TMEM
{
struct TextureUnitState
{
  enum class State
  {
    // Cache is invalid. Configuration has changed
    INVALID,

    // Valid, but not cached due to either being too big, or overlapping with another texture unit
    VALID,

    // Texture unit has cached all of the previous draw
    CACHED,
  };

  struct BankConfig
  {
    u32 width = 0;
    u32 height = 0;
    u32 base = 0;
    u32 size = 0;
    bool Overlaps(const BankConfig& other) const;
  };

  BankConfig even = {};
  BankConfig odd = {};
  State state = State::INVALID;

  bool Overlaps(const TextureUnitState& other) const;
};

static u32 CalculateUnitSize(TextureUnitState::BankConfig bank_config);

static std::array<TextureUnitState, 8> s_unit;

// On TMEM configuration changed:
// 1. invalidate stage.

void ConfigurationChanged(const TexUnitAddress bp_addr, const u32 config)
{
  auto& [even_unit_state, odd_unit_state, state] = s_unit[bp_addr.GetUnitID()];

  // If anything has changed, we can't assume existing state is still valid.
  state = TextureUnitState::State::INVALID;

  // Note: BPStructs has already filtered out NOP changes before calling us
  switch (bp_addr.Reg)
  {
  case TexUnitAddress::Register::SETIMAGE1:
  {
    // Image Type and Even bank's Cache Height, Cache Width, TMEM Offset
    const TexImage1 even = {.hex = config};
    even_unit_state = {even.cache_width, even.cache_height, even.tmem_even << 5, 0};
    break;
  }
  case TexUnitAddress::Register::SETIMAGE2:
  {
    // Odd bank's Cache Height, Cache Width, TMEM Offset
    const TexImage2 odd = {.hex = config};
    odd_unit_state = {odd.cache_width, odd.cache_height, odd.tmem_odd << 5, 0};
    break;
  }
  default:
    // Something else has changed
    return;
  }
}

void InvalidateAll()
{
  for (auto& [_even, _odd, state] : s_unit)
  {
    state = TextureUnitState::State::INVALID;
  }
}

// On invalidate cache:
// 1. invalidate all texture units.

void Invalidate([[maybe_unused]] u32 param)
{
  // The exact arguments of Invalidate commands is currently unknown.
  // It appears to contain the TMEM address and a size.

  // For simplicity, we will just invalidate everything
  InvalidateAll();
}

// On bind:
// 1. use mipmapping/32bit status to calculate final sizes
// 2. if texture size is small enough to fit in region mark as cached.
//    otherwise, mark as valid

void Bind(const u32 unit, const int width, const int height, const bool is_mipmapped, const bool is_32_bit)
{
  auto& [even, odd, state] = s_unit[unit];

  // All textures use the even bank.
  // It holds the level 0 mipmap (and other even mipmap LODs, if mipmapping is enabled)
  even.size = CalculateUnitSize(even);

  bool fits = (width * height * 32U) <= even.size;

  if (is_mipmapped || is_32_bit)
  {
    // And the odd bank is enabled when either mipmapping is enabled or the texture is 32 bit
    // It holds the Alpha and Red channels of 32 bit textures or the odd layers of a mipmapped
    // texture
    odd.size = CalculateUnitSize(odd);

    fits = fits && (width * height * 32U) <= odd.size;
  }
  else
  {
    odd.size = 0;
  }

  if (is_mipmapped)
  {
    // TODO: This is what games appear to expect from hardware. But seems odd, as it doesn't line up
    //       with how much extra memory is required for mipmapping, just 33% more.
    //       Hardware testing is required to see exactly what gets used.

    // When mipmapping is enabled, the even bank is doubled in size
    // The extended region holds the remaining even mipmap layers
    even.size *= 2;

    if (is_32_bit)
    {
      // When a 32bit texture is mipmapped, the odd bank is also doubled in size
      odd.size *= 2;
    }
  }

  state = fits ? TextureUnitState::State::CACHED : TextureUnitState::State::VALID;
}

static u32 CalculateUnitSize(const TextureUnitState::BankConfig bank_config)
{
  const u32 width = bank_config.width;
  const u32 height = bank_config.height;

  // These are the only cache sizes supported by the sdk
  if (width == height)
  {
    switch (width)
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

  return 512 * (1 << width) * (1 << height);
}

bool TextureUnitState::BankConfig::Overlaps(const BankConfig& other) const
{
  if (size == 0 || other.size == 0)
    return false;
  return (base <= other.base && (base + size) > other.base) ||
         (other.base <= base && (other.base + other.size) > base);
}

bool TextureUnitState::Overlaps(const TextureUnitState& other) const
{
  if (state == State::INVALID || other.state == State::INVALID)
    return false;
  return even.Overlaps(other.even) || even.Overlaps(other.odd) || odd.Overlaps(other.even) ||
         odd.Overlaps(other.odd);
}

// Scans though active texture units checks for overlaps.
void FinalizeBinds(const BitSet32 used_textures)
{
  for (const u32 i : used_textures)
  {
    if (s_unit[i].even.Overlaps(s_unit[i].odd))
    {  // Self-overlap
      s_unit[i].state = TextureUnitState::State::VALID;
    }
    for (size_t j = 0; j < s_unit.size(); j++)
    {
      if (j != i && s_unit[i].Overlaps(s_unit[j]))
      {
        // There is an overlap, downgrade both from CACHED
        // (for there to be an overlap, both must have started as valid or cached)
        s_unit[i].state = TextureUnitState::State::VALID;
        s_unit[j].state = TextureUnitState::State::VALID;
      }
    }
  }
}

bool IsCached(const u32 unit)
{
  return s_unit[unit].state == TextureUnitState::State::CACHED;
}

bool IsValid(const u32 unit)
{
  return s_unit[unit].state != TextureUnitState::State::INVALID;
}

void Init()
{
  s_unit.fill({});
}

void DoState(PointerWrap& p)
{
  p.DoArray(s_unit);
}

}  // namespace TMEM
