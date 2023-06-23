// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "Common/CommonTypes.h"

namespace PowerPC
{
enum CRBits
{
  CR_SO = 1,
  CR_EQ = 2,
  CR_GT = 4,
  CR_LT = 8,

  CR_SO_BIT = 0,
  CR_EQ_BIT = 1,
  CR_GT_BIT = 2,
  CR_LT_BIT = 3,

  CR_EMU_SO_BIT = 59,
  CR_EMU_LT_BIT = 62,
};

// Optimized CR implementation. Instead of storing CR in its PowerPC format
// (4 bit value, SO/EQ/LT/GT), we store instead a 64 bit value for each of
// the 8 CR register parts. This 64 bit value follows this format:
//   - SO iff. bit 59 is set
//   - EQ iff. lower 32 bits == 0
//   - GT iff. (s64)cr_val > 0
//   - LT iff. bit 62 is set
//
// This has the interesting property that sign-extending the result of an
// operation from 32 to 64 bits results in a 64 bit value that works as a
// CR value. Checking each part of CR is also fast, as it is equivalent to
// testing one bit or the low 32 bit part of a register. And CR can still
// be manipulated bit by bit fairly easily.
struct ConditionRegister
{
  // convert flags into 64-bit CR values with a lookup table
  static const std::array<u64, 16> s_crTable;

  u64 fields[8];

  // Convert between PPC and internal representation of CR.
  static constexpr u64 PPCToInternal(u8 value)
  {
    u64 cr_val = 0x100000000;
    cr_val |= (u64) !!(value & CR_SO) << CR_EMU_SO_BIT;
    cr_val |= (u64) !(value & CR_EQ);
    cr_val |= (u64) !(value & CR_GT) << 63;
    cr_val |= (u64) !!(value & CR_LT) << CR_EMU_LT_BIT;

    return cr_val;
  }

  // Warning: these CR operations are fairly slow since they need to convert from
  // PowerPC format (4 bit) to our internal 64 bit format.
  void SetField(u32 cr_field, u32 value) { fields[cr_field] = s_crTable[value]; }

  u32 GetField(u32 cr_field) const
  {
    const u64 cr_val = fields[cr_field];
    u32 ppc_cr = 0;

    // LT/SO
    static_assert(CR_EMU_LT_BIT - CR_EMU_SO_BIT == CR_LT_BIT - CR_SO_BIT);
    ppc_cr |= (cr_val >> CR_EMU_SO_BIT) & (PowerPC::CR_LT | PowerPC::CR_SO);
    // EQ
    ppc_cr |= ((cr_val & 0xFFFFFFFF) == 0) << PowerPC::CR_EQ_BIT;
    // GT
    ppc_cr |= (static_cast<s64>(cr_val) > 0) << PowerPC::CR_GT_BIT;

    return ppc_cr;
  }

  u32 GetBit(u32 bit) const { return (GetField(bit >> 2) >> (3 - (bit & 3))) & 1; }

  void SetBit(u32 bit, u32 value)
  {
    if (value & 1)
      SetField(bit >> 2, GetField(bit >> 2) | (0x8 >> (bit & 3)));
    else
      SetField(bit >> 2, GetField(bit >> 2) & ~(0x8 >> (bit & 3)));
  }

  // Set and Get are fairly slow. Should be avoided if possible.
  void Set(u32 new_cr);
  u32 Get() const;
};

}  // namespace PowerPC
