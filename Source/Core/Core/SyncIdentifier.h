// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <tuple>

#include "Common/CommonTypes.h"

namespace NetPlay
{
struct SyncIdentifier
{
  u64 dol_elf_size = 0;
  std::string game_id;
  u16 revision = 0;
  u8 disc_number = 0;
  bool is_datel = false;

  // This hash is intended to be (but is not guaranteed to be):
  // 1. Identical for discs with no differences that affect netplay/TAS sync
  // 2. Different for discs with differences that affect netplay/TAS sync
  // 3. Much faster than hashing the entire disc
  // The way the hash is calculated may change with updates to Dolphin.
  std::array<u8, 20> sync_hash{};

  bool operator==(const SyncIdentifier& s) const
  {
    return std::tie(dol_elf_size, game_id, revision, disc_number, is_datel, sync_hash) ==
           std::tie(s.dol_elf_size, s.game_id, s.revision, s.disc_number, s.is_datel, s.sync_hash);
  }
  bool operator!=(const SyncIdentifier& s) const { return !operator==(s); }
};

// The order of entries in this enum matters, as the lowest value is
// treated as the "best" available option.
enum class SyncIdentifierComparison : u8
{
  SameGame,
  DifferentHash,
  DifferentDiscNumber,
  DifferentRevision,
  DifferentRegion,
  DifferentGame,
  Unknown,
};

}  // namespace NetPlay
