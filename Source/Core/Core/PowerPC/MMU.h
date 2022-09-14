// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
// Use "Host" prefix.

enum class RequestedAddressSpace
{
  Effective,  // whatever the current MMU state is
  Physical,   // as if the MMU was turned off
  Virtual,    // specifically want MMU turned on, fails if off
};

// Reads a value from emulated memory using the currently active MMU settings.
// If the read fails (eg. address does not correspond to a mapped address in the current address
// space), a PanicAlert will be shown to the user and zero (or an empty string for the string case)
// will be returned.
u8 HostRead_U8(u32 address);
u16 HostRead_U16(u32 address);
u32 HostRead_U32(u32 address);
u64 HostRead_U64(u32 address);
float HostRead_F32(u32 address);
double HostRead_F64(u32 address);
u32 HostRead_Instruction(u32 address);
std::string HostGetString(u32 address, size_t size = 0);

template <typename T>
struct ReadResult
{
  // whether the address had to be translated (given address was treated as virtual) or not (given
  // address was treated as physical)
  bool translated;

  // the actual value that was read
  T value;

  ReadResult(bool translated_, T&& value_) : translated(translated_), value(std::forward<T>(value_))
  {
  }
  ReadResult(bool translated_, const T& value_) : translated(translated_), value(value_) {}
};

// Try to read a value from emulated memory at the given address in the given memory space.
// If the read succeeds, the returned value will be present and the ReadResult contains the read
// value and information on whether the given address had to be translated or not. Unlike the
// HostRead functions, this does not raise a user-visible alert on failure.
std::optional<ReadResult<u8>>
HostTryReadU8(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<u16>>
HostTryReadU16(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<u32>>
HostTryReadU32(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<u64>>
HostTryReadU64(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<float>>
HostTryReadF32(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<double>>
HostTryReadF64(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<u32>>
HostTryReadInstruction(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<ReadResult<std::string>>
HostTryReadString(u32 address, size_t size = 0,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);

// Writes a value to emulated memory using the currently active MMU settings.
// If the write fails (eg. address does not correspond to a mapped address in the current address
// space), a PanicAlert will be shown to the user.
void HostWrite_U8(u32 var, u32 address);
void HostWrite_U16(u32 var, u32 address);
void HostWrite_U32(u32 var, u32 address);
void HostWrite_U64(u64 var, u32 address);
void HostWrite_F32(float var, u32 address);
void HostWrite_F64(double var, u32 address);

struct WriteResult
{
  // whether the address had to be translated (given address was treated as virtual) or not (given
  // address was treated as physical)
  bool translated;

  explicit WriteResult(bool translated_) : translated(translated_) {}
};

// Try to a write a value to memory at the given address in the given memory space.
// If the write succeeds, the returned TryWriteResult contains information on whether the given
// address had to be translated or not. Unlike the HostWrite functions, this does not raise a
// user-visible alert on failure.
std::optional<WriteResult>
HostTryWriteU8(u32 var, const u32 address,
               RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<WriteResult>
HostTryWriteU16(u32 var, const u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<WriteResult>
HostTryWriteU32(u32 var, const u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<WriteResult>
HostTryWriteU64(u64 var, const u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<WriteResult>
HostTryWriteF32(float var, const u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);
std::optional<WriteResult>
HostTryWriteF64(double var, const u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);

// Returns whether a read or write to the given address will resolve to a RAM access in the given
// address space.
bool HostIsRAMAddress(u32 address, RequestedAddressSpace space = RequestedAddressSpace::Effective);

// Same as HostIsRAMAddress, but uses IBAT instead of DBAT.
bool HostIsInstructionRAMAddress(u32 address,
                                 RequestedAddressSpace space = RequestedAddressSpace::Effective);

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

void Write_U8(u32 var, u32 address);
void Write_U16(u32 var, u32 address);
void Write_U32(u32 var, u32 address);
void Write_U64(u64 var, u32 address);

void Write_U16_Swap(u32 var, u32 address);
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
  bool valid = false;
  bool translated = false;
  bool from_bat = false;
  u32 address = 0;

  TranslateResult() = default;
  explicit TranslateResult(u32 address_) : valid(true), address(address_) {}
  TranslateResult(bool from_bat_, u32 address_)
      : valid(true), translated(true), from_bat(from_bat_), address(address_)
  {
  }
};
TranslateResult JitCache_TranslateAddress(u32 address);

constexpr int BAT_INDEX_SHIFT = 17;
constexpr u32 BAT_PAGE_SIZE = 1 << BAT_INDEX_SHIFT;
constexpr u32 BAT_PAGE_COUNT = 1 << (32 - BAT_INDEX_SHIFT);
constexpr u32 BAT_MAPPED_BIT = 0x1;
constexpr u32 BAT_PHYSICAL_BIT = 0x2;
constexpr u32 BAT_WI_BIT = 0x4;
constexpr u32 BAT_RESULT_MASK = UINT32_C(~0x7);
using BatTable = std::array<u32, BAT_PAGE_COUNT>;  // 128 KB
extern BatTable ibat_table;
extern BatTable dbat_table;
inline bool TranslateBatAddess(const BatTable& bat_table, u32* address, bool* wi)
{
  u32 bat_result = bat_table[*address >> BAT_INDEX_SHIFT];
  if ((bat_result & BAT_MAPPED_BIT) == 0)
    return false;
  *address = (bat_result & BAT_RESULT_MASK) | (*address & (BAT_PAGE_SIZE - 1));
  *wi = (bat_result & BAT_WI_BIT) != 0;
  return true;
}

constexpr size_t HW_PAGE_SIZE = 4096;
constexpr size_t HW_PAGE_MASK = HW_PAGE_SIZE - 1;
constexpr u32 HW_PAGE_INDEX_SHIFT = 12;
constexpr u32 HW_PAGE_INDEX_MASK = 0x3f;

std::optional<u32> GetTranslatedAddress(u32 address);
}  // namespace PowerPC
