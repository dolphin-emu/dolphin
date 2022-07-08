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
constexpr u32 ICACHE_BLOCK_SIZE_WORDS = 8;  // 8 32-bit words, or 32 bytes

struct InstructionCache
{
  std::array<std::array<std::array<u32, ICACHE_BLOCK_SIZE_WORDS>, ICACHE_WAYS>, ICACHE_SETS> data{};
  std::array<std::array<u32, ICACHE_WAYS>, ICACHE_SETS> tags{};
  std::array<u32, ICACHE_SETS> plru{};
  std::array<u32, ICACHE_SETS> valid{};

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
