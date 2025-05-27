// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace Core
{
class CPUThreadGuard;
class System;
}  // namespace Core
namespace Memory
{
class MemoryManager;
}

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

template <typename T>
using make_unsigned_same_size = typename std::enable_if_t<
    sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
    std::conditional_t<
        sizeof(T) == 1, u8,
        std::conditional_t<sizeof(T) == 2, u16, std::conditional_t<sizeof(T) == 4, u32, u64>>>>;

template <typename T>
using make_atleast_u32 =
    std::enable_if_t<std::is_unsigned_v<T>, std::conditional_t<std::is_same_v<T, u64>, u64, u32>>;

template <typename T>
concept Size64 = (sizeof(T) == 8);

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
  template <typename T>
  static T HostRead(const Core::CPUThreadGuard& guard, const u32 address)
  {
    auto& mmu = guard.GetSystem().GetMMU();
    return std::bit_cast<T>(
        mmu.ReadFromHardware<XCheckTLBFlag::NoException, make_unsigned_same_size<T>>(address));
  }

  static u32 HostRead_Instruction(const Core::CPUThreadGuard& guard, u32 address);
  static std::string HostGetString(const Core::CPUThreadGuard& guard, u32 address, size_t size = 0);
  static std::u16string HostGetU16String(const Core::CPUThreadGuard& guard, u32 address,
                                         size_t size = 0);

  // Try to read a value from emulated memory at the given address in the given memory space.
  // If the read succeeds, the returned value will be present and the ReadResult contains the read
  // value and information on whether the given address had to be translated or not. Unlike the
  // HostRead functions, this does not raise a user-visible alert on failure.
  template <typename T>
  static std::optional<ReadResult<T>>
  HostTryRead(const Core::CPUThreadGuard& guard, const u32 address,
              RequestedAddressSpace space = RequestedAddressSpace::Effective)
  {
    if (!HostIsRAMAddress(guard, address, space))
      return std::nullopt;

    using U = make_unsigned_same_size<T>;
    auto& mmu = guard.GetSystem().GetMMU();
    switch (space)
    {
    case RequestedAddressSpace::Effective:
    {
      U value = mmu.ReadFromHardware<XCheckTLBFlag::NoException, U>(address);
      return ReadResult<T>(!!mmu.m_ppc_state.msr.DR, std::bit_cast<T>(std::move(value)));
    }
    case RequestedAddressSpace::Physical:
    {
      U value = mmu.ReadFromHardware<XCheckTLBFlag::NoException, U, true>(address);
      return ReadResult<T>(false, std::bit_cast<T>(std::move(value)));
    }
    case RequestedAddressSpace::Virtual:
    {
      if (!mmu.m_ppc_state.msr.DR)
        return std::nullopt;
      U value = mmu.ReadFromHardware<XCheckTLBFlag::NoException, U>(address);
      return ReadResult<T>(true, std::bit_cast<T>(std::move(value)));
    }
    }

    ASSERT(false);
    return std::nullopt;
  }

  static std::optional<ReadResult<u32>>
  HostTryReadInstruction(const Core::CPUThreadGuard& guard, u32 address,
                         RequestedAddressSpace space = RequestedAddressSpace::Effective);
  static std::optional<ReadResult<std::string>>
  HostTryReadString(const Core::CPUThreadGuard& guard, u32 address, size_t size = 0,
                    RequestedAddressSpace space = RequestedAddressSpace::Effective);

  // Writes a value to emulated memory using the currently active MMU settings.
  // If the write fails (eg. address does not correspond to a mapped address in the current address
  // space), a PanicAlert will be shown to the user.
  template <typename T>
  requires(!Size64<T>)
  static void HostWrite(const Core::CPUThreadGuard& guard, const auto var, const u32 address)
  {
    auto& mmu = guard.GetSystem().GetMMU();
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);
    mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, static_cast<u32>(v), sizeof(T));
  }

  template <typename T>
  requires Size64<T>
  static void HostWrite(const Core::CPUThreadGuard& guard, const auto var, const u32 address)
  {
    auto& mmu = guard.GetSystem().GetMMU();
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);
    mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, static_cast<u32>(v >> 32), 4);
    mmu.WriteToHardware<XCheckTLBFlag::NoException>(address + sizeof(u32), static_cast<u32>(v), 4);
  }

  // Try to a write a value to memory at the given address in the given memory space.
  // If the write succeeds, the returned TryWriteResult contains information on whether the given
  // address had to be translated or not. Unlike the HostWrite functions, this does not raise a
  // user-visible alert on failure.
  template <typename T>
  requires(!Size64<T>)
  static std::optional<WriteResult>
  HostTryWrite(const Core::CPUThreadGuard& guard, const auto var, const u32 address,
               RequestedAddressSpace space = RequestedAddressSpace::Effective)
  {
    constexpr auto size = sizeof(T);
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);

    if (!HostIsRAMAddress(guard, address, space))
      return std::nullopt;

    auto& mmu = guard.GetSystem().GetMMU();
    switch (space)
    {
    case RequestedAddressSpace::Effective:
      mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, static_cast<u32>(v), size);
      return WriteResult(!!mmu.m_ppc_state.msr.DR);
    case RequestedAddressSpace::Physical:
      mmu.WriteToHardware<XCheckTLBFlag::NoException, true>(address, static_cast<u32>(v), size);
      return WriteResult(false);
    case RequestedAddressSpace::Virtual:
      if (!mmu.m_ppc_state.msr.DR)
        return std::nullopt;
      mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, static_cast<u32>(v), size);
      return WriteResult(true);
    }

    ASSERT(false);
    return std::nullopt;
  }

  template <typename T>
  requires Size64<T>
  static std::optional<WriteResult>
  HostTryWrite(const Core::CPUThreadGuard& guard, const auto var, const u32 address,
               RequestedAddressSpace space = RequestedAddressSpace::Effective)
  {
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);
    const auto result = HostTryWrite<u32>(guard, static_cast<u32>(v >> 32), address, space);
    if (!result)
      return result;

    return HostTryWrite<u32>(guard, static_cast<u32>(v), address + 4, space);
  }

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

  template <typename T>
  T Read(const u32 address)
  {
    using U = make_unsigned_same_size<T>;
    U var = ReadFromHardware<XCheckTLBFlag::Read, U>(address);
    Memcheck(address, var, false, sizeof(T));
    return std::bit_cast<T>(var);
  }

  template <typename T>
  requires(!Size64<T>)
  void Write(const auto var, const u32 address)
  {
    constexpr auto size = sizeof(T);
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);
    Memcheck(address, v, true, size);
    WriteToHardware<XCheckTLBFlag::Write>(address, static_cast<u32>(v), size);
  }

  template <typename T>
  requires Size64<T>
  void Write(const auto var, const u32 address)
  {
    const auto v = std::bit_cast<make_unsigned_same_size<decltype(var)>>(var);
    Memcheck(address, v, true, 8);
    WriteToHardware<XCheckTLBFlag::Write>(address, static_cast<u32>(v >> 32), 4);
    WriteToHardware<XCheckTLBFlag::Write>(address + sizeof(u32), static_cast<u32>(v), 4);
  }

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

  template <XCheckTLBFlag flag, std::unsigned_integral T, bool never_translate = false>
  T ReadFromHardware(u32 em_address);
  template <XCheckTLBFlag flag, bool never_translate = false>
  void WriteToHardware(u32 em_address, const u32 data, const u32 size);
  template <XCheckTLBFlag flag>
  bool IsRAMAddress(u32 address, bool translate);

  Core::System& m_system;
  Memory::MemoryManager& m_memory;
  PowerPC::PowerPCManager& m_power_pc;
  PowerPC::PowerPCState& m_ppc_state;

  BatTable m_ibat_table;
  BatTable m_dbat_table;
};

void ClearDCacheLineFromJit(MMU& mmu, u32 address);
template <typename T>
// Returns zero-extended value
make_atleast_u32<T> ReadFromJit(MMU& mmu, u32 address)
{
  return mmu.Read<T>(address);
}
template <typename T>
// Can't use auto for var, as it'd prevent making a function pointer
void WriteFromJit(MMU& mmu, make_atleast_u32<T> var, u32 address)
{
  mmu.Write<T>(var, address);
}
void WriteU16SwapFromJit(MMU& mmu, u32 var, u32 address);
void WriteU32SwapFromJit(MMU& mmu, u32 var, u32 address);
void WriteU64SwapFromJit(MMU& mmu, u64 var, u32 address);
}  // namespace PowerPC
