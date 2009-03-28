// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _MEMMAP_H
#define _MEMMAP_H

// Includes
#include <string>
#include "Common.h"

// Global declarations
class PointerWrap;

typedef void (*writeFn8 )(const u8, const u32);
typedef void (*writeFn16)(const u16,const u32);
typedef void (*writeFn32)(const u32,const u32);
typedef void (*writeFn64)(const u64,const u32);

typedef void (*readFn8 )(u8&,  const u32);
typedef void (*readFn16)(u16&, const u32);
typedef void (*readFn32)(u32&, const u32);
typedef void (*readFn64)(u64&, const u32);

namespace Memory
{
	/* Base is a pointer to the base of the memory map. Yes, some MMU tricks
	 are used to set up a full GC or Wii memory map in process memory.  on
	 32-bit, you have to mask your offsets with 0x3FFFFFFF. This means that
	 some things are mirrored, but eh... it works. */

	extern u8 *base; 
	extern u8* m_pRAM;
	extern u8* m_pL1Cache;

	// The size should be 24mb only, but the RAM_MASK wouldn't work anymore
	enum
	{
		RAM_SIZE		= 0x2000000,
		RAM_MASK		= 0x1FFFFFF,
		FAKEVMEM_SIZE	= 0x2000000,
		FAKEVMEM_MASK	= 0x1FFFFFF,
		REALRAM_SIZE	= 0x1800000,		
		L1_CACHE_SIZE	= 0x40000,
		L1_CACHE_MASK	= 0x3FFFF,
		EFB_SIZE	    = 0x200000,
		EFB_MASK	    = 0x1FFFFF,
		IO_SIZE         = 0x10000,
		EXRAM_SIZE      = 0x4000000,
		EXRAM_MASK      = 0x3FFFFFF,
		#ifdef _M_IX86
			MEMVIEW32_MASK  = 0x3FFFFFFF,
		#endif
	};	

	// Init and Shutdown
	bool IsInitialized();
	bool Init();
	bool Shutdown();
	void DoState(PointerWrap &p);

	void Clear();
	bool AreMemoryBreakpointsActivated();

	// ONLY for use by GUI
	u8 ReadUnchecked_U8(const u32 _Address);
	u32 ReadUnchecked_U32(const u32 _Address);
	
	void WriteUnchecked_U8(const u8 _Data, const u32 _Address);
	void WriteUnchecked_U32(const u32 _Data, const u32 _Address);

	void InitHWMemFuncs();
	void InitHWMemFuncsWii();

	u32 Read_Instruction(const u32 _Address);
	bool IsRAMAddress(const u32 addr, bool allow_locked_cache = false);
	writeFn32 GetHWWriteFun32(const u32 _Address);

	inline u8* GetCachePtr() {return m_pL1Cache;}
	inline u8* GetMainRAMPtr() {return m_pRAM;}
	inline u32 ReadFast32(const u32 _Address)
	{
	#ifdef _M_IX86
		return Common::swap32(*(u32 *)(base + (_Address & MEMVIEW32_MASK)));  // ReadUnchecked_U32(_Address);
	#elif defined(_M_X64)
		return Common::swap32(*(u32 *)(base + _Address));
	#endif
	}

	u32 Read_Opcode(const u32 _Address);


	// For use by emulator

	/* Local byteswap shortcuts. They are placed inline so that they may hopefully be executed faster
	   than otherwise */
	inline u8 bswap(u8 val) {return val;}
	inline u16 bswap(u16 val) {return Common::swap16(val);}
	inline u32 bswap(u32 val) {return Common::swap32(val);}
	inline u64 bswap(u64 val) {return Common::swap64(val);}
	// =================

	// Read and write functions
	#define NUMHWMEMFUN 64
	#define HWSHIFT 10
	#define HW_MASK 0x3FF

	u8  Read_U8(const u32 _Address);
	u16 Read_U16(const u32 _Address);
	u32 Read_U32(const u32 _Address);
	u64 Read_U64(const u32 _Address);

	void Write_U8(const u8 _Data, const u32 _Address);
	void Write_U16(const u16 _Data, const u32 _Address);
	void Write_U32(const u32 _Data, const u32 _Address);
	void Write_U64(const u64 _Data, const u32 _Address);

	void WriteHW_U32(const u32 _Data, const u32 _Address);
	void GetString(std::string& _string, const u32 _Address);
	void WriteBigEData(const u8 *_pData, const u32 _Address, const u32 size);
	void ReadBigEData(u8 *_pDest, const u32 _Address, const u32 size);
	u8* GetPointer(const u32 _Address);
	void DMA_LCToMemory(const u32 _iMemAddr, const u32 _iCacheAddr, const u32 _iNumBlocks);
	void DMA_MemoryToLC(const u32 _iCacheAddr, const u32 _iMemAddr, const u32 _iNumBlocks);
	void Memset(const u32 _Address, const u8 _Data, const u32 _iLength);

	// TLB functions
	void SDRUpdated();
	enum XCheckTLBFlag
	{
		FLAG_NO_EXCEPTION,
		FLAG_READ,
		FLAG_WRITE,
		FLAG_OPCODE,
	};
	u32 CheckDTLB(u32 _Address, XCheckTLBFlag _Flag);
	extern u32 pagetable_base;
	extern u32 pagetable_hashmask;
};

#endif
