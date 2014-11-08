// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

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
extern u8* base;

// These are guaranteed to point to "low memory" addresses (sub-32-bit).
extern u8* m_pRAM;
extern u8* m_pEXRAM;
extern u8* m_pL1Cache;
extern u8* m_pVirtualFakeVMEM;
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
	L1_CACHE_SIZE = 0x40000,
	L1_CACHE_MASK = L1_CACHE_SIZE - 1,
	EFB_SIZE      = 0x200000,
	EFB_MASK      = EFB_SIZE - 1,
	IO_SIZE       = 0x10000,
	EXRAM_SIZE    = 0x04000000,
	EXRAM_MASK    = EXRAM_SIZE - 1,

	ADDR_MASK_HW_ACCESS = 0x0c000000,
	ADDR_MASK_MEM1      = 0x20000000,

#if _ARCH_32
	MEMVIEW32_MASK  = 0x3FFFFFFF,
#endif
};

// MMIO mapping object.
extern MMIO::Mapping* mmio_mapping;

// Init and Shutdown
bool IsInitialized();
void Init();
void Shutdown();
void DoState(PointerWrap &p);

void Clear();
bool AreMemoryBreakpointsActivated();

// ONLY for use by GUI
u8 ReadUnchecked_U8(const u32 _Address);
u32 ReadUnchecked_U32(const u32 _Address);

void WriteUnchecked_U8(const u8 _Data, const u32 _Address);
void WriteUnchecked_U32(const u32 _Data, const u32 _Address);

bool IsRAMAddress(const u32 addr, bool allow_locked_cache = false, bool allow_fake_vmem = false);

// used by interpreter to read instructions, uses iCache
u32 Read_Opcode(const u32 _Address);
// this is used by Debugger a lot.
// For now, just reads from memory!
u32 Read_Instruction(const u32 _Address);


// For use by emulator

u8  Read_U8(const u32 _Address);
u16 Read_U16(const u32 _Address);
u32 Read_U32(const u32 _Address);
u64 Read_U64(const u32 _Address);

// Useful helper functions, used by ARM JIT
float Read_F32(const u32 _Address);
double Read_F64(const u32 _Address);

// used by JIT. Return zero-extended 32bit values
u32 Read_U8_ZX(const u32 _Address);
u32 Read_U16_ZX(const u32 _Address);

void Write_U8(const u8 _Data, const u32 _Address);
void Write_U16(const u16 _Data, const u32 _Address);
void Write_U32(const u32 _Data, const u32 _Address);
void Write_U64(const u64 _Data, const u32 _Address);

void Write_U16_Swap(const u16 _Data, const u32 _Address);
void Write_U32_Swap(const u32 _Data, const u32 _Address);
void Write_U64_Swap(const u64 _Data, const u32 _Address);

// Useful helper functions, used by ARM JIT
void Write_F64(const double _Data, const u32 _Address);

std::string GetString(u32 em_address, size_t size = 0);

u8* GetPointer(const u32 _Address);
void DMA_LCToMemory(const u32 _iMemAddr, const u32 _iCacheAddr, const u32 _iNumBlocks);
void DMA_MemoryToLC(const u32 _iCacheAddr, const u32 _iMemAddr, const u32 _iNumBlocks);
void CopyFromEmu(void* data, u32 address, size_t size);
void CopyToEmu(u32 address, const void* data, size_t size);
void Memset(const u32 _Address, const u8 _Data, const u32 _iLength);
void ClearCacheLine(const u32 _Address); // Zeroes 32 bytes; address should be 32-byte-aligned

// TLB functions
void SDRUpdated();
enum XCheckTLBFlag
{
	FLAG_NO_EXCEPTION,
	FLAG_READ,
	FLAG_WRITE,
	FLAG_OPCODE,
};
u32 TranslateAddress(u32 _Address, XCheckTLBFlag _Flag);
void InvalidateTLBEntry(u32 _Address);
extern u32 pagetable_base;
extern u32 pagetable_hashmask;
}
