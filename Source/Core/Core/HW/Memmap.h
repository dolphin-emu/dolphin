// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "Core/PowerPC/MMU.h"

// Global declarations
/*#define NUMHWMEMFUN 64
#define HWSHIFT 10*/
class PointerWrap;

/*typedef void (*writeFn8 )(const u8, const u32);
typedef void (*writeFn16)(const u16,const u32);
typedef void (*writeFn32)(const u32,const u32);
typedef void (*writeFn64)(const u64,const u32);

typedef void (*readFn8 )(u8&,  const u32);
typedef void (*readFn16)(u16&, const u32);
typedef void (*readFn32)(u32&, const u32);
typedef void (*readFn64)(u64&, const u32);*/
namespace MMIO
{
class Mapping;
}

namespace Memory
{
// Base is a pointer to the base of the memory map. Yes, some MMU tricks
// are used to set up a full GC or Wii memory map in process memory.
// In 64-bit, this might point to "high memory" (above the 32-bit limit),
// so be sure to load it into a 64-bit register.
extern u8* physical_base;
extern u8* logical_base;

// This page table is used for a "soft MMU" implementation when
// setting up the full memory map in process memory isn't possible.
extern u8* physical_page_mappings_base;
extern u8* logical_page_mappings_base;

// The actual memory used for backing the memory map.
extern u8* m_pRAM;
extern u8* m_pEXRAM;
extern u8* m_pL1Cache;
extern u8* m_pFakeVMEM;

/*enum
{
	// RAM_SIZE is the amount allocated by the emulator, whereas REALRAM_SIZE is
	// what will be reported in lowmem, and thus used by emulated software.
	// Note: Writing to lowmem is done by IPL. If using retail IPL, it will
	// always be set to 24MB.
	REALRAM_SIZE	= 0x1800000,
	//RAM_SIZE		= ROUND_UP_POW2(REALRAM_SIZE),
  RAM_SIZE = 0x2000000,
	RAM_MASK		= RAM_SIZE - 1,
	FAKEVMEM_SIZE	= 0x2000000,
	FAKEVMEM_MASK	= FAKEVMEM_SIZE - 1,
	L1_CACHE_SIZE	= 0x40000,
	L1_CACHE_MASK	= L1_CACHE_SIZE - 1,
	EFB_SIZE		= 0x200000,
	EFB_MASK		= EFB_SIZE - 1,
	IO_SIZE			= 0x10000,
	EXRAM_SIZE		= 0x4000000,
	EXRAM_MASK		= EXRAM_SIZE - 1,

	ADDR_MASK_HW_ACCESS	= 0x0c000000,
	ADDR_MASK_MEM1		= 0x20000000,

#ifndef _M_X64
	MEMVIEW32_MASK  = 0x3FFFFFFF,
#endif
};

enum XCheckTLBFlag
{
	FLAG_NO_EXCEPTION,
	FLAG_READ,
	FLAG_WRITE,
	FLAG_OPCODE,
};*/

u32 GetRamSizeReal();
u32 GetRamSize();
u32 GetRamMask();
u32 GetFakeVMemSize();
u32 GetFakeVMemMask();
u32 GetL1CacheSize();
u32 GetL1CacheMask();
u32 GetIOSize();
u32 GetExRamSizeReal();
u32 GetExRamSize();
u32 GetExRamMask();

constexpr u32 MEM1_BASE_ADDR = 0x80000000U;
constexpr u32 MEM2_BASE_ADDR = 0x90000000U;
constexpr u32 MEM1_SIZE_RETAIL = 0x01800000U;
constexpr u32 MEM1_SIZE_GDEV = 0x04000000U;
constexpr u32 MEM2_SIZE_RETAIL = 0x04000000U;
constexpr u32 MEM2_SIZE_NDEV = 0x08000000U;

// MMIO mapping object.
extern std::unique_ptr<MMIO::Mapping> mmio_mapping;

// Init and Shutdown

// ONLY for use by GUI
/*u8 ReadUnchecked_U8(const u32 _Address);
u32 ReadUnchecked_U32(const u32 _Address);*/

//void WriteUnchecked_U8(const u8 _Data, const u32 _Address);
//void WriteUnchecked_U32(const u32 _Data, const u32 _Address);

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
u8* GetPointerForRange(u32 address, size_t size);
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
  const T* src = reinterpret_cast<T*>(GetPointerForRange(address, size));

  if (src == nullptr)
    return;

  for (size_t i = 0; i < size / sizeof(T); i++)
    data[i] = Common::FromBigEndian(src[i]);
}

template <typename T>
void CopyToEmuSwapped(u32 address, const T* data, size_t size)
{
  T* dest = reinterpret_cast<T*>(GetPointerForRange(address, size));

  if (dest == nullptr)
    return;

  for (size_t i = 0; i < size / sizeof(T); i++)
    dest[i] = Common::FromBigEndian(data[i]);
}
}  // namespace Memory
