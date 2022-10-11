// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <string>
#include <tuple>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Concepts.h"
#include "Core/ConfigManager.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/MMIOHandlers.h"

#include "Common/Future/CppLibBitCast.h"
#include "Common/Future/CppLibConcepts.h"

namespace MMIO
{
// There are three main MMIO blocks on the Wii (only one on the GameCube):
//  - 0x0C00xxxx: GameCube MMIOs (CP, PE, VI, PI, MI, DSP, DVD, SI, EI, AI, GP)
//  - 0x0D00xxxx: Wii MMIOs and GC mirrors (IPC, DVD, SI, EI, AI)
//  - 0x0D80xxxx: Mirror of 0x0D00xxxx.
//
// In practice, since the third block is a mirror of the second one, we can
// assume internally that there are only two blocks: one for GC, one for Wii.
enum Block
{
  GC_BLOCK = 0,
  WII_BLOCK = 1,

  NUM_BLOCKS
};
const u32 BLOCK_SIZE = 0x10000;
const u32 NUM_MMIOS = NUM_BLOCKS * BLOCK_SIZE;

// Checks if a given physical memory address refers to the MMIO address range.
// In practice, most games use a virtual memory mapping (via BATs set in the
// IPL) that matches the physical memory mapping for MMIOs.
//
// We have a special exception here for FIFO writes: these are handled via a
// different mechanism and should not go through the normal MMIO access
// interface.
inline bool IsMMIOAddress(u32 address)
{
  if (address == GPFifo::GATHER_PIPE_PHYSICAL_ADDRESS)
    return false;  // WG Pipe
  if ((address & 0xFFFF0000) == 0x0C000000)
    return true;  // GameCube MMIOs

  if (SConfig::GetInstance().bWii)
  {
    return ((address & 0xFFFF0000) == 0x0D000000) ||  // Wii MMIOs
           ((address & 0xFFFF0000) == 0x0D800000);    // Mirror of Wii MMIOs
  }

  return false;
}

// Compute the internal unique ID for a given MMIO address. This ID is computed
// from a very simple formula: (block_id << 16) | lower_16_bits(address).
//
// The block ID can easily be computed by simply checking bit 24 (CC vs. CD).
inline u32 UniqueID(u32 address)
{
  DEBUG_ASSERT_MSG(MEMMAP,
                   ((address & 0xFFFF0000) == 0x0C000000) ||
                       ((address & 0xFFFF0000) == 0x0D000000) ||
                       ((address & 0xFFFF0000) == 0x0D800000),
                   "Trying to get the ID of a non-existing MMIO address.");

  return (((address >> 24) & 1) << 16) | (address & 0xFFFF);
}

// Some utilities functions to define MMIO mappings.
namespace Utils
{
// Allow grabbing pointers to the high and low part of a 32 bits pointer.
inline u16* LowPart(u32* ptr)
{
  return (u16*)ptr;
}
inline u16* LowPart(std::atomic<u32>* ptr)
{
  static_assert(std::atomic<u32>::is_always_lock_free && sizeof(std::atomic<u32>) == sizeof(u32));
  return LowPart(std::bit_cast<u32*>(ptr));
}
inline u16* HighPart(u32* ptr)
{
  return LowPart(ptr) + 1;
}
inline u16* HighPart(std::atomic<u32>* ptr)
{
  static_assert(std::atomic<u32>::is_always_lock_free && sizeof(std::atomic<u32>) == sizeof(u32));
  return HighPart(std::bit_cast<u32*>(ptr));
}
}  // namespace Utils

template <class Unit>
concept HandlerConstraint = Common::SameAsAnyOf<Unit, u8, u16, u32>;

class Mapping
{
public:
  // MMIO registration interface. Use this to register new MMIO handlers.
  //
  // Example usages can be found in just about any HW/ module in Dolphin's
  // codebase.
  template <HandlerConstraint T>
  void RegisterRead(u32 addr, ReadHandlingMethod<T>* read)
  {
    GetHandlerForRead<T>(addr).ResetMethod(read);
  }

  template <HandlerConstraint T>
  void RegisterWrite(u32 addr, WriteHandlingMethod<T>* write)
  {
    GetHandlerForWrite<T>(addr).ResetMethod(write);
  }

  template <HandlerConstraint T>
  void Register(u32 addr, ReadHandlingMethod<T>* read, WriteHandlingMethod<T>* write)
  {
    RegisterRead(addr, read);
    RegisterWrite(addr, write);
  }

  // Direct read/write interface.
  //
  // These functions allow reading/writing an MMIO register at a given
  // address. They are used by the Memory:: access functions, which are
  // called in interpreter mode, from Dolphin's own code, or from JIT'd code
  // where the access address could not be predicted.
  template <HandlerConstraint T>
  T Read(u32 addr)
  {
    return GetHandlerForRead<T>(addr).Read(addr);
  }

  template <HandlerConstraint T>
  void Write(u32 addr, T val)
  {
    GetHandlerForWrite<T>(addr).Write(addr, val);
  }

  // Dummy 64 bits variants of these functions. While 64 bits MMIO access is
  // not supported, we need these in order to make the code compile.
  template <std::same_as<u64> T>
  T Read(u32 addr)
  {
    ASSERT(false);
    return 0;
  }

  template <std::same_as<u64> T>
  void Write(u32 addr, T val)
  {
    ASSERT(false);
  }

  // Handlers access interface.
  //
  // Use when you care more about how to access the MMIO register for an
  // address than the current value of that register. For example, this is
  // what could be used to implement fast MMIO accesses in Dolphin's JIT.
  template <HandlerConstraint T>
  ReadHandler<T>& GetHandlerForRead(u32 addr)
  {
    return GetReadHandler<T>(UniqueID(addr) / sizeof(T));
  }

  template <HandlerConstraint T>
  WriteHandler<T>& GetHandlerForWrite(u32 addr)
  {
    return GetWriteHandler<T>(UniqueID(addr) / sizeof(T));
  }

private:
  // These arrays contain the handlers for each MMIO access type: read/write
  // to 8/16/32 bits. They are indexed using the UniqueID(addr) function
  // defined earlier, which maps an MMIO address to a unique ID by using the
  // MMIO block ID.
  //
  // Each array contains NUM_MMIOS / sizeof (AccessType) because larger
  // access types mean less possible adresses (assuming aligned only
  // accesses).
  template <HandlerConstraint T>
  struct HandlerArray
  {
    using Read = std::array<ReadHandler<T>, NUM_MMIOS / sizeof(T)>;
    using Write = std::array<WriteHandler<T>, NUM_MMIOS / sizeof(T)>;
  };

  HandlerArray<u8>::Read m_read_handlers8;
  HandlerArray<u16>::Read m_read_handlers16;
  HandlerArray<u32>::Read m_read_handlers32;

  HandlerArray<u8>::Write m_write_handlers8;
  HandlerArray<u16>::Write m_write_handlers16;
  HandlerArray<u32>::Write m_write_handlers32;

  // Getter functions for the handler arrays.
  template <HandlerConstraint T>
  ReadHandler<T>& GetReadHandler(size_t index)
  {
    auto handlers = std::tie(m_read_handlers8, m_read_handlers16, m_read_handlers32);

    using ArrayType = typename HandlerArray<T>::Read;
    return std::get<ArrayType&>(handlers)[index];
  }

  template <HandlerConstraint T>
  WriteHandler<T>& GetWriteHandler(size_t index)
  {
    auto handlers = std::tie(m_write_handlers8, m_write_handlers16, m_write_handlers32);

    using ArrayType = typename HandlerArray<T>::Write;
    return std::get<ArrayType&>(handlers)[index];
  }
};
}  // namespace MMIO
