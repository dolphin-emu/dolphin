// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace PowerPC
{
// Routines for debugger UI, cheats, etc. to access emulated memory from the
// perspective of the CPU.  Not for use by core emulation routines.
// Use "Host_" prefix.
u8 HostRead_U8(u32 address);
u16 HostRead_U16(u32 address);
u32 HostRead_U32(u32 address);
u64 HostRead_U64(u32 address);
float HostRead_F32(u32 address);
double HostRead_F64(u32 address);
u32 HostRead_Instruction(u32 address);

void HostWrite_U8(u8 var, u32 address);
void HostWrite_U16(u16 var, u32 address);
void HostWrite_U32(u32 var, u32 address);
void HostWrite_U64(u64 var, u32 address);
void HostWrite_F32(float var, u32 address);
void HostWrite_F64(double var, u32 address);

std::string HostGetString(u32 address, size_t size = 0);

// Returns whether a read or write to the given address will resolve to a RAM
// access given the current CPU state.
bool HostIsRAMAddress(u32 address);

// Same as HostIsRAMAddress, but uses IBAT instead of DBAT.
bool HostIsInstructionRAMAddress(u32 address);

// Routines for the CPU core to access memory.

// Used by interpreter to read instructions, uses iCache
u32 Read_Opcode(u32 address);
struct TryReadInstResult
{
  bool valid;
  bool from_bat;
  u32 hex;
  u32 physical_address;
};
TryReadInstResult TryReadInstruction(u32 address);

u8 Read_U8(u32 address);
u16 Read_U16(u32 address);
u32 Read_U32(u32 address);
u64 Read_U64(u32 address);

// Useful helper functions, used by ARM JIT
float Read_F32(u32 address);
double Read_F64(u32 address);

// used by JIT. Return zero-extended 32bit values
u32 Read_U8_ZX(u32 address);
u32 Read_U16_ZX(u32 address);

void Write_U8(u8 var, u32 address);
void Write_U16(u16 var, u32 address);
void Write_U32(u32 var, u32 address);
void Write_U64(u64 var, u32 address);

void Write_U16_Swap(u16 var, u32 address);
void Write_U32_Swap(u32 var, u32 address);
void Write_U64_Swap(u64 var, u32 address);

// Useful helper functions, used by ARM JIT
void Write_F64(double var, u32 address);

void DMA_LCToMemory(u32 mem_address, u32 cache_address, u32 num_blocks);
void DMA_MemoryToLC(u32 cache_address, u32 mem_address, u32 num_blocks);
void ClearCacheLine(u32 address);  // Zeroes 32 bytes; address should be 32-byte-aligned

// TLB functions
void SDRUpdated();
void InvalidateTLBEntry(u32 address);
void DBATUpdated();
void IBATUpdated();

// Result changes based on the BAT registers and MSR.DR.  Returns whether
// it's safe to optimize a read or write to this address to an unguarded
// memory access.  Does not consider page tables.
bool IsOptimizableRAMAddress(u32 address);
u32 IsOptimizableMMIOAccess(u32 address, u32 access_size);
bool IsOptimizableGatherPipeWrite(u32 address);

struct TranslateResult
{
  bool valid;
  bool from_bat;
  u32 address;
};
TranslateResult JitCache_TranslateAddress(u32 address);

constexpr int BAT_INDEX_SHIFT = 17;
constexpr u32 BAT_PAGE_SIZE = 1 << BAT_INDEX_SHIFT;
constexpr u32 BAT_MAPPED_BIT = 0x1;
constexpr u32 BAT_PHYSICAL_BIT = 0x2;
constexpr u32 BAT_RESULT_MASK = UINT32_C(~0x3);
using BatTable = std::array<u32, 1 << (32 - BAT_INDEX_SHIFT)>;  // 128 KB
extern BatTable ibat_table;
extern BatTable dbat_table;
inline bool TranslateBatAddess(const BatTable& bat_table, u32* address)
{
  u32 bat_result = bat_table[*address >> BAT_INDEX_SHIFT];
  if ((bat_result & BAT_MAPPED_BIT) == 0)
    return false;
  *address = (bat_result & BAT_RESULT_MASK) | (*address & (BAT_PAGE_SIZE - 1));
  return true;
}

std::optional<u32> GetTranslatedAddress(u32 address);
}  // namespace PowerPC
