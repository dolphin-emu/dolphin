// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

namespace Core
{
class CPUThreadGuard;
class System;
};  // namespace Core
namespace Memory
{
class MemoryManager;
};

namespace PowerPC
{
class PowerPCManager;
struct PowerPCState;

enum class RequestedAddressSpace
{
  Effective,  // whatever the current MMU state is
  Physical,   // as if the MMU was turned off
  Virtual,    // specifically want MMU turned on, fails if off
};

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

struct WriteResult
{
  // whether the address had to be translated (given address was treated as virtual) or not (given
  // address was treated as physical)
  bool translated;

  explicit WriteResult(bool translated_) : translated(translated_) {}
};

constexpr int BAT_INDEX_SHIFT = 17;
constexpr u32 BAT_PAGE_SIZE = 1 << BAT_INDEX_SHIFT;
constexpr u32 BAT_PAGE_COUNT = 1 << (32 - BAT_INDEX_SHIFT);
constexpr u32 BAT_MAPPED_BIT = 0x1;
constexpr u32 BAT_PHYSICAL_BIT = 0x2;
constexpr u32 BAT_WI_BIT = 0x4;
constexpr u32 BAT_RESULT_MASK = UINT32_C(~0x7);
using BatTable = std::array<u32, BAT_PAGE_COUNT>;  // 128 KB

constexpr size_t HW_PAGE_SIZE = 4096;
constexpr size_t HW_PAGE_MASK = HW_PAGE_SIZE - 1;
constexpr u32 HW_PAGE_INDEX_SHIFT = 12;
constexpr u32 HW_PAGE_INDEX_MASK = 0x3f;

// Return value of MMU::TryReadInstruction().
struct TryReadInstResult
{
  bool valid;
  bool from_bat;
  u32 hex;
  u32 physical_address;
};

// Return value of MMU::JitCache_TranslateAddress().
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

enum class XCheckTLBFlag
{
  NoException,
  Read,
  Write,
  Opcode,
  OpcodeNoException
};

class MMU
{
public:
  MMU(Core::System& system, Memory::MemoryManager& memory, PowerPC::PowerPCManager& power_pc);
  MMU(const MMU& other) = delete;
  MMU(MMU&& other) = delete;
  MMU& operator=(const MMU& other) = delete;
  MMU& operator=(MMU&& other) = delete;
  ~MMU();

  // Routines for debugger UI, cheats, etc. to access emulated memory from the
  // perspective of the CPU.  Not for use by core emulation routines.
  // Use "Host" prefix.

  // Reads a value from emulated memory using the currently active MMU settings.
  // If the read fails (eg. address does not correspond to a mapped address in the current address
  // space), a PanicAlert will be shown to the user and zero (or an empty string for the string
  // case) will be returned.
  static u8 HostRead_U8(const Core::CPUThreadGuard& guard, u32 address);
  static u16 HostRead_U16(const Core::CPUThreadGuard& guard, u32 address);
  static u32 HostRead_U32(const Core::CPUThreadGuard& guard, u32 address);
  static u64 HostRead_U64(const Core::CPUThreadGuard& guard, u32 address);
  static float HostRead_F32(const Core::CPUThreadGuard& guard, u32 address);
  static double HostRead_F64(const Core::CPUThreadGuard& guard, u32 address);
  static u32 HostRead_Instruction(const Core::CPUThreadGuard& guard, u32 address);
  static std::string HostGetString(const Core::CPUThreadGuard& guard, u32 address, size_t size = 0);
  static std::u16string HostGetU16String(const Core::CPUThreadGuard& guard, u32 address,
                                         size_t size = 0);

  // Try to read a value from emulated memory at the given address in the given memory space.
  // If the read succeeds, the returned value will be present and the ReadResult contains the read
  // value and information on whether the given address had to be translated or not. Unlike the
  // HostRead functions, this does not raise a user-visible alert on failure.
  static std::optional<ReadResult<u8>>
  HostTryReadU8(const Core::CPUThreadGuard& guard, u32 address,
                RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<u16>>
  HostTryReadU16(const Core::CPUThreadGuard& guard, u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<u32>>
  HostTryReadU32(const Core::CPUThreadGuard& guard, u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<u64>>
  HostTryReadU64(const Core::CPUThreadGuard& guard, u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<float>>
  HostTryReadF32(const Core::CPUThreadGuard& guard, u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<double>>
  HostTryReadF64(const Core::CPUThreadGuard& guard, u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<u32>>
  HostTryReadInstruction(const Core::CPUThreadGuard& guard, u32 address,
                         RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<std::string>>
  HostTryReadString(const Core::CPUThreadGuard& guard, u32 address, size_t size = 0,
                    RequestedAddressSpace space = RequestedAddressSpace::Effective);

  // Writes a value to emulated memory using the currently active MMU settings.
  // If the write fails (eg. address does not correspond to a mapped address in the current address
  // space), a PanicAlert will be shown to the user.
  static void HostWrite_U8(const Core::CPUThreadGuard& guard, u32 var, u32 address);
  static void HostWrite_U16(const Core::CPUThreadGuard& guard, u32 var, u32 address);
  static void HostWrite_U32(const Core::CPUThreadGuard& guard, u32 var, u32 address);
  static void HostWrite_U64(const Core::CPUThreadGuard& guard, u64 var, u32 address);
  static void HostWrite_F32(const Core::CPUThreadGuard& guard, float var, u32 address);
  static void HostWrite_F64(const Core::CPUThreadGuard& guard, double var, u32 address);

  // Try to a write a value to memory at the given address in the given memory space.
  // If the write succeeds, the returned TryWriteResult contains information on whether the given
  // address had to be translated or not. Unlike the HostWrite functions, this does not raise a
  // user-visible alert on failure.
  static std::optional<WriteResult>
  HostTryWriteU8(const Core::CPUThreadGuard& guard, u32 var, const u32 address,
                 RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<WriteResult>
  HostTryWriteU16(const Core::CPUThreadGuard& guard, u32 var, const u32 address,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<WriteResult>
  HostTryWriteU32(const Core::CPUThreadGuard& guard, u32 var, const u32 address,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<WriteResult>
  HostTryWriteU64(const Core::CPUThreadGuard& guard, u64 var, const u32 address,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<WriteResult>
  HostTryWriteF32(const Core::CPUThreadGuard& guard, float var, const u32 address,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<WriteResult>
  HostTryWriteF64(const Core::CPUThreadGuard& guard, double var, const u32 address,
                  RequestedAddressSpace space = RequestedAddressSpace::Effective);

  // Returns whether a read or write to the given address will resolve to a RAM access in the given
  // address space.
  static bool HostIsRAMAddress(const Core::CPUThreadGuard& guard, u32 address,
                               RequestedAddressSpace space = RequestedAddressSpace::Effective);

  // Same as HostIsRAMAddress, but uses IBAT instead of DBAT.
  static bool
  HostIsInstructionRAMAddress(const Core::CPUThreadGuard& guard, u32 address,
                              RequestedAddressSpace space = RequestedAddressSpace::Effective);

  // Routines for the CPU core to access memory.

  // Used by interpreter to read instructions, uses iCache
  u32 Read_Opcode(u32 address);
  TryReadInstResult TryReadInstruction(u32 address);

  u8 Read_U8(u32 address);
  u16 Read_U16(u32 address);
  u32 Read_U32(u32 address);
  u64 Read_U64(u32 address);

  void Write_U8(u32 var, u32 address);
  void Write_U16(u32 var, u32 address);
  void Write_U32(u32 var, u32 address);
  void Write_U64(u64 var, u32 address);

  void Write_U16_Swap(u32 var, u32 address);
  void Write_U32_Swap(u32 var, u32 address);
  void Write_U64_Swap(u64 var, u32 address);

  void DMA_LCToMemory(u32 mem_address, u32 cache_address, u32 num_blocks);
  void DMA_MemoryToLC(u32 cache_address, u32 mem_address, u32 num_blocks);

  void ClearDCacheLine(u32 address);  // Zeroes 32 bytes; address should be 32-byte-aligned
  void StoreDCacheLine(u32 address);
  void InvalidateDCacheLine(u32 address);
  void FlushDCacheLine(u32 address);
  void TouchDCacheLine(u32 address, bool store);

  // TLB functions
  void SDRUpdated();
  void InvalidateTLBEntry(u32 address);
  void DBATUpdated();
  void IBATUpdated();

  // Result changes based on the BAT registers and MSR.DR.  Returns whether
  // it's safe to optimize a read or write to this address to an unguarded
  // memory access.  Does not consider page tables.
  bool IsOptimizableRAMAddress(u32 address, u32 access_size) const;
  u32 IsOptimizableMMIOAccess(u32 address, u32 access_size) const;
  bool IsOptimizableGatherPipeWrite(u32 address) const;

  TranslateResult JitCache_TranslateAddress(u32 address);

  std::optional<u32> GetTranslatedAddress(u32 address);

  BatTable& GetIBATTable() { return m_ibat_table; }
  BatTable& GetDBATTable() { return m_dbat_table; }

private:
  enum class TranslateAddressResultEnum : u8
  {
    BAT_TRANSLATED,
    PAGE_TABLE_TRANSLATED,
    DIRECT_STORE_SEGMENT,
    PAGE_FAULT,
  };

  struct TranslateAddressResult
  {
    u32 address;
    TranslateAddressResultEnum result;
    bool wi;  // Set to true if the view of memory is either write-through or cache-inhibited

    TranslateAddressResult(TranslateAddressResultEnum result_, u32 address_, bool wi_ = false)
        : address(address_), result(result_), wi(wi_)
    {
    }
    bool Success() const { return result <= TranslateAddressResultEnum::PAGE_TABLE_TRANSLATED; }
  };

  union EffectiveAddress
  {
    BitField<0, 12, u32> offset;
    BitField<12, 16, u32> page_index;
    BitField<22, 6, u32> API;
    BitField<28, 4, u32> SR;

    u32 Hex = 0;

    EffectiveAddress() = default;
    explicit EffectiveAddress(u32 address) : Hex{address} {}
  };

  template <const XCheckTLBFlag flag>
  TranslateAddressResult TranslateAddress(u32 address);

  template <const XCheckTLBFlag flag>
  TranslateAddressResult TranslatePageAddress(const EffectiveAddress address, bool* wi);

  void GenerateDSIException(u32 effective_address, bool write);
  void GenerateISIException(u32 effective_address);

  void Memcheck(u32 address, u64 var, bool write, size_t size);

  void UpdateBATs(BatTable& bat_table, u32 base_spr);
  void UpdateFakeMMUBat(BatTable& bat_table, u32 start_addr);

  template <XCheckTLBFlag flag, typename T, bool never_translate = false>
  T ReadFromHardware(u32 effective_address);
  template <XCheckTLBFlag flag, bool never_translate = false>
  void WriteToHardware(u32 effective_address, u32 data, u32 size);
  template <XCheckTLBFlag flag>
  bool IsRAMAddress(u32 address, bool translate);

  template <typename T>
  static std::optional<ReadResult<T>> HostTryReadUX(const Core::CPUThreadGuard& guard,
                                                    const u32 address, RequestedAddressSpace space);
  static std::optional<WriteResult> HostTryWriteUX(const Core::CPUThreadGuard& guard, const u32 var,
                                                   const u32 address, const u32 size,
                                                   RequestedAddressSpace space);

  Core::System& m_system;
  Memory::MemoryManager& m_memory;
  PowerPC::PowerPCManager& m_power_pc;
  PowerPC::PowerPCState& m_ppc_state;

  BatTable m_ibat_table;
  BatTable m_dbat_table;
};

void ClearDCacheLineFromJit(MMU& mmu, u32 address);
u32 ReadU8FromJit(MMU& mmu, u32 address);   // Returns zero-extended 32bit value
u32 ReadU16FromJit(MMU& mmu, u32 address);  // Returns zero-extended 32bit value
u32 ReadU32FromJit(MMU& mmu, u32 address);
u64 ReadU64FromJit(MMU& mmu, u32 address);
void WriteU8FromJit(MMU& mmu, u32 var, u32 address);
void WriteU16FromJit(MMU& mmu, u32 var, u32 address);
void WriteU32FromJit(MMU& mmu, u32 var, u32 address);
void WriteU64FromJit(MMU& mmu, u64 var, u32 address);
void WriteU16SwapFromJit(MMU& mmu, u32 var, u32 address);
void WriteU32SwapFromJit(MMU& mmu, u32 var, u32 address);
void WriteU64SwapFromJit(MMU& mmu, u64 var, u32 address);
}  // namespace PowerPC
