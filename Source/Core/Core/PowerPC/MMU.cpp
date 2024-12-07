// Copyright 2003 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Some of the code in this file was originally based on PearPC, though it has been modified since.
// We have been given permission by the author to re-license the code under GPLv2+.
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

#include "Core/PowerPC/MMU.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>
#include <string>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/GDBStub.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

#include "VideoCommon/VideoBackendBase.h"

namespace PowerPC
{
MMU::MMU(Core::System& system, Memory::MemoryManager& memory, PowerPC::PowerPCManager& power_pc)
    : m_system(system), m_memory(memory), m_power_pc(power_pc), m_ppc_state(power_pc.GetPPCState())
{
}

MMU::~MMU() = default;

// Overloaded byteswap functions, for use within the templated functions below.
[[maybe_unused]] static u8 bswap(u8 val)
{
  return val;
}
[[maybe_unused]] static s8 bswap(s8 val)
{
  return val;
}
[[maybe_unused]] static u16 bswap(u16 val)
{
  return Common::swap16(val);
}
[[maybe_unused]] static s16 bswap(s16 val)
{
  return Common::swap16(val);
}
[[maybe_unused]] static u32 bswap(u32 val)
{
  return Common::swap32(val);
}
[[maybe_unused]] static u64 bswap(u64 val)
{
  return Common::swap64(val);
}

static constexpr bool IsOpcodeFlag(XCheckTLBFlag flag)
{
  return flag == XCheckTLBFlag::Opcode || flag == XCheckTLBFlag::OpcodeNoException;
}

static constexpr bool IsNoExceptionFlag(XCheckTLBFlag flag)
{
  return flag == XCheckTLBFlag::NoException || flag == XCheckTLBFlag::OpcodeNoException;
}

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

template <XCheckTLBFlag flag, typename T, bool never_translate>
T MMU::ReadFromHardware(const u32 effective_address, const UGeckoInstruction inst)
{
  // ReadFromHardware is currently used with XCheckTLBFlag::OpcodeNoException by host instruction
  // functions. Actual instruction decoding (which can raise exceptions and uses icache) is handled
  // by TryReadInstruction.
  static_assert(flag == XCheckTLBFlag::NoException || flag == XCheckTLBFlag::Read ||
                flag == XCheckTLBFlag::OpcodeNoException);

  u32 physical_address;
  bool wi;

  if (!never_translate &&
      (IsOpcodeFlag(flag) ? m_ppc_state.msr.IR.Value() : m_ppc_state.msr.DR.Value()))
  {
    auto translated_addr = TranslateAddress<flag>(effective_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Read)
        GenerateDSIException(effective_address, false);
      return 0;
    }
    physical_address = translated_addr.address;
    wi = translated_addr.wi;
  }
  else
  {
    physical_address = effective_address;
    wi = false;
  }

  if (flag == XCheckTLBFlag::Read &&
      AccessCausesAlignmentException(effective_address, sizeof(T) << 3, inst, wi))
  {
    GenerateAlignmentException(m_ppc_state, effective_address, inst);
    return 0;
  }

  const u32 effective_start_page = effective_address & ~HW_PAGE_MASK;
  const u32 effective_end_page = (effective_address + sizeof(T) - 1) & ~HW_PAGE_MASK;
  if (effective_start_page != effective_end_page)
  {
    // This could be unaligned down to the byte level... hopefully this is rare, so doing it this
    // way isn't too terrible.
    u64 var = 0;
    for (u32 i = 0; i < sizeof(T); ++i)
    {
      var = (var << 8) | ReadFromHardware<flag, u8, never_translate>(effective_address + i, inst);
    }
    return static_cast<T>(var);
  }

  if (flag == XCheckTLBFlag::Read && (physical_address & 0xF8000000) == 0x08000000)
  {
    if (physical_address < 0x0c000000)
    {
      return EFB_Read(physical_address);
    }
    else
    {
      return static_cast<T>(
          m_memory.GetMMIOMapping()->Read<std::make_unsigned_t<T>>(m_system, physical_address));
    }
  }

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if (m_memory.GetL1Cache() && (physical_address >> 28) == 0xE &&
      (physical_address < (0xE0000000 + m_memory.GetL1CacheSize())))
  {
    T value;
    std::memcpy(&value, &m_memory.GetL1Cache()[physical_address & 0x0FFFFFFF], sizeof(T));
    return bswap(value);
  }

  if (m_memory.GetRAM() && (physical_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    T value;
    physical_address &= m_memory.GetRamMask();

    if (!m_ppc_state.m_enable_dcache || wi)
    {
      std::memcpy(&value, &m_memory.GetRAM()[physical_address], sizeof(T));
    }
    else
    {
      m_ppc_state.dCache.Read(m_memory, physical_address, &value, sizeof(T),
                              HID0(m_ppc_state).DLOCK || flag != XCheckTLBFlag::Read);
    }

    return bswap(value);
  }

  if (m_memory.GetEXRAM() && (physical_address >> 28) == 0x1 &&
      (physical_address & 0x0FFFFFFF) < m_memory.GetExRamSizeReal())
  {
    T value;
    physical_address &= 0x0FFFFFFF;

    if (!m_ppc_state.m_enable_dcache || wi)
    {
      std::memcpy(&value, &m_memory.GetEXRAM()[physical_address], sizeof(T));
    }
    else
    {
      m_ppc_state.dCache.Read(m_memory, physical_address + 0x10000000, &value, sizeof(T),
                              HID0(m_ppc_state).DLOCK || flag != XCheckTLBFlag::Read);
    }

    return bswap(value);
  }

  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (m_memory.GetFakeVMEM() && ((physical_address & 0xFE000000) == 0x7E000000))
  {
    T value;
    std::memcpy(&value, &m_memory.GetFakeVMEM()[physical_address & m_memory.GetFakeVMemMask()],
                sizeof(T));
    return bswap(value);
  }

  PanicAlertFmt("Unable to resolve read address {:x} PC {:x}", physical_address, m_ppc_state.pc);
  if (m_system.IsPauseOnPanicMode())
  {
    m_system.GetCPU().Break();
    m_ppc_state.Exceptions |= EXCEPTION_FAKE_MEMCHECK_HIT;
  }
  return 0;
}

template <XCheckTLBFlag flag, bool never_translate>
void MMU::WriteToHardware(const u32 effective_address, const u32 data, const u32 size,
                          const UGeckoInstruction inst)
{
  static_assert(flag == XCheckTLBFlag::NoException || flag == XCheckTLBFlag::Write);

  DEBUG_ASSERT(size <= 4);

  u32 physical_address;
  bool wi;

  if (!never_translate && m_ppc_state.msr.DR)
  {
    auto translated_addr = TranslateAddress<flag>(effective_address);
    if (!translated_addr.Success())
    {
      if (flag == XCheckTLBFlag::Write)
        GenerateDSIException(effective_address, true);
      return;
    }

    physical_address = translated_addr.address;
    wi = translated_addr.wi;
  }
  else
  {
    physical_address = effective_address;
    wi = false;
  }

  if (flag == XCheckTLBFlag::Write &&
      AccessCausesAlignmentException(effective_address, size << 3, inst, wi))
  {
    GenerateAlignmentException(m_ppc_state, effective_address, inst);
    return;
  }

  const u32 effective_start_page = effective_address & ~HW_PAGE_MASK;
  const u32 effective_end_page = (effective_address + size - 1) & ~HW_PAGE_MASK;
  if (effective_start_page != effective_end_page)
  {
    // The write crosses a page boundary. Break it up into two writes.
    const u32 first_half_size = effective_end_page - effective_address;
    const u32 second_half_size = size - first_half_size;
    WriteToHardware<flag, never_translate>(effective_address, std::rotr(data, second_half_size * 8),
                                           first_half_size, inst);
    WriteToHardware<flag, never_translate>(effective_end_page, data, second_half_size, inst);
    return;
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
      (physical_address & 0xFFFFF000) == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS)
  {
    switch (size)
    {
    case 1:
      m_system.GetGPFifo().Write8(static_cast<u8>(data));
      return;
    case 2:
      m_system.GetGPFifo().Write16(static_cast<u16>(data));
      return;
    case 4:
      m_system.GetGPFifo().Write32(data);
      return;
    default:
      // Some kind of misaligned write. TODO: Does this match how the actual hardware handles it?
      auto& gpfifo = m_system.GetGPFifo();
      for (size_t i = size * 8; i > 0;)
      {
        i -= 8;
        gpfifo.Write8(static_cast<u8>(data >> i));
      }
      return;
    }
  }

  if (flag == XCheckTLBFlag::Write && (physical_address & 0xF8000000) == 0x08000000)
  {
    if (physical_address < 0x0c000000)
    {
      EFB_Write(data, physical_address);
      return;
    }

    switch (size)
    {
    case 1:
      m_memory.GetMMIOMapping()->Write<u8>(m_system, physical_address, static_cast<u8>(data));
      return;
    case 2:
      m_memory.GetMMIOMapping()->Write<u16>(m_system, physical_address, static_cast<u16>(data));
      return;
    case 4:
      m_memory.GetMMIOMapping()->Write<u32>(m_system, physical_address, data);
      return;
    default:
      // Some kind of misaligned write. TODO: Does this match how the actual hardware handles it?
      for (size_t i = size * 8; i > 0; physical_address++)
      {
        i -= 8;
        m_memory.GetMMIOMapping()->Write<u8>(m_system, physical_address,
                                             static_cast<u8>(data >> i));
      }
      return;
    }
  }

  const u32 swapped_data = Common::swap32(std::rotr(data, size * 8));

  // Locked L1 technically doesn't have a fixed address, but games all use 0xE0000000.
  if (m_memory.GetL1Cache() && (physical_address >> 28 == 0xE) &&
      (physical_address < (0xE0000000 + m_memory.GetL1CacheSize())))
  {
    std::memcpy(&m_memory.GetL1Cache()[physical_address & 0x0FFFFFFF], &swapped_data, size);
    return;
  }

  if (wi && (size < 4 || (physical_address & 0x3)))
  {
    // When a write to memory is performed in hardware, 64 bits of data are sent to the memory
    // controller along with a mask. This mask is encoded using just two bits of data - one for
    // the upper 32 bits and one for the lower 32 bits - which leads to some odd data duplication
    // behavior for write-through/cache-inhibited writes with a start address or end address that
    // isn't 32-bit aligned. See https://bugs.dolphin-emu.org/issues/12565 for details.

    // TODO: This interrupt is supposed to have associated cause and address registers
    // TODO: This should trigger the hwtest's interrupt handling, but it does not seem to
    //       (https://github.com/dolphin-emu/hwtests/pull/42)
    m_system.GetProcessorInterface().SetInterrupt(ProcessorInterface::INT_CAUSE_PI);

    const u32 rotated_data = std::rotr(data, ((physical_address & 0x3) + size) * 8);

    const u32 start_addr = Common::AlignDown(physical_address, 8);
    const u32 end_addr = Common::AlignUp(physical_address + size, 8);
    for (u32 addr = start_addr; addr != end_addr; addr += 8)
    {
      WriteToHardware<flag, true>(addr, rotated_data, 4, inst);
      WriteToHardware<flag, true>(addr + 4, rotated_data, 4, inst);
    }

    return;
  }

  if (m_memory.GetRAM() && (physical_address & 0xF8000000) == 0x00000000)
  {
    // Handle RAM; the masking intentionally discards bits (essentially creating
    // mirrors of memory).
    physical_address &= m_memory.GetRamMask();

    if (m_ppc_state.m_enable_dcache && !wi)
    {
      m_ppc_state.dCache.Write(m_memory, physical_address, &swapped_data, size,
                               HID0(m_ppc_state).DLOCK);
    }

    if (!m_ppc_state.m_enable_dcache || wi || flag != XCheckTLBFlag::Write)
      std::memcpy(&m_memory.GetRAM()[physical_address], &swapped_data, size);

    return;
  }

  if (m_memory.GetEXRAM() && (physical_address >> 28) == 0x1 &&
      (physical_address & 0x0FFFFFFF) < m_memory.GetExRamSizeReal())
  {
    physical_address &= 0x0FFFFFFF;

    if (m_ppc_state.m_enable_dcache && !wi)
    {
      m_ppc_state.dCache.Write(m_memory, physical_address + 0x10000000, &swapped_data, size,
                               HID0(m_ppc_state).DLOCK);
    }

    if (!m_ppc_state.m_enable_dcache || wi || flag != XCheckTLBFlag::Write)
      std::memcpy(&m_memory.GetEXRAM()[physical_address], &swapped_data, size);

    return;
  }

  // In Fake-VMEM mode, we need to map the memory somewhere into
  // physical memory for BAT translation to work; we currently use
  // [0x7E000000, 0x80000000).
  if (m_memory.GetFakeVMEM() && ((physical_address & 0xFE000000) == 0x7E000000))
  {
    std::memcpy(&m_memory.GetFakeVMEM()[physical_address & m_memory.GetFakeVMemMask()],
                &swapped_data, size);
    return;
  }

  PanicAlertFmt("Unable to resolve write address {:x} PC {:x}", effective_address, m_ppc_state.pc);
  if (m_system.IsPauseOnPanicMode())
  {
    m_system.GetCPU().Break();
    m_ppc_state.Exceptions |= EXCEPTION_FAKE_MEMCHECK_HIT;
  }
}
// =====================

// =================================
/* These functions are primarily called by the Interpreter functions and are routed to the correct
   location through ReadFromHardware and WriteToHardware */
// ----------------

u32 MMU::Read_Opcode(u32 address)
{
  TryReadInstResult result = TryReadInstruction(address);
  if (!result.valid)
  {
    GenerateISIException(address);
    return 0;
  }
  return result.hex;
}

TryReadInstResult MMU::TryReadInstruction(u32 address)
{
  bool from_bat = true;
  if (m_ppc_state.msr.IR)
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

  u32 hex;
  // TODO: Refactor this. This icache implementation is totally wrong if used with the fake vmem.
  if (m_memory.GetFakeVMEM() && ((address & 0xFE000000) == 0x7E000000))
  {
    hex = Common::swap32(&m_memory.GetFakeVMEM()[address & m_memory.GetFakeVMemMask()]);
  }
  else
  {
    hex = m_ppc_state.iCache.ReadInstruction(m_memory, m_ppc_state, address);
  }
  return TryReadInstResult{true, from_bat, hex, address};
}

u32 MMU::HostRead_Instruction(const Core::CPUThreadGuard& guard, const u32 address)
{
  return guard.GetSystem().GetMMU().ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(
      address, UGeckoInstruction{});
}

std::optional<ReadResult<u32>> MMU::HostTryReadInstruction(const Core::CPUThreadGuard& guard,
                                                           const u32 address,
                                                           RequestedAddressSpace space)
{
  if (!HostIsInstructionRAMAddress(guard, address, space))
    return std::nullopt;

  auto& mmu = guard.GetSystem().GetMMU();
  switch (space)
  {
  case RequestedAddressSpace::Effective:
  {
    const u32 value =
        mmu.ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(address, UGeckoInstruction{});
    return ReadResult<u32>(!!mmu.m_ppc_state.msr.IR, value);
  }
  case RequestedAddressSpace::Physical:
  {
    const u32 value = mmu.ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32, true>(
        address, UGeckoInstruction{});
    return ReadResult<u32>(false, value);
  }
  case RequestedAddressSpace::Virtual:
  {
    if (!mmu.m_ppc_state.msr.IR)
      return std::nullopt;
    const u32 value =
        mmu.ReadFromHardware<XCheckTLBFlag::OpcodeNoException, u32>(address, UGeckoInstruction{});
    return ReadResult<u32>(true, value);
  }
  }

  ASSERT(false);
  return std::nullopt;
}

void MMU::Memcheck(u32 address, u64 var, bool write, size_t size)
{
  if (!m_power_pc.GetMemChecks().HasAny())
    return;

  TMemCheck* mc = m_power_pc.GetMemChecks().GetMemCheck(address, size);
  if (mc == nullptr)
    return;

  if (m_system.GetCPU().IsStepping())
  {
    // Disable when stepping so that resume works.
    return;
  }

  mc->num_hits++;

  const bool pause = mc->Action(m_system, var, address, write, size, m_ppc_state.pc);
  if (!pause)
    return;

  m_system.GetCPU().Break();

  if (GDBStub::IsActive())
    GDBStub::TakeControl();

  // Fake an exception so that all the code that tests for it in order to
  // skip the the rest of the instruction will apply.  (This means that
  // watchpoints will stop the emulator before the offending load/store,
  // not after like GDB does, but that's better anyway.  Just need to
  // make sure resuming after that works.)
  m_ppc_state.Exceptions |= EXCEPTION_FAKE_MEMCHECK_HIT;
}

u8 MMU::Read_U8(const u32 address, const UGeckoInstruction inst)
{
  u8 var = ReadFromHardware<XCheckTLBFlag::Read, u8>(address, inst);
  Memcheck(address, var, false, 1);
  return var;
}

u16 MMU::Read_U16(const u32 address, const UGeckoInstruction inst)
{
  u16 var = ReadFromHardware<XCheckTLBFlag::Read, u16>(address, inst);
  Memcheck(address, var, false, 2);
  return var;
}

u32 MMU::Read_U32(const u32 address, const UGeckoInstruction inst)
{
  u32 var = ReadFromHardware<XCheckTLBFlag::Read, u32>(address, inst);
  Memcheck(address, var, false, 4);
  return var;
}

u64 MMU::Read_U64(const u32 address, const UGeckoInstruction inst)
{
  u64 var = ReadFromHardware<XCheckTLBFlag::Read, u64>(address, inst);
  Memcheck(address, var, false, 8);
  return var;
}

template <typename T>
std::optional<ReadResult<T>> MMU::HostTryReadUX(const Core::CPUThreadGuard& guard,
                                                const u32 address, RequestedAddressSpace space)
{
  if (!HostIsRAMAddress(guard, address, space))
    return std::nullopt;

  auto& mmu = guard.GetSystem().GetMMU();
  switch (space)
  {
  case RequestedAddressSpace::Effective:
  {
    T value = mmu.ReadFromHardware<XCheckTLBFlag::NoException, T>(address, UGeckoInstruction{});
    return ReadResult<T>(!!mmu.m_ppc_state.msr.DR, std::move(value));
  }
  case RequestedAddressSpace::Physical:
  {
    T value =
        mmu.ReadFromHardware<XCheckTLBFlag::NoException, T, true>(address, UGeckoInstruction{});
    return ReadResult<T>(false, std::move(value));
  }
  case RequestedAddressSpace::Virtual:
  {
    if (!mmu.m_ppc_state.msr.DR)
      return std::nullopt;
    T value = mmu.ReadFromHardware<XCheckTLBFlag::NoException, T>(address, UGeckoInstruction{});
    return ReadResult<T>(true, std::move(value));
  }
  }

  ASSERT(false);
  return std::nullopt;
}

std::optional<ReadResult<u8>> MMU::HostTryReadU8(const Core::CPUThreadGuard& guard, u32 address,
                                                 RequestedAddressSpace space)
{
  return HostTryReadUX<u8>(guard, address, space);
}

std::optional<ReadResult<u16>> MMU::HostTryReadU16(const Core::CPUThreadGuard& guard, u32 address,
                                                   RequestedAddressSpace space)
{
  return HostTryReadUX<u16>(guard, address, space);
}

std::optional<ReadResult<u32>> MMU::HostTryReadU32(const Core::CPUThreadGuard& guard, u32 address,
                                                   RequestedAddressSpace space)
{
  return HostTryReadUX<u32>(guard, address, space);
}

std::optional<ReadResult<u64>> MMU::HostTryReadU64(const Core::CPUThreadGuard& guard, u32 address,
                                                   RequestedAddressSpace space)
{
  return HostTryReadUX<u64>(guard, address, space);
}

std::optional<ReadResult<float>> MMU::HostTryReadF32(const Core::CPUThreadGuard& guard, u32 address,
                                                     RequestedAddressSpace space)
{
  const auto result = HostTryReadUX<u32>(guard, address, space);
  if (!result)
    return std::nullopt;
  return ReadResult<float>(result->translated, std::bit_cast<float>(result->value));
}

std::optional<ReadResult<double>> MMU::HostTryReadF64(const Core::CPUThreadGuard& guard,
                                                      u32 address, RequestedAddressSpace space)
{
  const auto result = HostTryReadUX<u64>(guard, address, space);
  if (!result)
    return std::nullopt;
  return ReadResult<double>(result->translated, std::bit_cast<double>(result->value));
}

void MMU::Write_U8(const u32 var, const u32 address, const UGeckoInstruction inst)
{
  Memcheck(address, var, true, 1);
  WriteToHardware<XCheckTLBFlag::Write>(address, var, 1, inst);
}

void MMU::Write_U16(const u32 var, const u32 address, const UGeckoInstruction inst)
{
  Memcheck(address, var, true, 2);
  WriteToHardware<XCheckTLBFlag::Write>(address, var, 2, inst);
}
void MMU::Write_U16_Swap(const u32 var, const u32 address, const UGeckoInstruction inst)
{
  Write_U16((var & 0xFFFF0000) | Common::swap16(static_cast<u16>(var)), address, inst);
}

void MMU::Write_U32(const u32 var, const u32 address, const UGeckoInstruction inst)
{
  Memcheck(address, var, true, 4);
  WriteToHardware<XCheckTLBFlag::Write>(address, var, 4, inst);
}
void MMU::Write_U32_Swap(const u32 var, const u32 address, const UGeckoInstruction inst)
{
  Write_U32(Common::swap32(var), address, inst);
}

void MMU::Write_U64(const u64 var, const u32 address, const UGeckoInstruction inst)
{
  Memcheck(address, var, true, 8);
  WriteToHardware<XCheckTLBFlag::Write>(address, static_cast<u32>(var >> 32), 4, inst);
  WriteToHardware<XCheckTLBFlag::Write>(address + sizeof(u32), static_cast<u32>(var), 4, inst);
}
void MMU::Write_U64_Swap(const u64 var, const u32 address, const UGeckoInstruction inst)
{
  Write_U64(Common::swap64(var), address, inst);
}

u8 MMU::HostRead_U8(const Core::CPUThreadGuard& guard, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  return mmu.ReadFromHardware<XCheckTLBFlag::NoException, u8>(address, UGeckoInstruction{});
}

u16 MMU::HostRead_U16(const Core::CPUThreadGuard& guard, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  return mmu.ReadFromHardware<XCheckTLBFlag::NoException, u16>(address, UGeckoInstruction{});
}

u32 MMU::HostRead_U32(const Core::CPUThreadGuard& guard, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  return mmu.ReadFromHardware<XCheckTLBFlag::NoException, u32>(address, UGeckoInstruction{});
}

u64 MMU::HostRead_U64(const Core::CPUThreadGuard& guard, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  return mmu.ReadFromHardware<XCheckTLBFlag::NoException, u64>(address, UGeckoInstruction{});
}

float MMU::HostRead_F32(const Core::CPUThreadGuard& guard, const u32 address)
{
  const u32 integral = HostRead_U32(guard, address);

  return std::bit_cast<float>(integral);
}

double MMU::HostRead_F64(const Core::CPUThreadGuard& guard, const u32 address)
{
  const u64 integral = HostRead_U64(guard, address);

  return std::bit_cast<double>(integral);
}

void MMU::HostWrite_U8(const Core::CPUThreadGuard& guard, const u32 var, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, var, 1, UGeckoInstruction{});
}

void MMU::HostWrite_U16(const Core::CPUThreadGuard& guard, const u32 var, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, var, 2, UGeckoInstruction{});
}

void MMU::HostWrite_U32(const Core::CPUThreadGuard& guard, const u32 var, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, var, 4, UGeckoInstruction{});
}

void MMU::HostWrite_U64(const Core::CPUThreadGuard& guard, const u64 var, const u32 address)
{
  auto& mmu = guard.GetSystem().GetMMU();
  mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, static_cast<u32>(var >> 32), 4,
                                                  UGeckoInstruction{});
  mmu.WriteToHardware<XCheckTLBFlag::NoException>(address + sizeof(u32), static_cast<u32>(var), 4,
                                                  UGeckoInstruction{});
}

void MMU::HostWrite_F32(const Core::CPUThreadGuard& guard, const float var, const u32 address)
{
  const u32 integral = std::bit_cast<u32>(var);

  HostWrite_U32(guard, integral, address);
}

void MMU::HostWrite_F64(const Core::CPUThreadGuard& guard, const double var, const u32 address)
{
  const u64 integral = std::bit_cast<u64>(var);

  HostWrite_U64(guard, integral, address);
}

std::optional<WriteResult> MMU::HostTryWriteUX(const Core::CPUThreadGuard& guard, const u32 var,
                                               const u32 address, const u32 size,
                                               RequestedAddressSpace space)
{
  if (!HostIsRAMAddress(guard, address, space))
    return std::nullopt;

  auto& mmu = guard.GetSystem().GetMMU();
  switch (space)
  {
  case RequestedAddressSpace::Effective:
    mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, var, size, UGeckoInstruction{});
    return WriteResult(!!mmu.m_ppc_state.msr.DR);
  case RequestedAddressSpace::Physical:
    mmu.WriteToHardware<XCheckTLBFlag::NoException, true>(address, var, size, UGeckoInstruction{});
    return WriteResult(false);
  case RequestedAddressSpace::Virtual:
    if (!mmu.m_ppc_state.msr.DR)
      return std::nullopt;
    mmu.WriteToHardware<XCheckTLBFlag::NoException>(address, var, size, UGeckoInstruction{});
    return WriteResult(true);
  }

  ASSERT(false);
  return std::nullopt;
}

std::optional<WriteResult> MMU::HostTryWriteU8(const Core::CPUThreadGuard& guard, const u32 var,
                                               const u32 address, RequestedAddressSpace space)
{
  return HostTryWriteUX(guard, var, address, 1, space);
}

std::optional<WriteResult> MMU::HostTryWriteU16(const Core::CPUThreadGuard& guard, const u32 var,
                                                const u32 address, RequestedAddressSpace space)
{
  return HostTryWriteUX(guard, var, address, 2, space);
}

std::optional<WriteResult> MMU::HostTryWriteU32(const Core::CPUThreadGuard& guard, const u32 var,
                                                const u32 address, RequestedAddressSpace space)
{
  return HostTryWriteUX(guard, var, address, 4, space);
}

std::optional<WriteResult> MMU::HostTryWriteU64(const Core::CPUThreadGuard& guard, const u64 var,
                                                const u32 address, RequestedAddressSpace space)
{
  const auto result = HostTryWriteUX(guard, static_cast<u32>(var >> 32), address, 4, space);
  if (!result)
    return result;

  return HostTryWriteUX(guard, static_cast<u32>(var), address + 4, 4, space);
}

std::optional<WriteResult> MMU::HostTryWriteF32(const Core::CPUThreadGuard& guard, const float var,
                                                const u32 address, RequestedAddressSpace space)
{
  const u32 integral = std::bit_cast<u32>(var);
  return HostTryWriteU32(guard, integral, address, space);
}

std::optional<WriteResult> MMU::HostTryWriteF64(const Core::CPUThreadGuard& guard, const double var,
                                                const u32 address, RequestedAddressSpace space)
{
  const u64 integral = std::bit_cast<u64>(var);
  return HostTryWriteU64(guard, integral, address, space);
}

std::string MMU::HostGetString(const Core::CPUThreadGuard& guard, u32 address, size_t size)
{
  std::string s;
  do
  {
    if (!HostIsRAMAddress(guard, address))
      break;
    u8 res = HostRead_U8(guard, address);
    if (!res)
      break;
    s += static_cast<char>(res);
    ++address;
  } while (size == 0 || s.length() < size);
  return s;
}

std::u16string MMU::HostGetU16String(const Core::CPUThreadGuard& guard, u32 address, size_t size)
{
  std::u16string s;
  do
  {
    if (!HostIsRAMAddress(guard, address) || !HostIsRAMAddress(guard, address + 1))
      break;
    const u16 res = HostRead_U16(guard, address);
    if (!res)
      break;
    s += static_cast<char16_t>(res);
    address += 2;
  } while (size == 0 || s.length() < size);
  return s;
}

std::optional<ReadResult<std::string>> MMU::HostTryReadString(const Core::CPUThreadGuard& guard,
                                                              u32 address, size_t size,
                                                              RequestedAddressSpace space)
{
  auto c = HostTryReadU8(guard, address, space);
  if (!c)
    return std::nullopt;
  if (c->value == 0)
    return ReadResult<std::string>(c->translated, "");

  std::string s;
  s += static_cast<char>(c->value);
  while (size == 0 || s.length() < size)
  {
    ++address;
    const auto res = HostTryReadU8(guard, address, space);
    if (!res || res->value == 0)
      break;
    s += static_cast<char>(res->value);
  }
  return ReadResult<std::string>(c->translated, std::move(s));
}

bool MMU::IsOptimizableRAMAddress(const u32 address, const u32 access_size,
                                  const UGeckoInstruction inst) const
{
  if (m_power_pc.GetMemChecks().HasAny())
    return false;

  if (!m_ppc_state.msr.DR)
    return false;

  if (m_ppc_state.m_enable_dcache)
    return false;

  if ((address & GetAlignmentMask(access_size)) != 0 &&
      AccessCausesAlignmentExceptionIfMisaligned(inst, access_size))
  {
    return false;
  }

  // We store whether an access can be optimized to an unchecked access
  // in dbat_table.
  const u32 last_byte_address = address + (access_size >> 3) - 1;
  const u32 bat_result_1 = m_dbat_table[address >> BAT_INDEX_SHIFT];
  const u32 bat_result_2 = m_dbat_table[last_byte_address >> BAT_INDEX_SHIFT];
  return (bat_result_1 & bat_result_2 & BAT_PHYSICAL_BIT) != 0;
}

template <XCheckTLBFlag flag>
bool MMU::IsRAMAddress(u32 address, bool translate)
{
  if (translate)
  {
    auto translate_address = TranslateAddress<flag>(address);
    if (!translate_address.Success())
      return false;
    address = translate_address.address;
  }

  u32 segment = address >> 28;
  if (m_memory.GetRAM() && segment == 0x0 && (address & 0x0FFFFFFF) < m_memory.GetRamSizeReal())
  {
    return true;
  }
  else if (m_memory.GetEXRAM() && segment == 0x1 &&
           (address & 0x0FFFFFFF) < m_memory.GetExRamSizeReal())
  {
    return true;
  }
  else if (m_memory.GetFakeVMEM() && ((address & 0xFE000000) == 0x7E000000))
  {
    return true;
  }
  else if (m_memory.GetL1Cache() && segment == 0xE &&
           (address < (0xE0000000 + m_memory.GetL1CacheSize())))
  {
    return true;
  }
  return false;
}

bool MMU::HostIsRAMAddress(const Core::CPUThreadGuard& guard, u32 address,
                           RequestedAddressSpace space)
{
  auto& mmu = guard.GetSystem().GetMMU();
  switch (space)
  {
  case RequestedAddressSpace::Effective:
    return mmu.IsRAMAddress<XCheckTLBFlag::NoException>(address, mmu.m_ppc_state.msr.DR);
  case RequestedAddressSpace::Physical:
    return mmu.IsRAMAddress<XCheckTLBFlag::NoException>(address, false);
  case RequestedAddressSpace::Virtual:
    if (!mmu.m_ppc_state.msr.DR)
      return false;
    return mmu.IsRAMAddress<XCheckTLBFlag::NoException>(address, true);
  }

  ASSERT(false);
  return false;
}

bool MMU::HostIsInstructionRAMAddress(const Core::CPUThreadGuard& guard, u32 address,
                                      RequestedAddressSpace space)
{
  // Instructions are always 32bit aligned.
  if (address & 3)
    return false;

  auto& mmu = guard.GetSystem().GetMMU();
  switch (space)
  {
  case RequestedAddressSpace::Effective:
    return mmu.IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(address, mmu.m_ppc_state.msr.IR);
  case RequestedAddressSpace::Physical:
    return mmu.IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(address, false);
  case RequestedAddressSpace::Virtual:
    if (!mmu.m_ppc_state.msr.IR)
      return false;
    return mmu.IsRAMAddress<XCheckTLBFlag::OpcodeNoException>(address, true);
  }

  ASSERT(false);
  return false;
}

void MMU::DMA_LCToMemory(const u32 mem_address, const u32 cache_address, const u32 num_blocks)
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
      const u32 data = Common::swap32(m_memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF));
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
      const u32 data = Common::swap32(m_memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF));
      m_memory.GetMMIOMapping()->Write(m_system, mem_address + i, data);
    }
    return;
  }

  const u8* src = m_memory.GetL1Cache() + (cache_address & 0x3FFFF);
  m_memory.CopyToEmu(mem_address, src, 32 * num_blocks);
}

void MMU::DMA_MemoryToLC(const u32 cache_address, const u32 mem_address, const u32 num_blocks)
{
  // No known game uses this; here for completeness.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x08000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data = Common::swap32(EFB_Read(mem_address + i));
      std::memcpy(m_memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
    }
    return;
  }

  // No known game uses this.
  // TODO: Refactor.
  if ((mem_address & 0x0F000000) == 0x0C000000)
  {
    for (u32 i = 0; i < 32 * num_blocks; i += 4)
    {
      const u32 data =
          Common::swap32(m_memory.GetMMIOMapping()->Read<u32>(m_system, mem_address + i));
      std::memcpy(m_memory.GetL1Cache() + ((cache_address + i) & 0x3FFFF), &data, sizeof(u32));
    }
    return;
  }

  u8* dst = m_memory.GetL1Cache() + (cache_address & 0x3FFFF);
  m_memory.CopyFromEmu(dst, mem_address, 32 * num_blocks);
}

static bool TranslateBatAddress(const BatTable& bat_table, u32* address, bool* wi)
{
  u32 bat_result = bat_table[*address >> BAT_INDEX_SHIFT];
  if ((bat_result & BAT_MAPPED_BIT) == 0)
    return false;
  *address = (bat_result & BAT_RESULT_MASK) | (*address & (BAT_PAGE_SIZE - 1));
  *wi = (bat_result & BAT_WI_BIT) != 0;
  return true;
}

void MMU::ClearDCacheLine(u32 address, UGeckoInstruction inst)
{
  DEBUG_ASSERT((address & 0x1F) == 0);
  if (m_ppc_state.msr.DR)
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

  // TODO: This isn't precisely correct for non-RAM regions, but the difference
  // is unlikely to matter.
  for (u32 i = 0; i < 32; i += 4)
    WriteToHardware<XCheckTLBFlag::Write, true>(address + i, 0, 4, inst);
}

void MMU::StoreDCacheLine(u32 address)
{
  address &= ~0x1F;

  if (m_ppc_state.msr.DR)
  {
    auto translated_address = TranslateAddress<XCheckTLBFlag::Write>(address);
    if (translated_address.result == TranslateAddressResultEnum::DIRECT_STORE_SEGMENT)
    {
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

  if (m_ppc_state.m_enable_dcache)
    m_ppc_state.dCache.Store(m_memory, address);
}

void MMU::InvalidateDCacheLine(u32 address)
{
  address &= ~0x1F;

  if (m_ppc_state.msr.DR)
  {
    auto translated_address = TranslateAddress<XCheckTLBFlag::Write>(address);
    if (translated_address.result == TranslateAddressResultEnum::DIRECT_STORE_SEGMENT)
    {
      return;
    }
    if (translated_address.result == TranslateAddressResultEnum::PAGE_FAULT)
    {
      return;
    }
    address = translated_address.address;
  }

  if (m_ppc_state.m_enable_dcache)
    m_ppc_state.dCache.Invalidate(m_memory, address);
}

void MMU::FlushDCacheLine(u32 address)
{
  address &= ~0x1F;

  if (m_ppc_state.msr.DR)
  {
    auto translated_address = TranslateAddress<XCheckTLBFlag::Write>(address);
    if (translated_address.result == TranslateAddressResultEnum::DIRECT_STORE_SEGMENT)
    {
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

  if (m_ppc_state.m_enable_dcache)
    m_ppc_state.dCache.Flush(m_memory, address);
}

void MMU::TouchDCacheLine(u32 address, bool store)
{
  address &= ~0x1F;

  if (m_ppc_state.msr.DR)
  {
    auto translated_address = TranslateAddress<XCheckTLBFlag::Write>(address);
    if (translated_address.result == TranslateAddressResultEnum::DIRECT_STORE_SEGMENT)
    {
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

  if (m_ppc_state.m_enable_dcache)
    m_ppc_state.dCache.Touch(m_memory, address, store);
}

u32 MMU::IsOptimizableMMIOAccess(u32 address, u32 access_size) const
{
  if (m_power_pc.GetMemChecks().HasAny())
    return 0;

  if (!m_ppc_state.msr.DR)
    return 0;

  if (m_ppc_state.m_enable_dcache)
    return 0;

  // Translate address
  // If we also optimize for TLB mappings, we'd have to clear the
  // JitCache on each TLB invalidation.
  bool wi = false;
  if (!TranslateBatAddress(m_dbat_table, &address, &wi))
    return 0;

  // Check whether the address is an aligned address of an MMIO register.
  const bool aligned = (address & GetAlignmentMask(access_size)) == 0;
  if (!aligned || !MMIO::IsMMIOAddress(address, m_system.IsWii()))
    return 0;

  return address;
}

bool MMU::IsOptimizableGatherPipeWrite(u32 address) const
{
  if (m_power_pc.GetMemChecks().HasAny())
    return false;

  if (!m_ppc_state.msr.DR)
    return false;

  // Translate address, only check BAT mapping.
  // If we also optimize for TLB mappings, we'd have to clear the
  // JitCache on each TLB invalidation.
  bool wi = false;
  if (!TranslateBatAddress(m_dbat_table, &address, &wi))
    return false;

  // Check whether the translated address equals the address in WPAR.
  return address == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS;
}

TranslateResult MMU::JitCache_TranslateAddress(u32 address)
{
  if (!m_ppc_state.msr.IR)
    return TranslateResult{address};

  // TODO: We shouldn't use FLAG_OPCODE if the caller is the debugger.
  const auto tlb_addr = TranslateAddress<XCheckTLBFlag::Opcode>(address);
  if (!tlb_addr.Success())
    return TranslateResult{};

  const bool from_bat = tlb_addr.result == TranslateAddressResultEnum::BAT_TRANSLATED;
  return TranslateResult{from_bat, tlb_addr.address};
}

void MMU::GenerateDSIException(u32 effective_address, bool write)
{
  // DSI exceptions are only supported in MMU mode.
  if (!m_system.IsMMUMode())
  {
    PanicAlertFmt("Invalid {} {:#010x}, PC = {:#010x}", write ? "write to" : "read from",
                  effective_address, m_ppc_state.pc);
    if (m_system.IsPauseOnPanicMode())
    {
      m_system.GetCPU().Break();
      m_ppc_state.Exceptions |= EXCEPTION_FAKE_MEMCHECK_HIT;
    }
    return;
  }

  constexpr u32 dsisr_page = 1U << 30;
  constexpr u32 dsisr_store = 1U << 25;

  if (write)
    m_ppc_state.spr[SPR_DSISR] = dsisr_page | dsisr_store;
  else
    m_ppc_state.spr[SPR_DSISR] = dsisr_page;

  m_ppc_state.spr[SPR_DAR] = effective_address;

  m_ppc_state.Exceptions |= EXCEPTION_DSI;
}

void MMU::GenerateISIException(u32 effective_address)
{
  // Address of instruction could not be translated
  m_ppc_state.npc = effective_address;

  m_ppc_state.Exceptions |= EXCEPTION_ISI;
  WARN_LOG_FMT(POWERPC, "ISI exception at {:#010x}", m_ppc_state.pc);
}

void MMU::SDRUpdated()
{
  const auto sdr = UReg_SDR1{m_ppc_state.spr[SPR_SDR]};
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

  m_ppc_state.pagetable_base = htaborg << 16;
  m_ppc_state.pagetable_hashmask = ((htabmask << 10) | 0x3ff);
}

enum class TLBLookupResult
{
  Found,
  NotFound,
  UpdateC
};

static TLBLookupResult LookupTLBPageAddress(PowerPC::PowerPCState& ppc_state,
                                            const XCheckTLBFlag flag, const u32 vpa, const u32 vsid,
                                            u32* paddr, bool* wi)
{
  const u32 tag = vpa >> HW_PAGE_INDEX_SHIFT;
  const size_t tlb_index = IsOpcodeFlag(flag) ? PowerPC::INST_TLB_INDEX : PowerPC::DATA_TLB_INDEX;
  TLBEntry& tlbe = ppc_state.tlb[tlb_index][tag & HW_PAGE_INDEX_MASK];

  if (tlbe.tag[0] == tag && tlbe.vsid[0] == vsid)
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
  if (tlbe.tag[1] == tag && tlbe.vsid[1] == vsid)
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

static void UpdateTLBEntry(PowerPC::PowerPCState& ppc_state, const XCheckTLBFlag flag, UPTE_Hi pte2,
                           const u32 address, const u32 vsid)
{
  if (IsNoExceptionFlag(flag))
    return;

  const u32 tag = address >> HW_PAGE_INDEX_SHIFT;
  const size_t tlb_index = IsOpcodeFlag(flag) ? PowerPC::INST_TLB_INDEX : PowerPC::DATA_TLB_INDEX;
  TLBEntry& tlbe = ppc_state.tlb[tlb_index][tag & HW_PAGE_INDEX_MASK];
  const u32 index = tlbe.recent == 0 && tlbe.tag[0] != TLBEntry::INVALID_TAG;
  tlbe.recent = index;
  tlbe.paddr[index] = pte2.RPN << HW_PAGE_INDEX_SHIFT;
  tlbe.pte[index] = pte2.Hex;
  tlbe.tag[index] = tag;
  tlbe.vsid[index] = vsid;
}

void MMU::InvalidateTLBEntry(u32 address)
{
  const u32 entry_index = (address >> HW_PAGE_INDEX_SHIFT) & HW_PAGE_INDEX_MASK;

  m_ppc_state.tlb[PowerPC::DATA_TLB_INDEX][entry_index].Invalidate();
  m_ppc_state.tlb[PowerPC::INST_TLB_INDEX][entry_index].Invalidate();
}

// Page Address Translation
template <const XCheckTLBFlag flag>
MMU::TranslateAddressResult MMU::TranslatePageAddress(const EffectiveAddress address, bool* wi)
{
  const auto sr = UReg_SR{m_ppc_state.sr[address.SR]};
  const u32 VSID = sr.VSID;  // 24 bit

  // TLB cache
  // This catches 99%+ of lookups in practice, so the actual page table entry code below doesn't
  // benefit much from optimization.
  u32 translated_address = 0;
  const TLBLookupResult res =
      LookupTLBPageAddress(m_ppc_state, flag, address.Hex, VSID, &translated_address, wi);
  if (res == TLBLookupResult::Found)
  {
    return TranslateAddressResult{TranslateAddressResultEnum::PAGE_TABLE_TRANSLATED,
                                  translated_address};
  }

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
  const u32 api = address.API;                //  6 bit (part of page_index)

  // hash function no 1 "xor" .360
  u32 hash = (VSID ^ page_index);

  UPTE_Lo pte1;
  pte1.VSID = VSID;
  pte1.API = api;
  pte1.V = 1;

  for (int hash_func = 0; hash_func < 2; hash_func++)
  {
    // hash function no 2 "not" .360
    if (hash_func == 1)
    {
      hash = ~hash;
      pte1.H = 1;
    }

    u32 pteg_addr = ((hash & m_ppc_state.pagetable_hashmask) << 6) | m_ppc_state.pagetable_base;

    for (int i = 0; i < 8; i++, pteg_addr += 8)
    {
      constexpr XCheckTLBFlag pte_read_flag =
          IsNoExceptionFlag(flag) ? XCheckTLBFlag::NoException : XCheckTLBFlag::Read;
      const u32 pteg = ReadFromHardware<pte_read_flag, u32, true>(pteg_addr, UGeckoInstruction{});

      if (pte1.Hex == pteg)
      {
        UPTE_Hi pte2(
            ReadFromHardware<pte_read_flag, u32, true>(pteg_addr + 4, UGeckoInstruction{}));

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
          m_memory.Write_U32(pte2.Hex, pteg_addr + 4);
        }

        // We already updated the TLB entry if this was caused by a C bit.
        if (res != TLBLookupResult::UpdateC)
          UpdateTLBEntry(m_ppc_state, flag, pte2, address.Hex, VSID);

        *wi = (pte2.WIMG & 0b1100) != 0;

        return TranslateAddressResult{TranslateAddressResultEnum::PAGE_TABLE_TRANSLATED,
                                      (pte2.RPN << 12) | offset};
      }
    }
  }
  return TranslateAddressResult{TranslateAddressResultEnum::PAGE_FAULT, 0};
}

void MMU::UpdateBATs(BatTable& bat_table, u32 base_spr)
{
  // TODO: Separate BATs for MSR.PR==0 and MSR.PR==1
  // TODO: Handle PP settings.
  // TODO: Check how hardware reacts to overlapping BATs (including
  // BATs which should cause a DSI).
  // TODO: Check how hardware reacts to invalid BATs (bad mask etc).
  for (int i = 0; i < 4; ++i)
  {
    const u32 spr = base_spr + i * 2;
    const UReg_BAT_Up batu{m_ppc_state.spr[spr]};
    const UReg_BAT_Lo batl{m_ppc_state.spr[spr + 1]};
    if (batu.VS == 0 && batu.VP == 0)
      continue;

    if ((batu.BEPI & batu.BL) != 0)
    {
      // With a valid BAT, the simplest way to match is
      // (input & ~BL_mask) == BEPI. For now, assume it's
      // implemented this way for invalid BATs as well.
      WARN_LOG_FMT(POWERPC, "Bad BAT setup: BEPI overlaps BL");
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
        // that can't be correctly emulated by fast accesses, so we don't map uncached memory.
        // (No normal games are known to rely on the quirks, though.)
        if (!wi)
        {
          if (m_memory.GetFakeVMEM() && (physical_address & 0xFE000000) == 0x7E000000)
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (physical_address < m_memory.GetRamSizeReal())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (m_memory.GetEXRAM() && physical_address >> 28 == 0x1 &&
                   (physical_address & 0x0FFFFFFF) < m_memory.GetExRamSizeReal())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
          else if (physical_address >> 28 == 0xE &&
                   physical_address < 0xE0000000 + m_memory.GetL1CacheSize())
          {
            valid_bit |= BAT_PHYSICAL_BIT;
          }
        }

        // Fast accesses don't support memchecks, so force slow accesses by removing fastmem
        // mappings for all overlapping virtual pages.
        if (m_power_pc.GetMemChecks().OverlapsMemcheck(virtual_address, BAT_PAGE_SIZE))
          valid_bit &= ~BAT_PHYSICAL_BIT;

        // (BEPI | j) == (BEPI & ~BL) | (j & BL).
        bat_table[virtual_address >> BAT_INDEX_SHIFT] = physical_address | valid_bit;
      }
    }
  }
}

void MMU::UpdateFakeMMUBat(BatTable& bat_table, u32 start_addr)
{
  for (u32 i = 0; i < (0x10000000 >> BAT_INDEX_SHIFT); ++i)
  {
    // Map from 0x4XXXXXXX or 0x7XXXXXXX to the range
    // [0x7E000000,0x80000000).
    u32 e_address = i + (start_addr >> BAT_INDEX_SHIFT);
    u32 p_address = 0x7E000000 | (i << BAT_INDEX_SHIFT & m_memory.GetFakeVMemMask());
    u32 flags = BAT_MAPPED_BIT | BAT_PHYSICAL_BIT;

    if (m_power_pc.GetMemChecks().OverlapsMemcheck(e_address << BAT_INDEX_SHIFT, BAT_PAGE_SIZE))
      flags &= ~BAT_PHYSICAL_BIT;

    bat_table[e_address] = p_address | flags;
  }
}

void MMU::DBATUpdated()
{
  m_dbat_table = {};
  UpdateBATs(m_dbat_table, SPR_DBAT0U);
  bool extended_bats = m_system.IsWii() && HID4(m_ppc_state).SBE;
  if (extended_bats)
    UpdateBATs(m_dbat_table, SPR_DBAT4U);
  if (m_memory.GetFakeVMEM())
  {
    // In Fake-MMU mode, insert some extra entries into the BAT tables.
    UpdateFakeMMUBat(m_dbat_table, 0x40000000);
    UpdateFakeMMUBat(m_dbat_table, 0x70000000);
  }

#ifndef _ARCH_32
  m_memory.UpdateLogicalMemory(m_dbat_table);
#endif

  // IsOptimizable*Address and dcbz depends on the BAT mapping, so we need a flush here.
  m_system.GetJitInterface().ClearSafe();
}

void MMU::IBATUpdated()
{
  m_ibat_table = {};
  UpdateBATs(m_ibat_table, SPR_IBAT0U);
  bool extended_bats = m_system.IsWii() && HID4(m_ppc_state).SBE;
  if (extended_bats)
    UpdateBATs(m_ibat_table, SPR_IBAT4U);
  if (m_memory.GetFakeVMEM())
  {
    // In Fake-MMU mode, insert some extra entries into the BAT tables.
    UpdateFakeMMUBat(m_ibat_table, 0x40000000);
    UpdateFakeMMUBat(m_ibat_table, 0x70000000);
  }
  m_system.GetJitInterface().ClearSafe();
}

// Translate effective address using BAT or PAT.  Returns 0 if the address cannot be translated.
// Through the hardware looks up BAT and TLB in parallel, BAT is used first if available.
// So we first check if there is a matching BAT entry, else we look for the TLB in
// TranslatePageAddress().
template <const XCheckTLBFlag flag>
MMU::TranslateAddressResult MMU::TranslateAddress(u32 address)
{
  bool wi = false;

  if (TranslateBatAddress(IsOpcodeFlag(flag) ? m_ibat_table : m_dbat_table, &address, &wi))
    return TranslateAddressResult{TranslateAddressResultEnum::BAT_TRANSLATED, address, wi};

  return TranslatePageAddress<flag>(EffectiveAddress{address}, &wi);
}

std::optional<u32> MMU::GetTranslatedAddress(u32 address)
{
  auto result = TranslateAddress<XCheckTLBFlag::NoException>(address);
  if (!result.Success())
  {
    return std::nullopt;
  }
  return std::optional<u32>(result.address);
}

void ClearDCacheLineFromJit(MMU& mmu, u32 address, UGeckoInstruction inst)
{
  mmu.ClearDCacheLine(address, inst);
}
u32 ReadU8FromJit(MMU& mmu, u32 address, UGeckoInstruction inst)
{
  return mmu.Read_U8(address, inst);
}
u32 ReadU16FromJit(MMU& mmu, u32 address, UGeckoInstruction inst)
{
  return mmu.Read_U16(address, inst);
}
u32 ReadU32FromJit(MMU& mmu, u32 address, UGeckoInstruction inst)
{
  return mmu.Read_U32(address, inst);
}
u64 ReadU64FromJit(MMU& mmu, u32 address, UGeckoInstruction inst)
{
  return mmu.Read_U64(address, inst);
}
void WriteU8FromJit(MMU& mmu, u32 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U8(var, address, inst);
}
void WriteU16FromJit(MMU& mmu, u32 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U16(var, address, inst);
}
void WriteU32FromJit(MMU& mmu, u32 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U32(var, address, inst);
}
void WriteU64FromJit(MMU& mmu, u64 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U64(var, address, inst);
}
void WriteU16SwapFromJit(MMU& mmu, u32 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U16_Swap(var, address, inst);
}
void WriteU32SwapFromJit(MMU& mmu, u32 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U32_Swap(var, address, inst);
}
void WriteU64SwapFromJit(MMU& mmu, u64 var, u32 address, UGeckoInstruction inst)
{
  mmu.Write_U64_Swap(var, address, inst);
}

static bool IsDcbz(UGeckoInstruction inst)
{
  // dcbz, dcbz_l
  return inst.SUBOP10 == 1014 && (inst.OPCD == 31 || inst.OPCD == 4);
}

static bool IsFloat(UGeckoInstruction inst, size_t access_size)
{
  // Floating loadstore
  if (inst.OPCD >= 48 && inst.OPCD < 56)
    return true;

  // Paired non-indexed loadstore
  if (inst.OPCD >= 56 && inst.OPCD < 62)
    return access_size == (inst.W ? 32 : 64);

  // Paired indexed loadstore
  if (inst.OPCD == 4 && inst.SUBOP10 != 1014)
    return access_size == (inst.Wx ? 32 : 64);

  return false;
}

static bool IsMultiword(UGeckoInstruction inst)
{
  // lmw, stmw
  if (inst.OPCD == 46 || inst.OPCD == 47)
    return true;

  if (inst.OPCD != 31)
    return false;

  // lswx, lswi, stswx, stswi
  return inst.SUBOP10 == 533 || inst.SUBOP10 == 597 || inst.SUBOP10 == 661 || inst.SUBOP10 == 725;
}

static bool IsLwarxOrStwcx(UGeckoInstruction inst)
{
  // lwarx, stwcx
  return inst.OPCD == 31 && (inst.SUBOP10 == 20 || inst.SUBOP10 == 150);
}

static bool IsEciwxOrEcowx(UGeckoInstruction inst)
{
  // eciwx, ecowx
  return inst.OPCD == 31 && (inst.SUBOP10 == 310 || inst.SUBOP10 == 438);
}

bool AccessCausesAlignmentExceptionIfWi(UGeckoInstruction inst)
{
  return IsDcbz(inst);
}

bool AccessCausesAlignmentExceptionIfMisaligned(UGeckoInstruction inst, size_t access_size)
{
  return IsFloat(inst, access_size) || IsMultiword(inst) || IsLwarxOrStwcx(inst) ||
         IsEciwxOrEcowx(inst);
}

bool AccessCausesAlignmentException(u32 effective_address, size_t access_size,
                                    UGeckoInstruction inst, bool wi)
{
  if (wi && AccessCausesAlignmentExceptionIfWi(inst))
    return true;

  if ((effective_address & GetAlignmentMask(access_size)) == 0)
    return false;

  return AccessCausesAlignmentExceptionIfMisaligned(inst, access_size);
}

}  // namespace PowerPC
