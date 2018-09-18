// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
}

class Mapping
{
private:
  // These arrays contain the handlers for each MMIO access type: read/write
  // to 8/16/32 bits. They are indexed using the UniqueID(addr) function
  // defined earlier, which maps an MMIO address to a unique ID by using the
  // MMIO block ID.
  //
  // Each array contains NUM_MMIOS / sizeof (AccessType) because larger
  // access types mean less possible adresses (assuming aligned only
  // accesses).
  template <typename Unit>
  struct MMIOHander
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
    static const u32 BLOCK_SIZE = 0x10000;
    static const u32 NUM_MMIOS = NUM_BLOCKS * BLOCK_SIZE;

    static MMIOHander* Instance()
    {
      static MMIOHander inst;
      return &inst;
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

    ReadHandler<Unit>* GetHandlerForRead(u32 addr)
    {
      return m_read_handlers[UniqueID(addr) / sizeof(Unit)];
    }
    WriteHandler<Unit>* GetHandlerForWrite(u32 addr)
    {
      return m_write_handlers[UniqueID(addr) / sizeof(Unit)];
    }

    void SetHandlerForRead(u32 addr, ReadHandler<Unit>* read)
    {
      m_read_handlers[UniqueID(addr) / sizeof(Unit)] = read;
    }

    void SetHandlerForWrite(u32 addr, WriteHandler<Unit>* write)
    {
      m_write_handlers[UniqueID(addr) / sizeof(Unit)] = write;
    }

    ReadHandler<Unit>* m_read_handlers[NUM_MMIOS / sizeof(Unit)];
    WriteHandler<Unit>* m_write_handlers[NUM_MMIOS / sizeof(Unit)];
  };

public:
  // MMIO registration interface. Use this to register new MMIO handlers.
  //
  // Example usages can be found in just about any HW/ module in Dolphin's
  // codebase.
  template <typename Unit>
  void Register(u32 addr, ReadHandler<Unit>* read, WriteHandler<Unit>* write)
  {
    MMIOHander<Unit>::Instance()->SetHandlerForRead(addr, read);
    MMIOHander<Unit>::Instance()->SetHandlerForWrite(addr, write);
  }

  // Direct read/write interface.
  //
  // These functions allow reading/writing an MMIO register at a given
  // address. They are used by the Memory:: access functions, which are
  // called in interpreter mode, from Dolphin's own code, or from JIT'd code
  // where the access address could not be predicted.
  template <typename Unit>
  Unit Read(u32 addr)
  {
    return MMIOHander<Unit>::Instance()->GetHandlerForRead(addr)->Read(addr);
  }

  template <typename Unit>
  void Write(u32 addr, Unit val)
  {
    MMIOHander<Unit>::Instance()->GetHandlerForWrite(addr)->Write(addr, val);
  }

  // Handlers access interface.
  //
  // Use when you care more about how to access the MMIO register for an
  // address than the current value of that register. For example, this is
  // what could be used to implement fast MMIO accesses in Dolphin's JIT.
  template <typename Unit>
  ReadHandler<Unit>* GetHandlerForRead(u32 addr)
  {
    return MMIOHander<Unit>::Instance()->GetHandlerForRead(addr);
  }

  template <typename Unit>
  WriteHandler<Unit>* GetHandlerForWrite(u32 addr)
  {
    return MMIOHander<Unit>::Instance()->GetHandlerForWrite(addr);
  }

  template <typename Unit>
  void SetHandlerForRead(u32 addr, ReadHandler<Unit>* read)
  {
    MMIOHander<Unit>::Instance()->SetHandlerForRead(addr, read);
  }

  template <typename Unit>
  void SetHandlerForWrite(u32 addr, WriteHandler<Unit>* write)
  {
    MMIOHander<Unit>::Instance()->SetHandlerForWrite(addr, write);
  }
};

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

}
