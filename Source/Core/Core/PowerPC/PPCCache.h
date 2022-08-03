// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace PowerPC
{
constexpr u32 ICACHE_SETS = 128;
constexpr u32 ICACHE_WAYS = 8;
// size of an instruction cache block in words
constexpr u32 ICACHE_BLOCK_SIZE = 8;

constexpr u32 ICACHE_EXRAM_BIT = 0x10000000;
constexpr u32 ICACHE_VMEM_BIT = 0x20000000;

struct InstructionCache
{
  std::array<std::array<std::array<u32, ICACHE_BLOCK_SIZE>, ICACHE_WAYS>, ICACHE_SETS> data{};
  std::array<std::array<u32, ICACHE_WAYS>, ICACHE_SETS> tags{};
  std::array<u32, ICACHE_SETS> plru{};
  std::array<u32, ICACHE_SETS> valid{};

  // Note: This is only for performance purposes; this same data could be computed at runtime
  // from the tags and valid fields (and that's how it's done on the actual cache)
  std::array<u8, 1 << 20> lookup_table{};
  std::array<u8, 1 << 21> lookup_table_ex{};
  std::array<u8, 1 << 20> lookup_table_vmem{};

  bool m_disable_icache = false;
  std::optional<size_t> m_config_callback_id = std::nullopt;

  InstructionCache() = default;
  ~InstructionCache();
  u32 ReadInstruction(u32 addr);
  void Invalidate(u32 addr);
  void Init();
  void Reset();
  void DoState(PointerWrap& p);
  void RefreshConfig();
};
}  // namespace PowerPC
