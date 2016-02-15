// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

// Enable memory checks in the Debug/DebugFast builds, but NOT in release
#if defined(_DEBUG) || defined(DEBUGFAST)
	#define ENABLE_MEM_CHECK
#endif

// Global declarations
class PointerWrap;
namespace MMIO { class Mapping; }

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
extern bool bFakeVMEM;

enum
{
	// RAM_SIZE is the amount allocated by the emulator, whereas REALRAM_SIZE is
	// what will be reported in lowmem, and thus used by emulated software.
	// Note: Writing to lowmem is done by IPL. If using retail IPL, it will
	// always be set to 24MB.
	REALRAM_SIZE  = 0x01800000,
	RAM_SIZE      = ROUND_UP_POW2(REALRAM_SIZE),
	RAM_MASK      = RAM_SIZE - 1,
	FAKEVMEM_SIZE = 0x02000000,
	FAKEVMEM_MASK = FAKEVMEM_SIZE - 1,
	L1_CACHE_SIZE = 0x00040000,
	L1_CACHE_MASK = L1_CACHE_SIZE - 1,
	IO_SIZE       = 0x00010000,
	EXRAM_SIZE    = 0x04000000,
	EXRAM_MASK    = EXRAM_SIZE - 1,

	ADDR_MASK_HW_ACCESS = 0x0c000000,
	ADDR_MASK_MEM1      = 0x20000000,

#if _ARCH_32
	MEMVIEW32_MASK  = 0x3FFFFFFF,
#endif
};

// MMIO mapping object.
extern std::unique_ptr<MMIO::Mapping> mmio_mapping;

// Init and Shutdown
bool IsInitialized();
void Init();
void Shutdown();
void DoState(PointerWrap &p);

void Clear();
bool AreMemoryBreakpointsActivated();

// Routines to access physically addressed memory, designed for use by
// emulated hardware outside the CPU. Use "Device_" prefix.
std::string GetString(u32 em_address, size_t size = 0);
u8* GetPointer(const u32 address);
void CopyFromEmu(void* data, u32 address, size_t size);
void CopyToEmu(u32 address, const void* data, size_t size);
void Memset(u32 address, u8 value, size_t size);
u8  Read_U8(const u32 address);
u16 Read_U16(const u32 address);
u32 Read_U32(const u32 address);
u64 Read_U64(const u32 address);
void Write_U8(const u8 var, const u32 address);
void Write_U16(const u16 var, const u32 address);
void Write_U32(const u32 var, const u32 address);
void Write_U64(const u64 var, const u32 address);
void Write_U32_Swap(const u32 var, const u32 address);
void Write_U64_Swap(const u64 var, const u32 address);

// Templated functions for byteswapped copies.
template <typename T>
void CopyFromEmuSwapped(T* data, u32 address, size_t size)
{
	const T* src = reinterpret_cast<T*>(GetPointer(address));

	if(src == nullptr)
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

}
