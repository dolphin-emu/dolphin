// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/MMIOHandlers.h"

namespace MMIO
{
// Checks if a given physical memory address refers to the MMIO address range.
// In practice, most games use a virtual memory mapping (via BATs set in the
// IPL) that matches the physical memory mapping for MMIOs.
//
// We have a special exception here for FIFO writes: these are handled via a
// different mechanism and should not go through the normal MMIO access
// interface.
inline bool IsMMIOAddress(u32 address)
{
  if (address == 0x0C008000)
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

// Some utilities functions to define MMIO mappings.
namespace Utils
{
// Allow grabbing pointers to the high and low part of a 32 bits pointer.
inline u16* LowPart(u32* ptr)
{
  return (u16*)ptr;
}
inline u16* LowPart(volatile u32* ptr)
{
  return (u16*)ptr;
}
inline u16* HighPart(u32* ptr)
{
  return LowPart(ptr) + 1;
}
inline u16* HighPart(volatile u32* ptr)
{
  return LowPart(ptr) + 1;
}
}  // namespace Utils

class Mapping
{
public:
  Mapping()
  {
    memset(m_read8_handlers.data(), 0, m_read8_handlers.size());
    memset(m_read16_handlers.data(), 0, m_read16_handlers.size());
    memset(m_read32_handlers.data(), 0, m_read32_handlers.size());

    memset(m_write8_handlers.data(), 0, m_write8_handlers.size());
    memset(m_write16_handlers.data(), 0, m_write16_handlers.size());
    memset(m_write32_handlers.data(), 0, m_write32_handlers.size());
  }

  ~Mapping()
  {
    for(size_t i = 0; i < m_read8_handlers.size(); ++i)
    {
      if(m_read8_handlers[i])
        delete m_read8_handlers[i];
    }

    for(size_t i = 0; i < m_read16_handlers.size(); ++i)
    {
      if(m_read16_handlers[i])
        delete m_read16_handlers[i];
    }

    for(size_t i = 0; i < m_read32_handlers.size(); ++i)
    {
      if(m_read32_handlers[i])
        delete m_read32_handlers[i];
    }

    for(size_t i = 0; i < m_write8_handlers.size(); ++i)
    {
      if(m_write8_handlers[i])
        delete m_write8_handlers[i];
    }

    for(size_t i = 0; i < m_write16_handlers.size(); ++i)
    {
      if(m_write16_handlers[i])
        delete m_write16_handlers[i];
    }

    for(size_t i = 0; i < m_write32_handlers.size(); ++i)
    {
      if(m_write32_handlers[i])
        delete m_write32_handlers[i];
    }
  }

  // MMIO registration interface. Use this to register new MMIO handlers.
  //
  // Example usages can be found in just about any HW/ module in Dolphin's
  // codebase.
  template <typename Unit>
  void Register(u32 addr, ReadHandler<Unit>* read, WriteHandler<Unit>* write);

  // Handlers access interface.
  //
  // Use when you care more about how to access the MMIO register for an
  // address than the current value of that register. For example, this is
  // what could be used to implement fast MMIO accesses in Dolphin's JIT.
  template <typename Unit>
  ReadHandler<Unit>* GetHandlerForRead(u32 addr);

  template <typename Unit>
  WriteHandler<Unit>* GetHandlerForWrite(u32 addr);

  // Direct read/write interface.
  //
  // These functions allow reading/writing an MMIO register at a given
  // address. They are used by the Memory:: access functions, which are
  // called in interpreter mode, from Dolphin's own code, or from JIT'd code
  // where the access address could not be predicted.
  template <typename Unit>
  Unit Read(u32 addr)
  {
    return GetHandlerForRead<Unit>(addr)->Read(addr);
  }

  template <typename Unit>
  void Write(u32 addr, Unit val)
  {
    return GetHandlerForWrite<Unit>(addr)->Write(addr, val);
  }

private:
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
  static const u32 BLOCK_SIZE = 0x10000;
  static const u32 NUM_MMIOS = NUM_BLOCKS * BLOCK_SIZE;

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

  // These arrays contain the handlers for each MMIO access type: read/write
  // to 8/16/32 bits. They are indexed using the UniqueID(addr) function
  // defined earlier, which maps an MMIO address to a unique ID by using the
  // MMIO block ID.
  //
  // Each array contains NUM_MMIOS / sizeof (AccessType) because larger
  // access types mean less possible adresses (assuming aligned only
  // accesses).
  std::array<ReadHandler<u8>*, NUM_MMIOS / sizeof(u8)> m_read8_handlers;
  std::array<ReadHandler<u16>*, NUM_MMIOS / sizeof(u16)> m_read16_handlers;
  std::array<ReadHandler<u32>*, NUM_MMIOS / sizeof(u32)> m_read32_handlers;

  std::array<WriteHandler<u8>*, NUM_MMIOS / sizeof(u8)> m_write8_handlers;
  std::array<WriteHandler<u16>*, NUM_MMIOS / sizeof(u16)> m_write16_handlers;
  std::array<WriteHandler<u32>*, NUM_MMIOS / sizeof(u32)> m_write32_handlers;
};

// register
template <>
inline void Mapping::Register<u8>(u32 addr, ReadHandler<u8>* read, WriteHandler<u8>* write)
{
  m_read8_handlers[UniqueID(addr) / sizeof(u8)] = read;
  m_write8_handlers[UniqueID(addr) / sizeof(u8)] = write;
}

template <>
inline void Mapping::Register<u16>(u32 addr, ReadHandler<u16>* read, WriteHandler<u16>* write)
{
  m_read16_handlers[UniqueID(addr) / sizeof(u16)] = read;
  m_write16_handlers[UniqueID(addr) / sizeof(u16)] = write;
}

template <>
inline void Mapping::Register<u32>(u32 addr, ReadHandler<u32>* read, WriteHandler<u32>* write)
{
  m_read32_handlers[UniqueID(addr) / sizeof(u32)] = read;
  m_write32_handlers[UniqueID(addr) / sizeof(u32)] = write;
}

// handler for read
template <>
inline ReadHandler<u8>* Mapping::GetHandlerForRead<u8>(u32 addr)
{
  return m_read8_handlers[UniqueID(addr) / sizeof(u8)];
}

template <>
inline ReadHandler<u16>* Mapping::GetHandlerForRead<u16>(u32 addr)
{
  return m_read16_handlers[UniqueID(addr) / sizeof(u16)];
}

template <>
inline ReadHandler<u32>* Mapping::GetHandlerForRead<u32>(u32 addr)
{
  return m_read32_handlers[UniqueID(addr) / sizeof(u32)];
}

// handler for write
template <>
inline WriteHandler<u8>* Mapping::GetHandlerForWrite<u8>(u32 addr)
{
  return m_write8_handlers[UniqueID(addr) / sizeof(u8)];
}

template <>
inline WriteHandler<u16>* Mapping::GetHandlerForWrite<u16>(u32 addr)
{
  return m_write16_handlers[UniqueID(addr) / sizeof(u16)];
}

template <>
inline WriteHandler<u32>* Mapping::GetHandlerForWrite<u32>(u32 addr)
{
  return m_write32_handlers[UniqueID(addr) / sizeof(u32)];
}

// Dummy 64 bits variants of these functions. While 64 bits MMIO access is
// not supported, we need these in order to make the code compile.
template <>
inline u64 Mapping::Read<u64>(u32 addr)
{
  DEBUG_ASSERT(0);
  return 0;
}

template <>
inline void Mapping::Write(u32 addr, u64 val)
{
  DEBUG_ASSERT(0);
}

// Converters to larger and smaller size. Probably the most complex of these
// handlers to implement. They do not define new handling method types but
// instead will internally use the types defined above.
template <typename T>
struct SmallerAccessSize
{
};
template <>
struct SmallerAccessSize<u16>
{
    typedef u8 value;
};
template <>
struct SmallerAccessSize<u32>
{
    typedef u16 value;
};

template <typename T>
struct LargerAccessSize
{
};
template <>
struct LargerAccessSize<u8>
{
    typedef u16 value;
};
template <>
struct LargerAccessSize<u16>
{
    typedef u32 value;
};

template <typename T>
ReadHandler<T>* ReadToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr)
{
  typedef typename SmallerAccessSize<T>::value ST;

  ReadHandler<ST>* high_part = mmio->GetHandlerForRead<ST>(high_part_addr);
  ReadHandler<ST>* low_part = mmio->GetHandlerForRead<ST>(low_part_addr);

  // TODO(delroth): optimize
  return ComplexRead<T>([=](u32 addr) {
      return ((T)high_part->Read(high_part_addr) << (8 * sizeof(ST))) | low_part->Read(low_part_addr);
  });
}

template <typename T>
WriteHandler<T>* WriteToSmaller(Mapping* mmio, u32 high_part_addr, u32 low_part_addr)
{
  typedef typename SmallerAccessSize<T>::value ST;

  WriteHandler<ST>* high_part = mmio->GetHandlerForWrite<ST>(high_part_addr);
  WriteHandler<ST>* low_part = mmio->GetHandlerForWrite<ST>(low_part_addr);

  // TODO(delroth): optimize
  return ComplexWrite<T>([=](u32 addr, T val) {
      high_part->Write(high_part_addr, val >> (8 * sizeof(ST)));
      low_part->Write(low_part_addr, (ST)val);
  });
}

template <typename T>
ReadHandler<T>* ReadToLarger(Mapping* mmio, u32 larger_addr, u32 shift)
{
  typedef typename LargerAccessSize<T>::value LT;

  ReadHandler<LT>* large = mmio->GetHandlerForRead<LT>(larger_addr);

  // TODO(delroth): optimize
  return ComplexRead<T>(
    [large, shift](u32 addr) { return large->Read(addr & ~(sizeof(LT) - 1)) >> shift; });
}

}  // namespace MMIO
