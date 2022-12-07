// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/MMU.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "VideoCommon/VideoBackendBase.h"

namespace PowerPC
{
// EFB RE
/*
GXPeekZ
80322de8: rlwinm    r0, r3, 2, 14, 29 (0003fffc)   a =  x << 2 & 0x3fffc
80322dec: oris      r0, r0, 0xC800                 a |= 0xc8000000
80322df0: rlwinm    r3, r0, 0, 20, 9 (ffc00fff)    x = a & 0xffc00fff
80322df4: rlwinm    r0, r4, 12, 4, 19 (0ffff000)   a = (y << 12) & 0x0ffff000;
80322df8: or        r0, r3, r0                     a |= x;
80322dfc: rlwinm    r0, r0, 0, 10, 7 (ff3fffff)    a &= 0xff3fffff
80322e00: oris      r3, r0, 0x0040                 x = a | 0x00400000
80322e04: lwz       r0, 0 (r3)                     r0 = *r3
80322e08: stw       r0, 0 (r5)                     z =
80322e0c: blr
*/

// =================================
// From Memmap.cpp
// ----------------

// Overloaded byteswap functions, for use within the templated functions below.
inline u8 bswap(u8 val)
{
  return val;
}
inline s8 bswap(s8 val)
{
  return val;
}
inline u16 bswap(u16 val)
{
  return Common::swap16(val);
}
inline s16 bswap(s16 val)
{
  return Common::swap16(val);
}
inline u32 bswap(u32 val)
{
  return Common::swap32(val);
}
inline u64 bswap(u64 val)
{
  return Common::swap64(val);
}
// =================

enum class XCheckTLBFlag
{
  NoException,
  Read,
  Write,
  Opcode,
  OpcodeNoException
};

static bool IsOpcodeFlag(XCheckTLBFlag flag)
{
  return flag == XCheckTLBFlag::Opcode || flag == XCheckTLBFlag::OpcodeNoException;
}

static bool IsNoExceptionFlag(XCheckTLBFlag flag)
{
  return flag == XCheckTLBFlag::NoException || flag == XCheckTLBFlag::OpcodeNoException;
}

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
template <const XCheckTLBFlag flag>
static TranslateAddressResult TranslateAddress(u32 address);

// Nasty but necessary. Super Mario Galaxy pointer relies on this stuff.
static u32 EFB_Read(const u32 addr)
{
  u32 var = 0;
  // Convert address to coordinates. It's possible that this should be done
  // differently depending on color depth, especially regarding PeekColor.
  const u32 x = (addr & 0xfff) >> 2;
  const u32 y = (addr >> 12) & 0x3ff;

  if (addr & 0x00800000)
  {
    ERROR_LOG_FMT(MEMMAP, "Unimplemented Z+Color EFB read @ {:#010x}", addr);
  }
  else if (addr & 0x00400000)
  {
    var = g_video_backend->Video_AccessEFB(EFBAccessType::PeekZ, x, y, 0);
    DEBUG_LOG_FMT(MEMMAP, "EFB Z Read @ {}, {}\t= {:#010x}", x, y, var);
  }
  else
  {
    var = g_video_backend->Video_AccessEFB(EFBAccessType::PeekColor, x, y, 0);
    DEBUG_LOG_FMT(MEMMAP, "EFB Color Read @ {}, {}\t= {:#010x}", x, y, var);
  }

  return var;
}

static void EFB_Write(u32 data, u32 addr)
{
  const u32 x = (addr & 0xfff) >> 2;
  const u32 y = (addr >> 12) & 0x3ff;

  if (addr & 0x00800000)
  {
    // It's possible to do a z-tested write to EFB by writing a 64bit value to this address range.
    // Not much is known, but let's at least get some loging.
    ERROR_LOG_FMT(MEMMAP, "Unimplemented Z+Color EFB write. {:08x} @ {:#010x}", data, addr);
  }
  else if (addr & 0x00400000)
  {
    g_video_backend->Video_AccessEFB(EFBAccessType::PokeZ, x, y, data);
    DEBUG_LOG_FMT(MEMMAP, "EFB Z Write {:08x} @ {}, {}", data, x, y);
  }
  else
  {
    g_video_backend->Video_AccessEFB(EFBAccessType::PokeColor, x, y, data);
    DEBUG_LOG_FMT(MEMMAP, "EFB Color Write {:08x} @ {}, {}", data, x, y);
  }
}

BatTable ibat_table;
BatTable dbat_table;

static void GenerateDSIException(u32 effective_address, bool write);

template <XCheckTLBFlag flag, typename T, bool never_translate = false>
static T ReadFromHardware(Memory::MemoryManager& memory, u32 em_address)
{
  const u32 em_address_start_page = em_address & ~HW_PAGE_MASK;
  const u32 em_address_end_page = (em_address + sizeof(T) - 1) & ~HW_PAGE_MASK;
  if (em_address_start_page != em_address_end_page)
  {
    // This could be unaligned down to the byte level... hopefully this is rare, so doing it this
    // way isn't too terrible.
    // TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
    // Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
    u64 var = 0;
    for (u32 i = 0; i < sizeof(T); ++i)
      var = (var << 8) | ReadFromHardware<flag, u8, never_translate>(memory, em_address + i);
    return static_cast<T>(var);
  }

  if (!never_translate && MSR.DR)
  {
    auto translated_addr = TranslateAddress<flag>(em_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Read)
        GenerateDSIException(em_address, false);
      return 0;
    }
    em_address = translated_addr.address;
  }

  if (flag == XCheckTLBFlag::Read && (em_address & 0xF8000000) == 0x08000000)
  {
    if (em_address < 0x0c000000)
      return EFB_Read(em_address);
    else
      return static_cast<T>(memory.GetMMIOMapping()->Read<std::make_unsigned_t<T>>(em_address));
  }

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if (memory.GetL1Cache() && (em_address >> 28) == 0xE &&
      (em_address < (0xE0000000 + memory.GetL1CacheSize())))
  {
    T value;
    std::memcpy(&value, &memory.GetL1Cache()[em_address & 0x0FFFFFFF], sizeof(T));
    return bswap(value);
  }

  if (memory.GetRAM() && (em_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    T value;
    std::memcpy(&value, &memory.GetRAM()[em_address & memory.GetRamMask()], sizeof(T));
    return bswap(value);
  }

  if (memory.GetEXRAM() && (em_address >> 28) == 0x1 &&
      (em_address & 0x0FFFFFFF) < memory.GetExRamSizeReal())
  {
    T value;
    std::memcpy(&value, &memory.GetEXRAM()[em_address & 0x0FFFFFFF], sizeof(T));
    return bswap(value);
  }

  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (memory.GetFakeVMEM() && ((em_address & 0xFE000000) == 0x7E000000))
  {
    T value;
    std::memcpy(&value, &memory.GetFakeVMEM()[em_address & memory.GetFakeVMemMask()], sizeof(T));
    return bswap(value);
  }

  PanicAlertFmt("Unable to resolve read address {:x} PC {:x}", em_address, PC);
  if (Core::System::GetInstance().IsPauseOnPanicMode())
  {
    CPU::Break();
    ppcState.Exceptions |= EXCEPTION_DSI | EXCEPTION_FAKE_MEMCHECK_HIT;
  }
  return 0;
}

template <XCheckTLBFlag flag, bool never_translate = false>
static void WriteToHardware(Memory::MemoryManager& memory, u32 em_address, const u32 data,
                            const u32 size)
{
  DEBUG_ASSERT(size <= 4);

  const u32 em_address_start_page = em_address & ~HW_PAGE_MASK;
  const u32 em_address_end_page = (em_address + size - 1) & ~HW_PAGE_MASK;
  if (em_address_start_page != em_address_end_page)
  {
    // The write crosses a page boundary. Break it up into two writes.
    // TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
    // Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
    const u32 first_half_size = em_address_end_page - em_address;
    const u32 second_half_size = size - first_half_size;
    WriteToHardware<flag, never_translate>(
        memory, em_address, Common::RotateRight(data, second_half_size * 8), first_half_size);
    WriteToHardware<flag, never_translate>(memory, em_address_end_page, data, second_half_size);
    return;
  }

  bool wi = false;

  if (!never_translate && MSR.DR)
  {
    auto translated_addr = TranslateAddress<flag>(em_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Write)
        GenerateDSIException(em_address, true);
      return;
    }
    em_address = translated_addr.address;
    wi = translated_addr.wi;
  }

  // Check for a gather pipe write (which are not implemented through the MMIO system).
  //
  // Note that we must mask the address to correctly emulate certain games; Pac-Man World 3
  // in particular is affected by this. (See https://bugs.dolphin-emu.org/issues/8386)
  //
  // The PowerPC 750CL manual says (in section 9.4.2 Write Gather Pipe Operation on page 327):
  // "A noncacheable store to an address with bits 0-26 matching WPAR[GB_ADDR] but with bits 27-31
  // not all zero will result in incorrect data in the buffer." So, it's possible that in some cases
  // writes which do not exactly match the masking behave differently, but Pac-Man World 3's writes
  // happen to behave correctly.
  if (flag == XCheckTLBFlag::Write &&
      (em_address & 0xFFFFF000) == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS)
  {
    switch (size)
    {
    case 1:
      GPFifo::Write8(static_cast<u8>(data));
      return;
    case 2:
      GPFifo::Write16(static_cast<u16>(data));
      return;
    case 4:
      GPFifo::Write32(data);
      return;
    default:
      // Some kind of misaligned write. TODO: Does this match how the actual hardware handles it?
      for (size_t i = size * 8; i > 0;)
      {
        i -= 8;
        GPFifo::Write8(static_cast<u8>(data >> i));
      }
      return;
    }
  }

  if (flag == XCheckTLBFlag::Write && (em_address & 0xF8000000) == 0x08000000)
  {
    if (em_address < 0x0c000000)
    {
      EFB_Write(data, em_address);
      return;
    }

    switch (size)
    {
    case 1:
      memory.GetMMIOMapping()->Write<u8>(em_address, static_cast<u8>(data));
      return;
    case 2:
      memory.GetMMIOMapping()->Write<u16>(em_address, static_cast<u16>(data));
      return;
    case 4:
      memory.GetMMIOMapping()->Write<u32>(em_address, data);
      return;
    default:
      // Some kind of misaligned write. TODO: Does this match how the actual hardware handles it?
      for (size_t i = size * 8; i > 0; em_address++)
      {
        i -= 8;
        memory.GetMMIOMapping()->Write<u8>(em_address, static_cast<u8>(data >> i));
      }
      return;
    }
  }

  const u32 swapped_data = Common::swap32(Common::RotateRight(data, size * 8));

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if (memory.GetL1Cache() && (em_address >> 28 == 0xE) &&
      (em_address < (0xE0000000 + memory.GetL1CacheSize())))
  {
    std::memcpy(&memory.GetL1Cache()[em_address & 0x0FFFFFFF], &swapped_data, size);
    return;
  }

  if (wi && (size < 4 || (em_address & 0x3)))
  {
    // When a write to memory is performed in hardware, 64 bits of data are sent to the memory
    // controller along with a mask. This mask is encoded using just two bits of data - one for
    // the upper 32 bits and one for the lower 32 bits - which leads to some odd data duplication
    // behavior for write-through/cache-inhibited writes with a start address or end address that
    // isn't 32-bit aligned. See https://bugs.dolphin-emu.org/issues/12565 for details.

    // TODO: This interrupt is supposed to have associated cause and address registers
    // TODO: This should trigger the hwtest's interrupt handling, but it does not seem to
    //       (https://github.com/dolphin-emu/hwtests/pull/42)
    ProcessorInterface::SetInterrupt(ProcessorInterface::INT_CAUSE_PI);

    const u32 rotated_data = Common::RotateRight(data, ((em_address & 0x3) + size) * 8);

    for (u32 addr = em_address & ~0x7; addr < em_address + size; addr += 8)
    {
      WriteToHardware<flag, true>(memory, addr, rotated_data, 4);
      WriteToHardware<flag, true>(memory, addr + 4, rotated_data, 4);
    }

    return;
  }

  if (memory.GetRAM() && (em_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    std::memcpy(&memory.GetRAM()[em_address & memory.GetRamMask()], &swapped_data, size);
    return;
  }

  if (memory.GetEXRAM() && (em_address >> 28) == 0x1 &&
      (em_address & 0x0FFFFFFF) < memory.GetExRamSizeReal())
  {
    std::memcpy(&memory.GetEXRAM()[em_address & 0x0FFFFFFF], &swapped_data, size);
    return;
  }

  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (memory.GetFakeVMEM() && ((em_address & 0xFE000000) == 0x7E000000))
  {
    std::memcpy(&memory.GetFakeVMEM()[em_address & memory.GetFakeVMemMask()], &swapped_data, size);
    return;
  }

  PanicAlertFmt("Unable to resolve write address {:x} PC {:x}", em_address, PC);
  if (Core::System::GetInstance().IsPauseOnPanicMode())
  {
    CPU::Break();
    ppcState.Exceptions |= EXCEPTION_DSI | EXCEPTION_FAKE_MEMCHECK_HIT;
  }
}
// =====================

// =================================
/* These functions are primarily called by the Interpreter functions and are routed to the correct
   location through ReadFromHardware and WriteToHardware */
// ----------------

static void GenerateISIException(u32 effective_address);

u32 Read_Opcode(u32 address)
{
  TryReadInstResult result = TryReadInstruction(address);
  if (!result.valid)
  {
    GenerateISIException(address);
    return 0;
  }
  return result.hex;
}

TryReadInstResult TryReadInstruction(u32 address)
{
  bool from_bat = true;
  if (MSR.IR)
  {
    auto tlb_addr = TranslateAddress<XCheckTLBFlag::Opcode>(address);
    if (!tlb_addr.Success())
    {
      return TryReadInstResult{false, false, 0, 0};
    }
    else
    {
      address = tlb_addr.address;
      from_bat = tlb_addr.result == TranslateAddressResultEnum::BAT_TRANSLATED;
    }
  }

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  u32 hex;
  // TODO: Refactor this. This icache implementation is totally wrong if used with the fake vmem.
  if (memory.GetFakeVMEM() && ((address & 0xFE000000) == 0x7E000000))
  {
    hex = Common::swap32(&memory.GetFakeVMEM()[address & memory.GetFakeVMemMask()]);
  }
  else
  {
    hex = PowerPC::ppcState.iCache.ReadInstruction(address);
  }
  return TryReadInstResult{true, from_bat, hex, address};
}

u32 HostRead_Instruction(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(memory, address);
}

std::optional<ReadResult<u32>> HostTryReadInstruction(const u32 address,
                                                      RequestedAddressSpace space)
{
  if (!HostIsInstructionRAMAddress(address, space))
    return std::nullopt;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (space)
  {
  case RequestedAddressSpace::Effective:
  {
    const u32 value = ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(memory, address);
    return ReadResult<u32>(!!MSR.DR, value);
  }
  case RequestedAddressSpace::Physical:
  {
    const u32 value =
        ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32, true>(memory, address);
    return ReadResult<u32>(false, value);
  }
  case RequestedAddressSpace::Virtual:
  {
    if (!MSR.DR)
      return std::nullopt;
    const u32 value = ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(memory, address);
    return ReadResult<u32>(true, value);
  }
  }

  ASSERT(0);
  return std::nullopt;
}

static void Memcheck(u32 address, u64 var, bool write, size_t size)
{
  if (!memchecks.HasAny())
    return;

  TMemCheck* mc = memchecks.GetMemCheck(address, size);
  if (mc == nullptr)
    return;

  if (CPU::IsStepping())
  {
    // Disable when stepping so that resume works.
    return;
  }

  mc->num_hits++;

  const bool pause = mc->Action(&debug_interface, var, address, write, size, PC);
  if (!pause)
    return;

  CPU::Break();

  if (GDBStub::IsActive())
    GDBStub::TakeControl();

  // Fake a DSI so that all the code that tests for it in order to skip
  // the rest of the instruction will apply.  (This means that
  // watchpoints will stop the emulator before the offending load/store,
  // not after like GDB does, but that's better anyway.  Just need to
  // make sure resuming after that works.)
  // It doesn't matter if ReadFromHardware triggers its own DSI because
  // we'll take it after resuming.
  ppcState.Exceptions |= EXCEPTION_DSI | EXCEPTION_FAKE_MEMCHECK_HIT;
}

u8 Read_U8(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u8 var = ReadFromHardware<XCheckTLBFlag::Read, u8>(memory, address);
  Memcheck(address, var, false, 1);
  return var;
}

u16 Read_U16(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u16 var = ReadFromHardware<XCheckTLBFlag::Read, u16>(memory, address);
  Memcheck(address, var, false, 2);
  return var;
}

u32 Read_U32(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u32 var = ReadFromHardware<XCheckTLBFlag::Read, u32>(memory, address);
  Memcheck(address, var, false, 4);
  return var;
}

u64 Read_U64(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  u64 var = ReadFromHardware<XCheckTLBFlag::Read, u64>(memory, address);
  Memcheck(address, var, false, 8);
  return var;
}

double Read_F64(const u32 address)
{
  const u64 integral = Read_U64(address);

  return Common::BitCast<double>(integral);
}

float Read_F32(const u32 address)
{
  const u32 integral = Read_U32(address);

  return Common::BitCast<float>(integral);
}

template <typename T>
static std::optional<ReadResult<T>> HostTryReadUX(const u32 address, RequestedAddressSpace space)
{
  if (!HostIsRAMAddress(address, space))
    return std::nullopt;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (space)
  {
  case RequestedAddressSpace::Effective:
  {
    T value = ReadFromHardware<XCheckTLBFlag::NoException, T>(memory, address);
    return ReadResult<T>(!!MSR.DR, std::move(value));
  }
  case RequestedAddressSpace::Physical:
  {
    T value = ReadFromHardware<XCheckTLBFlag::NoException, T, true>(memory, address);
    return ReadResult<T>(false, std::move(value));
  }
  case RequestedAddressSpace::Virtual:
  {
    if (!MSR.DR)
      return std::nullopt;
    T value = ReadFromHardware<XCheckTLBFlag::NoException, T>(memory, address);
    return ReadResult<T>(true, std::move(value));
  }
  }

  ASSERT(0);
  return std::nullopt;
}

std::optional<ReadResult<u8>> HostTryReadU8(u32 address, RequestedAddressSpace space)
{
  return HostTryReadUX<u8>(address, space);
}

std::optional<ReadResult<u16>> HostTryReadU16(u32 address, RequestedAddressSpace space)
{
  return HostTryReadUX<u16>(address, space);
}

std::optional<ReadResult<u32>> HostTryReadU32(u32 address, RequestedAddressSpace space)
{
  return HostTryReadUX<u32>(address, space);
}

std::optional<ReadResult<u64>> HostTryReadU64(u32 address, RequestedAddressSpace space)
{
  return HostTryReadUX<u64>(address, space);
}

std::optional<ReadResult<float>> HostTryReadF32(u32 address, RequestedAddressSpace space)
{
  const auto result = HostTryReadUX<u32>(address, space);
  if (!result)
    return std::nullopt;
  return ReadResult<float>(result->translated, Common::BitCast<float>(result->value));
}

std::optional<ReadResult<double>> HostTryReadF64(u32 address, RequestedAddressSpace space)
{
  const auto result = HostTryReadUX<u64>(address, space);
  if (!result)
    return std::nullopt;
  return ReadResult<double>(result->translated, Common::BitCast<double>(result->value));
}

u32 Read_U8_ZX(const u32 address)
{
  return Read_U8(address);
}

u32 Read_U16_ZX(const u32 address)
{
  return Read_U16(address);
}

void Write_U8(const u32 var, const u32 address)
{
  Memcheck(address, var, true, 1);
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::Write>(memory, address, var, 1);
}

void Write_U16(const u32 var, const u32 address)
{
  Memcheck(address, var, true, 2);
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::Write>(memory, address, var, 2);
}
void Write_U16_Swap(const u32 var, const u32 address)
{
  Write_U16((var & 0xFFFF0000) | Common::swap16(static_cast<u16>(var)), address);
}

void Write_U32(const u32 var, const u32 address)
{
  Memcheck(address, var, true, 4);
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::Write>(memory, address, var, 4);
}
void Write_U32_Swap(const u32 var, const u32 address)
{
  Write_U32(Common::swap32(var), address);
}

void Write_U64(const u64 var, const u32 address)
{
  Memcheck(address, var, true, 8);
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::Write>(memory, address, static_cast<u32>(var >> 32), 4);
  WriteToHardware<XCheckTLBFlag::Write>(memory, address + sizeof(u32), static_cast<u32>(var), 4);
}
void Write_U64_Swap(const u64 var, const u32 address)
{
  Write_U64(Common::swap64(var), address);
}

void Write_F64(const double var, const u32 address)
{
  const u64 integral = Common::BitCast<u64>(var);

  Write_U64(integral, address);
}

u8 HostRead_U8(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return ReadFromHardware<XCheckTLBFlag::NoException, u8>(memory, address);
}

u16 HostRead_U16(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return ReadFromHardware<XCheckTLBFlag::NoException, u16>(memory, address);
}

u32 HostRead_U32(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return ReadFromHardware<XCheckTLBFlag::NoException, u32>(memory, address);
}

u64 HostRead_U64(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  return ReadFromHardware<XCheckTLBFlag::NoException, u64>(memory, address);
}

float HostRead_F32(const u32 address)
{
  const u32 integral = HostRead_U32(address);

  return Common::BitCast<float>(integral);
}

double HostRead_F64(const u32 address)
{
  const u64 integral = HostRead_U64(address);

  return Common::BitCast<double>(integral);
}

void HostWrite_U8(const u32 var, const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::NoException>(memory, address, var, 1);
}

void HostWrite_U16(const u32 var, const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::NoException>(memory, address, var, 2);
}

void HostWrite_U32(const u32 var, const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::NoException>(memory, address, var, 4);
}

void HostWrite_U64(const u64 var, const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  WriteToHardware<XCheckTLBFlag::NoException>(memory, address, static_cast<u32>(var >> 32), 4);
  WriteToHardware<XCheckTLBFlag::NoException>(memory, address + sizeof(u32), static_cast<u32>(var),
                                              4);
}

void HostWrite_F32(const float var, const u32 address)
{
  const u32 integral = Common::BitCast<u32>(var);

  HostWrite_U32(integral, address);
}

void HostWrite_F64(const double var, const u32 address)
{
  const u64 integral = Common::BitCast<u64>(var);

  HostWrite_U64(integral, address);
}

static std::optional<WriteResult> HostTryWriteUX(const u32 var, const u32 address, const u32 size,
                                                 RequestedAddressSpace space)
{
  if (!HostIsRAMAddress(address, space))
    return std::nullopt;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (space)
  {
  case RequestedAddressSpace::Effective:
    WriteToHardware<XCheckTLBFlag::NoException>(memory, address, var, size);
    return WriteResult(!!MSR.DR);
  case RequestedAddressSpace::Physical:
    WriteToHardware<XCheckTLBFlag::NoException, true>(memory, address, var, size);
    return WriteResult(false);
  case RequestedAddressSpace::Virtual:
    if (!MSR.DR)
      return std::nullopt;
    WriteToHardware<XCheckTLBFlag::NoException>(memory, address, var, size);
    return WriteResult(true);
  }

  ASSERT(0);
  return std::nullopt;
}

std::optional<WriteResult> HostTryWriteU8(const u32 var, const u32 address,
                                          RequestedAddressSpace space)
{
  return HostTryWriteUX(var, address, 1, space);
}

std::optional<WriteResult> HostTryWriteU16(const u32 var, const u32 address,
                                           RequestedAddressSpace space)
{
  return HostTryWriteUX(var, address, 2, space);
}

std::optional<WriteResult> HostTryWriteU32(const u32 var, const u32 address,
                                           RequestedAddressSpace space)
{
  return HostTryWriteUX(var, address, 4, space);
}

std::optional<WriteResult> HostTryWriteU64(const u64 var, const u32 address,
                                           RequestedAddressSpace space)
{
  const auto result = HostTryWriteUX(static_cast<u32>(var >> 32), address, 4, space);
  if (!result)
    return result;

  return HostTryWriteUX(static_cast<u32>(var), address + 4, 4, space);
}

std::optional<WriteResult> HostTryWriteF32(const float var, const u32 address,
                                           RequestedAddressSpace space)
{
  const u32 integral = Common::BitCast<u32>(var);
  return HostTryWriteU32(integral, address, space);
}

std::optional<WriteResult> HostTryWriteF64(const double var, const u32 address,
                                           RequestedAddressSpace space)
{
  const u64 integral = Common::BitCast<u64>(var);
  return HostTryWriteU64(integral, address, space);
}

std::string HostGetString(u32 address, size_t size)
{
  std::string s;
  do
  {
    if (!HostIsRAMAddress(address))
      break;
    u8 res = HostRead_U8(address);
    if (!res)
      break;
    s += static_cast<char>(res);
    ++address;
  } while (size == 0 || s.length() < size);
  return s;
}

std::optional<ReadResult<std::string>> HostTryReadString(u32 address, size_t size,
                                                         RequestedAddressSpace space)
{
  auto c = HostTryReadU8(address, space);
  if (!c)
    return std::nullopt;
  if (c->value == 0)
    return ReadResult<std::string>(c->translated, "");

  std::string s;
  s += static_cast<char>(c->value);
  while (size == 0 || s.length() < size)
  {
    ++address;
    const auto res = HostTryReadU8(address, space);
    if (!res || res->value == 0)
      break;
    s += static_cast<char>(res->value);
  }
  return ReadResult<std::string>(c->translated, std::move(s));
}

bool IsOptimizableRAMAddress(const u32 address)
{
  if (PowerPC::memchecks.HasAny())
    return false;

  if (!MSR.DR)
    return false;

  // TODO: This API needs to take an access size
  //
  // We store whether an access can be optimized to an unchecked access
  // in dbat_table.
  u32 bat_result = dbat_table[address >> BAT_INDEX_SHIFT];
  return (bat_result & BAT_PHYSICAL_BIT) != 0;
}

template <XCheckTLBFlag flag>
static bool IsRAMAddress(Memory::MemoryManager& memory, u32 address, bool translate)
{
  if (translate)
  {
    auto translate_address = TranslateAddress<flag>(address);
    if (!translate_address.Success())
      return false;
    address = translate_address.address;
  }

  u32 segment = address >> 28;
  if (memory.GetRAM() && segment == 0x0 && (address & 0x0FFFFFFF) < memory.GetRamSizeReal())
  {
    return true;
  }
  else if (memory.GetEXRAM() && segment == 0x1 &&
           (address & 0x0FFFFFFF) < memory.GetExRamSizeReal())
  {
    return true;
  }
  else if (memory.GetFakeVMEM() && ((address & 0xFE000000) == 0x7E000000))
  {
    return true;
  }
  else if (memory.GetL1Cache() && segment == 0xE &&
           (address < (0xE0000000 + memory.GetL1CacheSize())))
  {
    return true;
  }
  return false;
}

bool HostIsRAMAddress(u32 address, RequestedAddressSpace space)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (space)
  {
  case RequestedAddressSpace::Effective:
    return IsRAMAddress<XCheckTLBFlag::NoException>(memory, address, MSR.DR);
  case RequestedAddressSpace::Physical:
    return IsRAMAddress<XCheckTLBFlag::NoException>(memory, address, false);
  case RequestedAddressSpace::Virtual:
    if (!MSR.DR)
      return false;
    return IsRAMAddress<XCheckTLBFlag::NoException>(memory, address, true);
  }

  ASSERT(0);
  return false;
}

bool HostIsInstructionRAMAddress(u32 address, RequestedAddressSpace space)
{
  // Instructions are always 32bit aligned.
  if (address & 3)
    return false;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  switch (space)
  {
  case RequestedAddressSpace::Effective:
    return IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(memory, address, MSR.IR);
  case RequestedAddressSpace::Physical:
    return IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(memory, address, false);
  case RequestedAddressSpace::Virtual:
    if (!MSR.IR)
      return false;
    return IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(memory, address, true);
  }

  ASSERT(0);
  return false;
}

void DMA_LCToMemory(const u32 mem_address, const u32 cache_address, const u32 num_blocks)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  // TODO: It's not completely clear this is the right spot for this code;
  // what would happen if, for example, the DVD drive tried to write to the EFB?
  // TODO: This is terribly slow.
  // TODO: Refactor.
  // Avatar: The Last Airbender (GC) uses this for videos.
  if ((mem_address & 0x0F000000) == 0x08000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF));
      EFB_Write(data, mem_address + i);
    }
    return;
  }

  // No known game uses this; here for completeness.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x0C000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF));
      memory.GetMMIOMapping()->Write(mem_address + i, data);
    }
    return;
  }

  const u8* src = memory.GetL1Cache() + (cache_address & 0x3FFFF);
  u8* dst = memory.GetPointer(mem_address);
  if (dst == nullptr)
    return;

  memcpy(dst, src, 32 * num_blocks);
}

void DMA_MemoryToLC(const u32 cache_address, const u32 mem_address, const u32 num_blocks)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  const u8* src = memory.GetPointer(mem_address);
  u8* dst = memory.GetL1Cache() + (cache_address & 0x3FFFF);

  // No known game uses this; here for completeness.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x08000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(EFB_Read(mem_address + i));
      std::memcpy(memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
    }
    return;
  }

  // No known game uses this.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x0C000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(memory.GetMMIOMapping()->Read<u32>(mem_address + i));
      std::memcpy(memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
    }
    return;
  }

  if (src == nullptr)
    return;

  memcpy(dst, src, 32 * num_blocks);
}

void ClearCacheLine(u32 address)
{
  DEBUG_ASSERT((address & 0x1F) == 0);
  if (MSR.DR)
  {
    auto translated_address = TranslateAddress<XCheckTLBFlag::Write>(address);
    if (translated_address.result == TranslateAddressResultEnum::DIRECT_STORE_SEGMENT)
    {
      // dcbz to direct store segments is ignored. This is a little
      // unintuitive, but this is consistent with both console and the PEM.
      // Advance Game Port crashes if we don't emulate this correctly.
      return;
    }
    if (translated_address.result == TranslateAddressResultEnum::PAGE_FAULT)
    {
      // If translation fails, generate a DSI.
      GenerateDSIException(address, true);
      return;
    }
    address = translated_address.address;
  }

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  // TODO: This isn't precisely correct for non-RAM regions, but the difference
  // is unlikely to matter.
  for (u32 i = 0; i < 32; i += 4)
    WriteToHardware<XCheckTLBFlag::Write, true>(memory, address + i, 0, 4);
}

u32 IsOptimizableMMIOAccess(u32 address, u32 access_size)
{
  if (PowerPC::memchecks.HasAny())
    return 0;

  if (!MSR.DR)
    return 0;

  // Translate address
  // If we also optimize for TLB mappings, we'd have to clear the
  // JitCache on each TLB invalidation.
  bool wi = false;
  if (!TranslateBatAddess(dbat_table, &address, &wi))
    return 0;

  // Check whether the address is an aligned address of an MMIO register.
  const bool aligned = (address & ((access_size >> 3) - 1)) == 0;
  if (!aligned || !MMIO::IsMMIOAddress(address))
    return 0;

  return address;
}

bool IsOptimizableGatherPipeWrite(u32 address)
{
  if (PowerPC::memchecks.HasAny())
    return false;

  if (!MSR.DR)
    return false;

  // Translate address, only check BAT mapping.
  // If we also optimize for TLB mappings, we'd have to clear the
  // JitCache on each TLB invalidation.
  bool wi = false;
  if (!TranslateBatAddess(dbat_table, &address, &wi))
    return false;

  // Check whether the translated address equals the address in WPAR.
  return address == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS;
}

TranslateResult JitCache_TranslateAddress(u32 address)
{
  if (!MSR.IR)
    return TranslateResult{address};

  // TODO: We shouldn't use FLAG_OPCODE if the caller is the debugger.
  const auto tlb_addr = TranslateAddress<XCheckTLBFlag::Opcode>(address);
  if (!tlb_addr.Success())
    return TranslateResult{};

  const bool from_bat = tlb_addr.result == TranslateAddressResultEnum::BAT_TRANSLATED;
  return TranslateResult{from_bat, tlb_addr.address};
}

// *********************************************************************************
// Warning: Test Area
//
// This code is for TESTING and it works in interpreter mode ONLY. Some games (like
// COD iirc) work thanks to this basic TLB emulation.
// It is just a small hack and we have never spend enough time to finalize it.
// Cheers PearPC!
//
// *********************************************************************************

/*
 * PearPC
 * ppc_mmu.cc
 *
 * Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

static void GenerateDSIException(u32 effective_address, bool write)
{
  // DSI exceptions are only supported in MMU mode.
  if (!Core::System::GetInstance().IsMMUMode())
  {
    PanicAlertFmt("Invalid {} {:#010x}, PC = {:#010x}", write ? "write to" : "read from",
                  effective_address, PC);
    if (Core::System::GetInstance().IsPauseOnPanicMode())
    {
      CPU::Break();
      ppcState.Exceptions |= EXCEPTION_DSI | EXCEPTION_FAKE_MEMCHECK_HIT;
    }
    return;
  }

  constexpr u32 dsisr_page = 1U << 30;
  constexpr u32 dsisr_store = 1U << 25;

  if (effective_address != 0)
    ppcState.spr[SPR_DSISR] = dsisr_page | dsisr_store;
  else
    ppcState.spr[SPR_DSISR] = dsisr_page;

  ppcState.spr[SPR_DAR] = effective_address;

  ppcState.Exceptions |= EXCEPTION_DSI;
}

static void GenerateISIException(u32 effective_address)
{
  // Address of instruction could not be translated
  NPC = effective_address;

  PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
  WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", PC);
}

void SDRUpdated()
{
  const auto sdr = UReg_SDR1{ppcState.spr[SPR_SDR]};
  const u32 htabmask = sdr.htabmask;

  if (!Common::IsValidLowMask(htabmask))
    WARN_LOG_FMT(POWERPC, "Invalid HTABMASK: 0b{:032b}", htabmask);

  // While 6xx_pem.pdf §7.6.1.1 mentions that the number of trailing zeros in HTABORG
  // must be equal to the number of trailing ones in the mask (i.e. HTABORG must be
  // properly aligned), this is actually not a hard requirement. Real hardware will just OR
  // the base address anyway. Ignoring SDR changes would lead to incorrect emulation.
  const u32 htaborg = sdr.htaborg;
  if ((htaborg & htabmask) != 0)
    WARN_LOG_FMT(POWERPC, "Invalid HTABORG: htaborg=0x{:08x} htabmask=0x{:08x}", htaborg, htabmask);

  ppcState.pagetable_base = htaborg << 16;
  ppcState.pagetable_hashmask = ((htabmask << 10) | 0x3ff);
}

enum class TLBLookupResult
{
  Found,
  NotFound,
  UpdateC
};

static TLBLookupResult LookupTLBPageAddress(const XCheckTLBFlag flag, const u32 vpa, u32* paddr,
                                            bool* wi)
{
  const u32 tag = vpa >> HW_PAGE_INDEX_SHIFT;
  TLBEntry& tlbe = ppcState.tlb[IsOpcodeFlag(flag)][tag & HW_PAGE_INDEX_MASK];

  if (tlbe.tag[0] == tag)
  {
    UPTE_Hi pte2(tlbe.pte[0]);

    // Check if C bit requires updating
    if (flag == XCheckTLBFlag::Write)
    {
      if (pte2.C == 0)
      {
        pte2.C = 1;
        tlbe.pte[0] = pte2.Hex;
        return TLBLookupResult::UpdateC;
      }
    }

    if (!IsNoExceptionFlag(flag))
      tlbe.recent = 0;

    *paddr = tlbe.paddr[0] | (vpa & 0xfff);
    *wi = (pte2.WIMG & 0b1100) != 0;

    return TLBLookupResult::Found;
  }
  if (tlbe.tag[1] == tag)
  {
    UPTE_Hi pte2(tlbe.pte[1]);

    // Check if C bit requires updating
    if (flag == XCheckTLBFlag::Write)
    {
      if (pte2.C == 0)
      {
        pte2.C = 1;
        tlbe.pte[1] = pte2.Hex;
        return TLBLookupResult::UpdateC;
      }
    }

    if (!IsNoExceptionFlag(flag))
      tlbe.recent = 1;

    *paddr = tlbe.paddr[1] | (vpa & 0xfff);
    *wi = (pte2.WIMG & 0b1100) != 0;

    return TLBLookupResult::Found;
  }
  return TLBLookupResult::NotFound;
}

static void UpdateTLBEntry(const XCheckTLBFlag flag, UPTE_Hi pte2, const u32 address)
{
  if (IsNoExceptionFlag(flag))
    return;

  const u32 tag = address >> HW_PAGE_INDEX_SHIFT;
  TLBEntry& tlbe = ppcState.tlb[IsOpcodeFlag(flag)][tag & HW_PAGE_INDEX_MASK];
  const u32 index = tlbe.recent == 0 && tlbe.tag[0] != TLBEntry::INVALID_TAG;
  tlbe.recent = index;
  tlbe.paddr[index] = pte2.RPN << HW_PAGE_INDEX_SHIFT;
  tlbe.pte[index] = pte2.Hex;
  tlbe.tag[index] = tag;
}

void InvalidateTLBEntry(u32 address)
{
  const u32 entry_index = (address >> HW_PAGE_INDEX_SHIFT) & HW_PAGE_INDEX_MASK;

  ppcState.tlb[0][entry_index].Invalidate();
  ppcState.tlb[1][entry_index].Invalidate();
}

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

// Page Address Translation
static TranslateAddressResult TranslatePageAddress(const EffectiveAddress address,
                                                   const XCheckTLBFlag flag, bool* wi)
{
  // TLB cache
  // This catches 99%+ of lookups in practice, so the actual page table entry code below doesn't
  // benefit much from optimization.
  u32 translated_address = 0;
  const TLBLookupResult res = LookupTLBPageAddress(flag, address.Hex, &translated_address, wi);
  if (res == TLBLookupResult::Found)
  {
    return TranslateAddressResult{TranslateAddressResultEnum::PAGE_TABLE_TRANSLATED,
                                  translated_address};
  }

  const auto sr = UReg_SR{ppcState.sr[address.SR]};

  if (sr.T != 0)
    return TranslateAddressResult{TranslateAddressResultEnum::DIRECT_STORE_SEGMENT, 0};

  // TODO: Handle KS/KP segment register flags.

  // No-execute segment register flag.
  if ((flag == XCheckTLBFlag::Opcode || flag == XCheckTLBFlag::OpcodeNoException) && sr.N != 0)
  {
    return TranslateAddressResult{TranslateAddressResultEnum::PAGE_FAULT, 0};
  }

  const u32 offset = address.offset;          // 12 bit
  const u32 page_index = address.page_index;  // 16 bit
  const u32 VSID = sr.VSID;                   // 24 bit
  const u32 api = address.API;                //  6 bit (part of page_index)

  // hash function no 1 "xor" .360
  u32 hash = (VSID ^ page_index);

  UPTE_Lo pte1;
  pte1.VSID = VSID;
  pte1.API = api;
  pte1.V = 1;

  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  for (int hash_func = 0; hash_func < 2; hash_func++)
  {
    // hash function no 2 "not" .360
    if (hash_func == 1)
    {
      hash = ~hash;
      pte1.H = 1;
    }

    u32 pteg_addr =
        ((hash & PowerPC::ppcState.pagetable_hashmask) << 6) | PowerPC::ppcState.pagetable_base;

    for (int i = 0; i < 8; i++, pteg_addr += 8)
    {
      const u32 pteg = memory.Read_U32(pteg_addr);

      if (pte1.Hex == pteg)
      {
        UPTE_Hi pte2(memory.Read_U32(pteg_addr + 4));

        // set the access bits
        switch (flag)
        {
        case XCheckTLBFlag::NoException:
        case XCheckTLBFlag::OpcodeNoException:
          break;
        case XCheckTLBFlag::Read:
          pte2.R = 1;
          break;
        case XCheckTLBFlag::Write:
          pte2.R = 1;
          pte2.C = 1;
          break;
        case XCheckTLBFlag::Opcode:
          pte2.R = 1;
          break;
        }

        if (!IsNoExceptionFlag(flag))
        {
          memory.Write_U32(pte2.Hex, pteg_addr + 4);
        }

        // We already updated the TLB entry if this was caused by a C bit.
        if (res != TLBLookupResult::UpdateC)
          UpdateTLBEntry(flag, pte2, address.Hex);

        *wi = (pte2.WIMG & 0b1100) != 0;

        return TranslateAddressResult{TranslateAddressResultEnum::PAGE_TABLE_TRANSLATED,
                                      (pte2.RPN << 12) | offset};
      }
    }
  }
  return TranslateAddressResult{TranslateAddressResultEnum::PAGE_FAULT, 0};
}

static void UpdateBATs(BatTable& bat_table, u32 base_spr)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  // TODO: Separate BATs for MSR.PR==0 and MSR.PR==1
  // TODO: Handle PP settings.
  // TODO: Check how hardware reacts to overlapping BATs (including
  // BATs which should cause a DSI).
  // TODO: Check how hardware reacts to invalid BATs (bad mask etc).
  for (int i = 0; i < 4; ++i)
  {
    const u32 spr = base_spr + i * 2;
    const UReg_BAT_Up batu{ppcState.spr[spr]};
    const UReg_BAT_Lo batl{ppcState.spr[spr + 1]};
    if (batu.VS == 0 && batu.VP == 0)
      continue;

    if ((batu.BEPI & batu.BL) != 0)
    {
      // With a valid BAT, the simplest way to match is
      // (input & ~BL_mask) == BEPI. For now, assume it's
      // implemented this way for invalid BATs as well.
      WARN_LOG_FMT(POWERPC, "Bad BAT setup: BEPI overlaps BL");
      continue;
    }
    if ((batl.BRPN & batu.BL) != 0)
    {
      // With a valid BAT, the simplest way to translate is
      // (input & BL_mask) | BRPN_address. For now, assume it's
      // implemented this way for invalid BATs as well.
      WARN_LOG_FMT(POWERPC, "Bad BAT setup: BPRN overlaps BL");
    }
    if (!Common::IsValidLowMask((u32)batu.BL))
    {
      // With a valid BAT, the simplest way of masking is
      // (input & ~BL_mask) for matching and (input & BL_mask) for
      // translation. For now, assume it's implemented this way for
      // invalid BATs as well.
      WARN_LOG_FMT(POWERPC, "Bad BAT setup: invalid mask in BL");
    }
    for (u32 j = 0; j <= batu.BL; ++j)
    {
      // Enumerate all bit-patterns which fit within the given mask.
      if ((j & batu.BL) == j)
      {
        // This bit is a little weird: if BRPN & j != 0, we end up with
        // a strange mapping. Need to check on hardware.
        u32 physical_address = (batl.BRPN | j) << BAT_INDEX_SHIFT;
        u32 virtual_address = (batu.BEPI | j) << BAT_INDEX_SHIFT;

        // BAT_MAPPED_BIT is whether the translation is valid
        // BAT_PHYSICAL_BIT is whether we can use the fastmem arena
        // BAT_WI_BIT is whether either W or I (of WIMG) is set
        u32 valid_bit = BAT_MAPPED_BIT;

        const bool wi = (batl.WIMG & 0b1100) != 0;
        if (wi)
          valid_bit |= BAT_WI_BIT;

        // Enable fastmem mappings for cached memory. There are quirks related to uncached memory
        // that fastmem doesn't emulate properly (though no normal games are known to rely on them).
        if (!wi)
        {
          if (memory.GetFakeVMEM() && (physical_address & 0xFE000000) == 0x7E000000)
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (physical_address < memory.GetRamSizeReal())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (memory.GetEXRAM() && physical_address >> 28 == 0x1 &&
                   (physical_address & 0x0FFFFFFF) < memory.GetExRamSizeReal())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (physical_address >> 28 == 0xE &&
                   physical_address < 0xE0000000 + memory.GetL1CacheSize())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
        }

        // Fastmem doesn't support memchecks, so disable it for all overlapping virtual pages.
        if (PowerPC::memchecks.OverlapsMemcheck(virtual_address, BAT_PAGE_SIZE))
          valid_bit &= ~BAT_PHYSICAL_BIT;

        // (BEPI | j) == (BEPI & ~BL) | (j & BL).
        bat_table[virtual_address >> BAT_INDEX_SHIFT] = physical_address | valid_bit;
      }
    }
  }
}

static void UpdateFakeMMUBat(BatTable& bat_table, u32 start_addr)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  for (u32 i = 0; i < (0x10000000 >> BAT_INDEX_SHIFT); ++i)
  {
    // Map from 0x4XXXXXXX or 0x7XXXXXXX to the range
    // [0x7E000000,0x80000000).
    u32 e_address = i + (start_addr >> BAT_INDEX_SHIFT);
    u32 p_address = 0x7E000000 | (i << BAT_INDEX_SHIFT & memory.GetFakeVMemMask());
    u32 flags = BAT_MAPPED_BIT | BAT_PHYSICAL_BIT;

    if (PowerPC::memchecks.OverlapsMemcheck(e_address << BAT_INDEX_SHIFT, BAT_PAGE_SIZE))
      flags &= ~BAT_PHYSICAL_BIT;

    bat_table[e_address] = p_address | flags;
  }
}

void DBATUpdated()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  dbat_table = {};
  UpdateBATs(dbat_table, SPR_DBAT0U);
  bool extended_bats = SConfig::GetInstance().bWii && HID4.SBE;
  if (extended_bats)
    UpdateBATs(dbat_table, SPR_DBAT4U);
  if (memory.GetFakeVMEM())
  {
    // In Fake-MMU mode, insert some extra entries into the BAT tables.
    UpdateFakeMMUBat(dbat_table, 0x40000000);
    UpdateFakeMMUBat(dbat_table, 0x70000000);
  }

#ifndef _ARCH_32
  memory.UpdateLogicalMemory(dbat_table);
#endif

  // IsOptimizable*Address and dcbz depends on the BAT mapping, so we need a flush here.
  JitInterface::ClearSafe();
}

void IBATUpdated()
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  ibat_table = {};
  UpdateBATs(ibat_table, SPR_IBAT0U);
  bool extended_bats = SConfig::GetInstance().bWii && HID4.SBE;
  if (extended_bats)
    UpdateBATs(ibat_table, SPR_IBAT4U);
  if (memory.GetFakeVMEM())
  {
    // In Fake-MMU mode, insert some extra entries into the BAT tables.
    UpdateFakeMMUBat(ibat_table, 0x40000000);
    UpdateFakeMMUBat(ibat_table, 0x70000000);
  }
  JitInterface::ClearSafe();
}

// Translate effective address using BAT or PAT.  Returns 0 if the address cannot be translated.
// Through the hardware looks up BAT and TLB in parallel, BAT is used first if available.
// So we first check if there is a matching BAT entry, else we look for the TLB in
// TranslatePageAddress().
template <const XCheckTLBFlag flag>
static TranslateAddressResult TranslateAddress(u32 address)
{
  bool wi = false;

  if (TranslateBatAddess(IsOpcodeFlag(flag) ? ibat_table : dbat_table, &address, &wi))
    return TranslateAddressResult{TranslateAddressResultEnum::BAT_TRANSLATED, address, wi};

  return TranslatePageAddress(EffectiveAddress{address}, flag, &wi);
}

std::optional<u32> GetTranslatedAddress(u32 address)
{
  auto result = TranslateAddress<XCheckTLBFlag::NoException>(address);
  if (!result.Success())
  {
    return std::nullopt;
  }
  return std::optional<u32>(result.address);
}

}  // namespace PowerPC
