// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/ConfigManager.h"

// Global declarations
class PointerWrap;
namespace MMIO
{
class Mapping;
}

namespace Memory
{
// Base is a pointer to the base of the memory map. Yes, some MMU tricks
// are used to set up a full GC or Wii memory map in process memory.  on
// 32-bit, you have to mask your offsets with 0x3FFFFFFF. This means that
// some things are mirrored too many times, but eh... it works.

// In 64-bit, this might point to "high memory" (above the 32-bit limit),
// so be sure to load it into a 64-bit register.
extern u8* physical_base;
extern u8* logical_base;

// These are guaranteed to point to "low memory" addresses (sub-32-bit).
extern u8* m_pRAM;
extern u8* m_pEXRAM;
extern u8* m_pL1Cache;
extern u8* m_pFakeVMEM;

// RAM_SIZE is the amount allocated by the emulator, whereas REALRAM_SIZE is
// what will be reported in lowmem, and thus used by emulated software.
// Note: Writing to lowmem is done by IPL. If using retail IPL, it will
// always be set to 24MB.
extern u32 REALRAM_SIZE;
extern u32 RAM_SIZE;
extern u32 RAM_MASK;
extern u32 FAKEVMEM_SIZE;
extern u32 FAKEVMEM_MASK;
extern u32 L1_CACHE_SIZE;
extern u32 L1_CACHE_MASK;
extern u32 IO_SIZE;
extern u32 EXRAM_SIZE;
//extern u32 EXRAM_SIZE;
extern u32 EXRAM_MASK;

// MMIO mapping object.
extern std::unique_ptr<MMIO::Mapping> mmio_mapping;

// Init and Shutdown
bool IsInitialized();
void Init();
void Shutdown();
bool InitFastmemArena();
void ShutdownFastmemArena();
void DoState(PointerWrap& p);

void UpdateLogicalMemory(const PowerPC::BatTable& dbat_table);

void Clear();

// Routines to access physically addressed memory, designed for use by
// emulated hardware outside the CPU. Use "Device_" prefix.
std::string GetString(u32 em_address, size_t size = 0);
u8* GetPointer(u32 address);
void CopyFromEmu(void* data, u32 address, size_t size);
void CopyToEmu(u32 address, const void* data, size_t size);
void Memset(u32 address, u8 value, size_t size);
u8 Read_U8(u32 address);
u16 Read_U16(u32 address);
u32 Read_U32(u32 address);
u64 Read_U64(u32 address);
void Write_U8(u8 var, u32 address);
void Write_U16(u16 var, u32 address);
void Write_U32(u32 var, u32 address);
void Write_U64(u64 var, u32 address);
void Write_U32_Swap(u32 var, u32 address);
void Write_U64_Swap(u64 var, u32 address);

// Templated functions for byteswapped copies.
template <typename T>
void CopyFromEmuSwapped(T* data, u32 address, size_t size)
{
  const T* src = reinterpret_cast<T*>(GetPointer(address));

  if (src == nullptr)
    return;

  for (size_t i = 0; i < size / sizeof(T); i++)
    data[i] = Common::FromBigEndian(src[i]);
}

template <typename T>
void CopyToEmuSwapped(u32 address, const T* data, size_t size)
{
  T* dest = reinterpret_cast<T*>(GetPointer(address));

  if (dest == nullptr)
    return;

  for (size_t i = 0; i < size / sizeof(T); i++)
    dest[i] = Common::FromBigEndian(data[i]);
}
}  // namespace Memory
