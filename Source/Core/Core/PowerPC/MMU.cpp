// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/MMU.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/VideoBackendBase.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

namespace PowerPC
{
constexpr size_t HW_PAGE_SIZE = 4096;
constexpr u32 HW_PAGE_INDEX_SHIFT = 12;
constexpr u32 HW_PAGE_INDEX_MASK = 0x3f;

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

struct TranslateAddressResult
{
  enum
  {
    BAT_TRANSLATED,
    PAGE_TABLE_TRANSLATED,
    DIRECT_STORE_SEGMENT,
    PAGE_FAULT
  } result;
  u32 address;
  bool Success() const { return result <= PAGE_TABLE_TRANSLATED; }
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
    ERROR_LOG(MEMMAP, "Unimplemented Z+Color EFB read @ 0x%08x", addr);
  }
  else if (addr & 0x00400000)
  {
    var = g_video_backend->Video_AccessEFB(EFBAccessType::PeekZ, x, y, 0);
    DEBUG_LOG(MEMMAP, "EFB Z Read @ %u, %u\t= 0x%08x", x, y, var);
  }
  else
  {
    var = g_video_backend->Video_AccessEFB(EFBAccessType::PeekColor, x, y, 0);
    DEBUG_LOG(MEMMAP, "EFB Color Read @ %u, %u\t= 0x%08x", x, y, var);
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
    ERROR_LOG(MEMMAP, "Unimplemented Z+Color EFB write. %08x @ 0x%08x", data, addr);
  }
  else if (addr & 0x00400000)
  {
    g_video_backend->Video_AccessEFB(EFBAccessType::PokeZ, x, y, data);
    DEBUG_LOG(MEMMAP, "EFB Z Write %08x @ %u, %u", data, x, y);
  }
  else
  {
    g_video_backend->Video_AccessEFB(EFBAccessType::PokeColor, x, y, data);
    DEBUG_LOG(MEMMAP, "EFB Color Write %08x @ %u, %u", data, x, y);
  }
}

BatTable ibat_table;
BatTable dbat_table;

static void GenerateDSIException(u32 effective_address, bool write);

template <XCheckTLBFlag flag, typename T, bool never_translate = false>
static T ReadFromHardware(u32 em_address)
{
  if (!never_translate && MSR.DR)
  {
    auto translated_addr = TranslateAddress<flag>(em_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Read)
        GenerateDSIException(em_address, false);
      return 0;
    }
    if ((em_address & (HW_PAGE_SIZE - 1)) > HW_PAGE_SIZE - sizeof(T))
    {
      // This could be unaligned down to the byte level... hopefully this is rare, so doing it this
      // way isn't too terrible.
      // TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
      // Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
      u32 em_address_next_page = (em_address + sizeof(T) - 1) & ~(HW_PAGE_SIZE - 1);
      auto addr_next_page = TranslateAddress<flag>(em_address_next_page);
      if (!addr_next_page.Success())
      {
        if (flag == XCheckTLBFlag::Read)
          GenerateDSIException(em_address_next_page, false);
        return 0;
      }
      T var = 0;
      u32 addr_translated = translated_addr.address;
      for (u32 addr = em_address; addr < em_address + sizeof(T); addr++, addr_translated++)
      {
        if (addr == em_address_next_page)
          addr_translated = addr_next_page.address;
        var = (var << 8) | ReadFromHardware<flag, u8, true>(addr_translated);
      }
      return var;
    }
    em_address = translated_addr.address;
  }

  // TODO: Make sure these are safe for unaligned addresses.

  if ((em_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    // TODO: Only the first REALRAM_SIZE is supposed to be backed by actual memory.
    T value;
    std::memcpy(&value, &Memory::m_pRAM[em_address & Memory::RAM_MASK], sizeof(T));
    return bswap(value);
  }

  if (Memory::m_pEXRAM && (em_address >> 28) == 0x1 &&
      (em_address & 0x0FFFFFFF) < Memory::EXRAM_SIZE)
  {
    T value;
    std::memcpy(&value, &Memory::m_pEXRAM[em_address & 0x0FFFFFFF], sizeof(T));
    return bswap(value);
  }

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if ((em_address >> 28) == 0xE && (em_address < (0xE0000000 + Memory::L1_CACHE_SIZE)))
  {
    T value;
    std::memcpy(&value, &Memory::m_pL1Cache[em_address & 0x0FFFFFFF], sizeof(T));
    return bswap(value);
  }
  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (Memory::m_pFakeVMEM && ((em_address & 0xFE000000) == 0x7E000000))
  {
    T value;
    std::memcpy(&value, &Memory::m_pFakeVMEM[em_address & Memory::RAM_MASK], sizeof(T));
    return bswap(value);
  }

  if (flag == XCheckTLBFlag::Read && (em_address & 0xF8000000) == 0x08000000)
  {
    if (em_address < 0x0c000000)
      return EFB_Read(em_address);
    else
      return (T)Memory::mmio_mapping->Read<typename std::make_unsigned<T>::type>(em_address);
  }

  PanicAlert("Unable to resolve read address %x PC %x", em_address, PC);
  return 0;
}

template <XCheckTLBFlag flag, typename T, bool never_translate = false>
static void WriteToHardware(u32 em_address, const T data)
{
  if (!never_translate && MSR.DR)
  {
    auto translated_addr = TranslateAddress<flag>(em_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Write)
        GenerateDSIException(em_address, true);
      return;
    }
    if ((em_address & (sizeof(T) - 1)) &&
        (em_address & (HW_PAGE_SIZE - 1)) > HW_PAGE_SIZE - sizeof(T))
    {
      // This could be unaligned down to the byte level... hopefully this is rare, so doing it this
      // way isn't too terrible.
      // TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
      // Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
      u32 em_address_next_page = (em_address + sizeof(T) - 1) & ~(HW_PAGE_SIZE - 1);
      auto addr_next_page = TranslateAddress<flag>(em_address_next_page);
      if (!addr_next_page.Success())
      {
        if (flag == XCheckTLBFlag::Write)
          GenerateDSIException(em_address_next_page, true);
        return;
      }
      T val = bswap(data);
      u32 addr_translated = translated_addr.address;
      for (size_t i = 0; i < sizeof(T); i++, addr_translated++)
      {
        if (em_address + i == em_address_next_page)
          addr_translated = addr_next_page.address;
        WriteToHardware<flag, u8, true>(addr_translated, static_cast<u8>(val >> (i * 8)));
      }
      return;
    }
    em_address = translated_addr.address;
  }

  // TODO: Make sure these are safe for unaligned addresses.

  if ((em_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    // TODO: Only the first REALRAM_SIZE is supposed to be backed by actual memory.
    const T swapped_data = bswap(data);
    std::memcpy(&Memory::m_pRAM[em_address & Memory::RAM_MASK], &swapped_data, sizeof(T));
    return;
  }

  if (Memory::m_pEXRAM && (em_address >> 28) == 0x1 &&
      (em_address & 0x0FFFFFFF) < Memory::EXRAM_SIZE)
  {
    const T swapped_data = bswap(data);
    std::memcpy(&Memory::m_pEXRAM[em_address & 0x0FFFFFFF], &swapped_data, sizeof(T));
    return;
  }

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if ((em_address >> 28 == 0xE) && (em_address < (0xE0000000 + Memory::L1_CACHE_SIZE)))
  {
    const T swapped_data = bswap(data);
    std::memcpy(&Memory::m_pL1Cache[em_address & 0x0FFFFFFF], &swapped_data, sizeof(T));
    return;
  }

  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (Memory::m_pFakeVMEM && ((em_address & 0xFE000000) == 0x7E000000))
  {
    const T swapped_data = bswap(data);
    std::memcpy(&Memory::m_pFakeVMEM[em_address & Memory::RAM_MASK], &swapped_data, sizeof(T));
    return;
  }

  // Check for a gather pipe write.
  // Note that we must mask the address to correctly emulate certain games;
  // Pac-Man World 3 in particular is affected by this.
  if (flag == XCheckTLBFlag::Write && (em_address & 0xFFFFF000) == 0x0C008000)
  {
    switch (sizeof(T))
    {
    case 1:
      GPFifo::Write8((u8)data);
      return;
    case 2:
      GPFifo::Write16((u16)data);
      return;
    case 4:
      GPFifo::Write32((u32)data);
      return;
    case 8:
      GPFifo::Write64((u64)data);
      return;
    }
  }

  if (flag == XCheckTLBFlag::Write && (em_address & 0xF8000000) == 0x08000000)
  {
    if (em_address < 0x0c000000)
    {
      EFB_Write((u32)data, em_address);
      return;
    }
    else
    {
      Memory::mmio_mapping->Write(em_address, data);
      return;
    }
  }

  PanicAlert("Unable to resolve write address %x PC %x", em_address, PC);
  return;
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
      from_bat = tlb_addr.result == TranslateAddressResult::BAT_TRANSLATED;
    }
  }

  u32 hex;
  // TODO: Refactor this. This icache implementation is totally wrong if used with the fake vmem.
  if (Memory::m_pFakeVMEM && ((address & 0xFE000000) == 0x7E000000))
  {
    hex = Common::swap32(&Memory::m_pFakeVMEM[address & Memory::FAKEVMEM_MASK]);
  }
  else
  {
    hex = PowerPC::ppcState.iCache.ReadInstruction(address);
  }
  return TryReadInstResult{true, from_bat, hex, address};
}

u32 HostRead_Instruction(const u32 address)
{
  UGeckoInstruction inst = HostRead_U32(address);
  return inst.hex;
}

static void Memcheck(u32 address, u32 var, bool write, size_t size)
{
  if (PowerPC::memchecks.HasAny())
  {
    TMemCheck* mc = PowerPC::memchecks.GetMemCheck(address, size);
    if (mc)
    {
      if (CPU::IsStepping())
      {
        // Disable when stepping so that resume works.
        return;
      }
      mc->num_hits++;
      bool pause = mc->Action(&PowerPC::debug_interface, var, address, write, size, PC);
      if (pause)
      {
        CPU::Break();
        // Fake a DSI so that all the code that tests for it in order to skip
        // the rest of the instruction will apply.  (This means that
        // watchpoints will stop the emulator before the offending load/store,
        // not after like GDB does, but that's better anyway.  Just need to
        // make sure resuming after that works.)
        // It doesn't matter if ReadFromHardware triggers its own DSI because
        // we'll take it after resuming.
        PowerPC::ppcState.Exceptions |= EXCEPTION_DSI | EXCEPTION_FAKE_MEMCHECK_HIT;
      }
    }
  }
}

u8 Read_U8(const u32 address)
{
  u8 var = ReadFromHardware<XCheckTLBFlag::Read, u8>(address);
  Memcheck(address, var, false, 1);
  return var;
}

u16 Read_U16(const u32 address)
{
  u16 var = ReadFromHardware<XCheckTLBFlag::Read, u16>(address);
  Memcheck(address, var, false, 2);
  return var;
}

u32 Read_U32(const u32 address)
{
  u32 var = ReadFromHardware<XCheckTLBFlag::Read, u32>(address);
  Memcheck(address, var, false, 4);
  return var;
}

u64 Read_U64(const u32 address)
{
  u64 var = ReadFromHardware<XCheckTLBFlag::Read, u64>(address);
  Memcheck(address, (u32)var, false, 8);
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

u32 Read_U8_ZX(const u32 address)
{
  return Read_U8(address);
}

u32 Read_U16_ZX(const u32 address)
{
  return Read_U16(address);
}

void Write_U8(const u8 var, const u32 address)
{
  Memcheck(address, var, true, 1);
  WriteToHardware<XCheckTLBFlag::Write, u8>(address, var);
}

void Write_U16(const u16 var, const u32 address)
{
  Memcheck(address, var, true, 2);
  WriteToHardware<XCheckTLBFlag::Write, u16>(address, var);
}
void Write_U16_Swap(const u16 var, const u32 address)
{
  Memcheck(address, var, true, 2);
  Write_U16(Common::swap16(var), address);
}

void Write_U32(const u32 var, const u32 address)
{
  Memcheck(address, var, true, 4);
  WriteToHardware<XCheckTLBFlag::Write, u32>(address, var);
}
void Write_U32_Swap(const u32 var, const u32 address)
{
  Memcheck(address, var, true, 4);
  Write_U32(Common::swap32(var), address);
}

void Write_U64(const u64 var, const u32 address)
{
  Memcheck(address, (u32)var, true, 8);
  WriteToHardware<XCheckTLBFlag::Write, u64>(address, var);
}
void Write_U64_Swap(const u64 var, const u32 address)
{
  Memcheck(address, (u32)var, true, 8);
  Write_U64(Common::swap64(var), address);
}

void Write_F64(const double var, const u32 address)
{
  const u64 integral = Common::BitCast<u64>(var);

  Write_U64(integral, address);
}

u8 HostRead_U8(const u32 address)
{
  return ReadFromHardware<XCheckTLBFlag::NoException, u8>(address);
}

u16 HostRead_U16(const u32 address)
{
  return ReadFromHardware<XCheckTLBFlag::NoException, u16>(address);
}

u32 HostRead_U32(const u32 address)
{
  return ReadFromHardware<XCheckTLBFlag::NoException, u32>(address);
}

u64 HostRead_U64(const u32 address)
{
  return ReadFromHardware<XCheckTLBFlag::NoException, u64>(address);
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

void HostWrite_U8(const u8 var, const u32 address)
{
  WriteToHardware<XCheckTLBFlag::NoException, u8>(address, var);
}

void HostWrite_U16(const u16 var, const u32 address)
{
  WriteToHardware<XCheckTLBFlag::NoException, u16>(address, var);
}

void HostWrite_U32(const u32 var, const u32 address)
{
  WriteToHardware<XCheckTLBFlag::NoException, u32>(address, var);
}

void HostWrite_U64(const u64 var, const u32 address)
{
  WriteToHardware<XCheckTLBFlag::NoException, u64>(address, var);
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
static bool IsRAMAddress(u32 address, bool translate)
{
  if (translate)
  {
    auto translate_address = TranslateAddress<flag>(address);
    if (!translate_address.Success())
      return false;
    address = translate_address.address;
  }

  u32 segment = address >> 28;
  if (segment == 0x0 && (address & 0x0FFFFFFF) < Memory::REALRAM_SIZE)
    return true;
  else if (Memory::m_pEXRAM && segment == 0x1 && (address & 0x0FFFFFFF) < Memory::EXRAM_SIZE)
    return true;
  else if (Memory::m_pFakeVMEM && ((address & 0xFE000000) == 0x7E000000))
    return true;
  else if (segment == 0xE && (address < (0xE0000000 + Memory::L1_CACHE_SIZE)))
    return true;
  return false;
}

bool HostIsRAMAddress(u32 address)
{
  return IsRAMAddress<XCheckTLBFlag::NoException>(address, MSR.DR);
}

bool HostIsInstructionRAMAddress(u32 address)
{
  // Instructions are always 32bit aligned.
  return !(address & 3) && IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(address, MSR.IR);
}

void DMA_LCToMemory(const u32 mem_address, const u32 cache_address, const u32 num_blocks)
{
  // TODO: It's not completely clear this is the right spot for this code;
  // what would happen if, for example, the DVD drive tried to write to the EFB?
  // TODO: This is terribly slow.
  // TODO: Refactor.
  // Avatar: The Last Airbender (GC) uses this for videos.
  if ((mem_address & 0x0F000000) == 0x08000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(Memory::m_pL1Cache + ((cache_address + i) & 0x3FFFF));
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
      const u32 data = Common::swap32(Memory::m_pL1Cache + ((cache_address + i) & 0x3FFFF));
      Memory::mmio_mapping->Write(mem_address + i, data);
    }
    return;
  }

  const u8* src = Memory::m_pL1Cache + (cache_address & 0x3FFFF);
  u8* dst = Memory::GetPointer(mem_address);
  if (dst == nullptr)
    return;

  memcpy(dst, src, 32 * num_blocks);
}

void DMA_MemoryToLC(const u32 cache_address, const u32 mem_address, const u32 num_blocks)
{
  const u8* src = Memory::GetPointer(mem_address);
  u8* dst = Memory::m_pL1Cache + (cache_address & 0x3FFFF);

  // No known game uses this; here for completeness.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x08000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(EFB_Read(mem_address + i));
      std::memcpy(Memory::m_pL1Cache + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
    }
    return;
  }

  // No known game uses this.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x0C000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(Memory::mmio_mapping->Read<u32>(mem_address + i));
      std::memcpy(Memory::m_pL1Cache + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
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
    if (translated_address.result == TranslateAddressResult::DIRECT_STORE_SEGMENT)
    {
      // dcbz to direct store segments is ignored. This is a little
      // unintuitive, but this is consistent with both console and the PEM.
      // Advance Game Port crashes if we don't emulate this correctly.
      return;
    }
    if (translated_address.result == TranslateAddressResult::PAGE_FAULT)
    {
      // If translation fails, generate a DSI.
      GenerateDSIException(address, true);
      return;
    }
    address = translated_address.address;
  }

  // TODO: This isn't precisely correct for non-RAM regions, but the difference
  // is unlikely to matter.
  for (u32 i = 0; i < 32; i += 8)
    WriteToHardware<XCheckTLBFlag::Write, u64, true>(address + i, 0);
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
  if (!TranslateBatAddess(dbat_table, &address))
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
  if (!TranslateBatAddess(dbat_table, &address))
    return false;

  // Check whether the translated address equals the address in WPAR.
  return address == 0x0C008000;
}

TranslateResult JitCache_TranslateAddress(u32 address)
{
  if (!MSR.IR)
    return TranslateResult{true, true, address};

  // TODO: We shouldn't use FLAG_OPCODE if the caller is the debugger.
  auto tlb_addr = TranslateAddress<XCheckTLBFlag::Opcode>(address);
  if (!tlb_addr.Success())
  {
    return TranslateResult{false, false, 0};
  }

  bool from_bat = tlb_addr.result == TranslateAddressResult::BAT_TRANSLATED;
  return TranslateResult{true, from_bat, tlb_addr.address};
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

#define PPC_EXC_DSISR_PAGE (1 << 30)
#define PPC_EXC_DSISR_PROT (1 << 27)
#define PPC_EXC_DSISR_STORE (1 << 25)

#define SDR1_HTABORG(v) (((v) >> 16) & 0xffff)
#define SDR1_HTABMASK(v) ((v)&0x1ff)
#define SDR1_PAGETABLE_BASE(v) ((v)&0xffff)
#define SR_T (1 << 31)
#define SR_Ks (1 << 30)
#define SR_Kp (1 << 29)
#define SR_N (1 << 28)
#define SR_VSID(v) ((v)&0xffffff)
#define SR_BUID(v) (((v) >> 20) & 0x1ff)
#define SR_CNTRL_SPEC(v) ((v)&0xfffff)

#define EA_SR(v) (((v) >> 28) & 0xf)
#define EA_PageIndex(v) (((v) >> 12) & 0xffff)
#define EA_Offset(v) ((v)&0xfff)
#define EA_API(v) (((v) >> 22) & 0x3f)

#define PA_RPN(v) (((v) >> 12) & 0xfffff)
#define PA_Offset(v) ((v)&0xfff)

#define PTE1_V (1 << 31)
#define PTE1_VSID(v) (((v) >> 7) & 0xffffff)
#define PTE1_H (1 << 6)
#define PTE1_API(v) ((v)&0x3f)

#define PTE2_RPN(v) ((v)&0xfffff000)
#define PTE2_R (1 << 8)
#define PTE2_C (1 << 7)
#define PTE2_WIMG(v) (((v) >> 3) & 0xf)
#define PTE2_PP(v) ((v)&3)

// Hey! these duplicate a structure in Gekko.h
union UPTE1
{
  struct
  {
    u32 API : 6;
    u32 H : 1;
    u32 VSID : 24;
    u32 V : 1;
  };
  u32 Hex;
};

union UPTE2
{
  struct
  {
    u32 PP : 2;
    u32 : 1;
    u32 WIMG : 4;
    u32 C : 1;
    u32 R : 1;
    u32 : 3;
    u32 RPN : 20;
  };
  u32 Hex;
};

static void GenerateDSIException(u32 effective_address, bool write)
{
  // DSI exceptions are only supported in MMU mode.
  if (!SConfig::GetInstance().bMMU)
  {
    PanicAlert("Invalid %s 0x%08x, PC = 0x%08x ", write ? "write to" : "read from",
               effective_address, PC);
    return;
  }

  if (effective_address)
    PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
  else
    PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE;

  PowerPC::ppcState.spr[SPR_DAR] = effective_address;

  PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
}

static void GenerateISIException(u32 effective_address)
{
  // Address of instruction could not be translated
  NPC = effective_address;

  PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
  WARN_LOG(POWERPC, "ISI exception at 0x%08x", PC);
}

void SDRUpdated()
{
  u32 htabmask = SDR1_HTABMASK(PowerPC::ppcState.spr[SPR_SDR]);
  if (!Common::IsValidLowMask(htabmask))
  {
    return;
  }
  u32 htaborg = SDR1_HTABORG(PowerPC::ppcState.spr[SPR_SDR]);
  if (htaborg & htabmask)
  {
    return;
  }
  PowerPC::ppcState.pagetable_base = htaborg << 16;
  PowerPC::ppcState.pagetable_hashmask = ((htabmask << 10) | 0x3ff);
}

enum class TLBLookupResult
{
  Found,
  NotFound,
  UpdateC
};

static TLBLookupResult LookupTLBPageAddress(const XCheckTLBFlag flag, const u32 vpa, u32* paddr)
{
  const u32 tag = vpa >> HW_PAGE_INDEX_SHIFT;
  TLBEntry& tlbe = ppcState.tlb[IsOpcodeFlag(flag)][tag & HW_PAGE_INDEX_MASK];

  if (tlbe.tag[0] == tag)
  {
    // Check if C bit requires updating
    if (flag == XCheckTLBFlag::Write)
    {
      UPTE2 PTE2;
      PTE2.Hex = tlbe.pte[0];
      if (PTE2.C == 0)
      {
        PTE2.C = 1;
        tlbe.pte[0] = PTE2.Hex;
        return TLBLookupResult::UpdateC;
      }
    }

    if (!IsNoExceptionFlag(flag))
      tlbe.recent = 0;

    *paddr = tlbe.paddr[0] | (vpa & 0xfff);

    return TLBLookupResult::Found;
  }
  if (tlbe.tag[1] == tag)
  {
    // Check if C bit requires updating
    if (flag == XCheckTLBFlag::Write)
    {
      UPTE2 PTE2;
      PTE2.Hex = tlbe.pte[1];
      if (PTE2.C == 0)
      {
        PTE2.C = 1;
        tlbe.pte[1] = PTE2.Hex;
        return TLBLookupResult::UpdateC;
      }
    }

    if (!IsNoExceptionFlag(flag))
      tlbe.recent = 1;

    *paddr = tlbe.paddr[1] | (vpa & 0xfff);

    return TLBLookupResult::Found;
  }
  return TLBLookupResult::NotFound;
}

static void UpdateTLBEntry(const XCheckTLBFlag flag, UPTE2 PTE2, const u32 address)
{
  if (IsNoExceptionFlag(flag))
    return;

  const int tag = address >> HW_PAGE_INDEX_SHIFT;
  TLBEntry& tlbe = ppcState.tlb[IsOpcodeFlag(flag)][tag & HW_PAGE_INDEX_MASK];
  const int index = tlbe.recent == 0 && tlbe.tag[0] != TLBEntry::INVALID_TAG;
  tlbe.recent = index;
  tlbe.paddr[index] = PTE2.RPN << HW_PAGE_INDEX_SHIFT;
  tlbe.pte[index] = PTE2.Hex;
  tlbe.tag[index] = tag;
}

void InvalidateTLBEntry(u32 address)
{
  const u32 entry_index = (address >> HW_PAGE_INDEX_SHIFT) & HW_PAGE_INDEX_MASK;

  TLBEntry& tlbe = ppcState.tlb[0][entry_index];
  tlbe.tag[0] = TLBEntry::INVALID_TAG;
  tlbe.tag[1] = TLBEntry::INVALID_TAG;

  TLBEntry& tlbe_i = ppcState.tlb[1][entry_index];
  tlbe_i.tag[0] = TLBEntry::INVALID_TAG;
  tlbe_i.tag[1] = TLBEntry::INVALID_TAG;
}

// Page Address Translation
static TranslateAddressResult TranslatePageAddress(const u32 address, const XCheckTLBFlag flag)
{
  // TLB cache
  // This catches 99%+ of lookups in practice, so the actual page table entry code below doesn't
  // benefit
  // much from optimization.
  u32 translatedAddress = 0;
  TLBLookupResult res = LookupTLBPageAddress(flag, address, &translatedAddress);
  if (res == TLBLookupResult::Found)
    return TranslateAddressResult{TranslateAddressResult::PAGE_TABLE_TRANSLATED, translatedAddress};

  u32 sr = PowerPC::ppcState.sr[EA_SR(address)];

  if (sr & 0x80000000)
    return TranslateAddressResult{TranslateAddressResult::DIRECT_STORE_SEGMENT, 0};

  // TODO: Handle KS/KP segment register flags.

  // No-execute segment register flag.
  if ((flag == XCheckTLBFlag::Opcode || flag == XCheckTLBFlag::OpcodeNoException) &&
      (sr & 0x10000000))
  {
    return TranslateAddressResult{TranslateAddressResult::PAGE_FAULT, 0};
  }

  u32 offset = EA_Offset(address);         // 12 bit
  u32 page_index = EA_PageIndex(address);  // 16 bit
  u32 VSID = SR_VSID(sr);                  // 24 bit
  u32 api = EA_API(address);               //  6 bit (part of page_index)

  // hash function no 1 "xor" .360
  u32 hash = (VSID ^ page_index);
  u32 pte1 = Common::swap32((VSID << 7) | api | PTE1_V);

  for (int hash_func = 0; hash_func < 2; hash_func++)
  {
    // hash function no 2 "not" .360
    if (hash_func == 1)
    {
      hash = ~hash;
      pte1 |= PTE1_H << 24;
    }

    u32 pteg_addr =
        ((hash & PowerPC::ppcState.pagetable_hashmask) << 6) | PowerPC::ppcState.pagetable_base;

    for (int i = 0; i < 8; i++, pteg_addr += 8)
    {
      u32 pteg;
      std::memcpy(&pteg, &Memory::physical_base[pteg_addr], sizeof(u32));

      if (pte1 == pteg)
      {
        UPTE2 PTE2;
        PTE2.Hex = Common::swap32(&Memory::physical_base[pteg_addr + 4]);

        // set the access bits
        switch (flag)
        {
        case XCheckTLBFlag::NoException:
        case XCheckTLBFlag::OpcodeNoException:
          break;
        case XCheckTLBFlag::Read:
          PTE2.R = 1;
          break;
        case XCheckTLBFlag::Write:
          PTE2.R = 1;
          PTE2.C = 1;
          break;
        case XCheckTLBFlag::Opcode:
          PTE2.R = 1;
          break;
        }

        if (!IsNoExceptionFlag(flag))
        {
          const u32 swapped_pte2 = Common::swap32(PTE2.Hex);
          std::memcpy(&Memory::physical_base[pteg_addr + 4], &swapped_pte2, sizeof(u32));
        }

        // We already updated the TLB entry if this was caused by a C bit.
        if (res != TLBLookupResult::UpdateC)
          UpdateTLBEntry(flag, PTE2, address);

        return TranslateAddressResult{TranslateAddressResult::PAGE_TABLE_TRANSLATED,
                                      (PTE2.RPN << 12) | offset};
      }
    }
  }
  return TranslateAddressResult{TranslateAddressResult::PAGE_FAULT, 0};
}

static void UpdateBATs(BatTable& bat_table, u32 base_spr)
{
  // TODO: Separate BATs for MSR.PR==0 and MSR.PR==1
  // TODO: Handle PP/WIMG settings.
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
      WARN_LOG(POWERPC, "Bad BAT setup: BEPI overlaps BL");
      continue;
    }
    if ((batl.BRPN & batu.BL) != 0)
    {
      // With a valid BAT, the simplest way to translate is
      // (input & BL_mask) | BRPN_address. For now, assume it's
      // implemented this way for invalid BATs as well.
      WARN_LOG(POWERPC, "Bad BAT setup: BPRN overlaps BL");
    }
    if (!Common::IsValidLowMask((u32)batu.BL))
    {
      // With a valid BAT, the simplest way of masking is
      // (input & ~BL_mask) for matching and (input & BL_mask) for
      // translation. For now, assume it's implemented this way for
      // invalid BATs as well.
      WARN_LOG(POWERPC, "Bad BAT setup: invalid mask in BL");
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

        // The bottom bit is whether the translation is valid; the second
        // bit from the bottom is whether we can use the fastmem arena.
        u32 valid_bit = BAT_MAPPED_BIT;
        if (Memory::m_pFakeVMEM && (physical_address & 0xFE000000) == 0x7E000000)
          valid_bit |= BAT_PHYSICAL_BIT;
        else if (physical_address < Memory::REALRAM_SIZE)
          valid_bit |= BAT_PHYSICAL_BIT;
        else if (Memory::m_pEXRAM && physical_address >> 28 == 0x1 &&
                 (physical_address & 0x0FFFFFFF) < Memory::EXRAM_SIZE)
          valid_bit |= BAT_PHYSICAL_BIT;
        else if (physical_address >> 28 == 0xE &&
                 physical_address < 0xE0000000 + Memory::L1_CACHE_SIZE)
          valid_bit |= BAT_PHYSICAL_BIT;

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
  for (u32 i = 0; i < (0x10000000 >> BAT_INDEX_SHIFT); ++i)
  {
    // Map from 0x4XXXXXXX or 0x7XXXXXXX to the range
    // [0x7E000000,0x80000000).
    u32 e_address = i + (start_addr >> BAT_INDEX_SHIFT);
    u32 p_address = 0x7E000000 | (i << BAT_INDEX_SHIFT & Memory::FAKEVMEM_MASK);
    u32 flags = BAT_MAPPED_BIT | BAT_PHYSICAL_BIT;

    if (PowerPC::memchecks.OverlapsMemcheck(e_address << BAT_INDEX_SHIFT, BAT_PAGE_SIZE))
      flags &= ~BAT_PHYSICAL_BIT;

    bat_table[e_address] = p_address | flags;
  }
}

void DBATUpdated()
{
  dbat_table = {};
  UpdateBATs(dbat_table, SPR_DBAT0U);
  bool extended_bats = SConfig::GetInstance().bWii && HID4.SBE;
  if (extended_bats)
    UpdateBATs(dbat_table, SPR_DBAT4U);
  if (Memory::m_pFakeVMEM)
  {
    // In Fake-MMU mode, insert some extra entries into the BAT tables.
    UpdateFakeMMUBat(dbat_table, 0x40000000);
    UpdateFakeMMUBat(dbat_table, 0x70000000);
  }

#ifndef _ARCH_32
  Memory::UpdateLogicalMemory(dbat_table);
#endif

  // IsOptimizable*Address and dcbz depends on the BAT mapping, so we need a flush here.
  JitInterface::ClearSafe();
}

void IBATUpdated()
{
  ibat_table = {};
  UpdateBATs(ibat_table, SPR_IBAT0U);
  bool extended_bats = SConfig::GetInstance().bWii && HID4.SBE;
  if (extended_bats)
    UpdateBATs(ibat_table, SPR_IBAT4U);
  if (Memory::m_pFakeVMEM)
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
  if (TranslateBatAddess(IsOpcodeFlag(flag) ? ibat_table : dbat_table, &address))
    return TranslateAddressResult{TranslateAddressResult::BAT_TRANSLATED, address};

  return TranslatePageAddress(address, flag);
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
