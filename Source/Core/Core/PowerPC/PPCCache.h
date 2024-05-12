// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"

class JitInterface;
namespace Memory
{
class MemoryManager;
}
class PointerWrap;
namespace PowerPC
{
struct PowerPCState;
}

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

  void Store(Memory::MemoryManager& memory, u32 addr);
  void Invalidate(Memory::MemoryManager& memory, u32 addr);
  void Flush(Memory::MemoryManager& memory, u32 addr);
  void Touch(Memory::MemoryManager& memory, u32 addr, bool store);

  void FlushAll(Memory::MemoryManager& memory);

  std::pair<u32, u32> GetCache(Memory::MemoryManager& memory, u32 addr, bool locked);

  void Read(Memory::MemoryManager& memory, u32 addr, void* buffer, u32 len, bool locked);
  void Write(Memory::MemoryManager& memory, u32 addr, const void* buffer, u32 len, bool locked);

  void Init(Memory::MemoryManager& memory);
  void Reset();

  void DoState(Memory::MemoryManager& memory, PointerWrap& p);
};

struct InstructionCache : public Cache
{
  std::optional<Config::ConfigChangedCallbackID> m_config_callback_id = std::nullopt;

  bool m_disable_icache = false;

  InstructionCache() = default;
  ~InstructionCache();
  u32 ReadInstruction(Memory::MemoryManager& memory, PowerPC::PowerPCState& ppc_state, u32 addr);
  void Invalidate(Memory::MemoryManager& memory, JitInterface& jit_interface, u32 addr);
  void Init(Memory::MemoryManager& memory);
  void Reset(JitInterface& jit_interface);
  void RefreshConfig();
};
}  // namespace PowerPC
