// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace PowerPC
{
constexpr u32 CACHE_SETS = 128;
constexpr u32 CACHE_WAYS = 8;
// size of an instruction cache block in words
constexpr u32 CACHE_BLOCK_SIZE = 8;

constexpr u32 CACHE_EXRAM_BIT = 0x10000000;
constexpr u32 CACHE_VMEM_BIT = 0x20000000;

struct Cache
{
  std::array<std::array<std::array<u32, CACHE_BLOCK_SIZE>, CACHE_WAYS>, CACHE_SETS> data{};

  // Stores the 32-byte aligned address of the start of each cache block. This consists of the cache
  // set and tag. Real hardware only needs to store the tag, but also including the set simplifies
  // debugging and getting the actual address in the cache, without changing behavior (as the set
  // portion of the address is by definition the same for all addresses in a set).
  std::array<std::array<u32, CACHE_WAYS>, CACHE_SETS> addrs{};

  std::array<u8, CACHE_SETS> plru{};
  std::array<u8, CACHE_SETS> valid{};
  std::array<u8, CACHE_SETS> modified{};

  // Note: This is only for performance purposes; this same data could be computed at runtime
  // from the tags and valid fields (and that's how it's done on the actual cache)
  std::vector<u8> lookup_table{};
  std::vector<u8> lookup_table_ex{};
  std::vector<u8> lookup_table_vmem{};

  void Store(u32 addr);
  void Invalidate(u32 addr);
  void Flush(u32 addr);
  void Touch(u32 addr, bool store);

  void FlushAll();

  std::pair<u32, u32> GetCache(u32 addr, bool locked);

  void Read(u32 addr, void* buffer, u32 len, bool locked);
  void Write(u32 addr, const void* buffer, u32 len, bool locked);

  void Init();
  void Reset();

  void DoState(PointerWrap& p);
};

struct InstructionCache : public Cache
{
  std::optional<size_t> m_config_callback_id = std::nullopt;

  bool m_disable_icache = false;

  InstructionCache() = default;
  ~InstructionCache();
  u32 ReadInstruction(u32 addr);
  void Invalidate(u32 addr);
  void Init();
  void Reset();
  void RefreshConfig();
};
}  // namespace PowerPC
