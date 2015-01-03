// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official Git repository and contact information can be found at
// https://github.com/dolphin-emu/dolphin

#include "Common/Atomic.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MMIO.h"
#include "Core/PowerPC/PowerPC.h"

#include "VideoCommon/VideoBackendBase.h"

#ifdef USE_GDBSTUB
#include "Core/PowerPC/GDBStub.h"
#endif

namespace Memory
{

#define HW_PAGE_SIZE 4096

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
inline u8 bswap(u8 val)   { return val; }
inline s8 bswap(s8 val)   { return val; }
inline u16 bswap(u16 val) { return Common::swap16(val); }
inline s16 bswap(s16 val) { return Common::swap16(val); }
inline u32 bswap(u32 val) { return Common::swap32(val); }
inline u64 bswap(u64 val) { return Common::swap64(val); }
// =================


// Nasty but necessary. Super Mario Galaxy pointer relies on this stuff.
static u32 EFB_Read(const u32 addr)
{
	u32 var = 0;
	// Convert address to coordinates. It's possible that this should be done
	// differently depending on color depth, especially regarding PEEK_COLOR.
	int x = (addr & 0xfff) >> 2;
	int y = (addr >> 12) & 0x3ff;

	if (addr & 0x00400000)
	{
		var = g_video_backend->Video_AccessEFB(PEEK_Z, x, y, 0);
		DEBUG_LOG(MEMMAP, "EFB Z Read @ %i, %i\t= 0x%08x", x, y, var);
	}
	else
	{
		var = g_video_backend->Video_AccessEFB(PEEK_COLOR, x, y, 0);
		DEBUG_LOG(MEMMAP, "EFB Color Read @ %i, %i\t= 0x%08x", x, y, var);
	}

	return var;
}

static void GenerateDSIException(u32 _EffectiveAddress, bool _bWrite);

template <XCheckTLBFlag flag, typename T>
__forceinline T ReadFromHardware(const u32 em_address)
{
	int segment = em_address >> 28;
	// Quick check for an address that can't meet any of the following conditions,
	// to speed up the MMU path.
	if (!BitSet32(0xCFC)[segment])
	{
		// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
		if ((em_address & 0xC8000000) == 0xC8000000)
		{
			if (em_address < 0xcc000000)
				return EFB_Read(em_address);
			else
				return (T)mmio_mapping->Read<typename std::make_unsigned<T>::type>(em_address);
		}
		else if (segment == 0x8 || segment == 0xC || segment == 0x0)
		{
			return bswap((*(const T*)&m_pRAM[em_address & RAM_MASK]));
		}
		else if (m_pEXRAM && (segment == 0x9 || segment == 0xD || segment == 0x1))
		{
			return bswap((*(const T*)&m_pEXRAM[em_address & EXRAM_MASK]));
		}
		else if (segment == 0xE && (em_address < (0xE0000000 + L1_CACHE_SIZE)))
		{
			return bswap((*(const T*)&m_pL1Cache[em_address & L1_CACHE_MASK]));
		}
	}

	if (bFakeVMEM && (segment == 0x7 || segment == 0x4))
	{
		// fake VMEM
		return bswap((*(const T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK]));
	}

	// MMU: Do page table translation
	u32 tlb_addr = TranslateAddress<flag>(em_address);
	if (tlb_addr == 0)
	{
		if (flag == FLAG_READ)
			GenerateDSIException(em_address, false);
		return 0;
	}

	// Handle loads that cross page boundaries (ewwww)
	// The alignment check isn't strictly necessary, but since this is a rare slow path, it provides a faster
	// (1 instruction on x86) bailout.
	if (sizeof(T) > 1 && (em_address & (sizeof(T) - 1)) && (em_address & (HW_PAGE_SIZE - 1)) > HW_PAGE_SIZE - sizeof(T))
	{
		// This could be unaligned down to the byte level... hopefully this is rare, so doing it this
		// way isn't too terrible.
		// TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
		// Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
		u32 em_address_next_page = (em_address + sizeof(T) - 1) & ~(HW_PAGE_SIZE - 1);
		u32 tlb_addr_next_page = TranslateAddress<flag>(em_address_next_page);
		if (tlb_addr == 0 || tlb_addr_next_page == 0)
		{
			if (flag == FLAG_READ)
				GenerateDSIException(em_address_next_page, false);
			return 0;
		}
		T var = 0;
		for (u32 addr = em_address; addr < em_address + sizeof(T); addr++, tlb_addr++)
		{
			if (addr == em_address_next_page)
				tlb_addr = tlb_addr_next_page;
			var = (var << 8) | Memory::base[tlb_addr];
		}
		return var;
	}

	// The easy case!
	return bswap(*(const T*)&Memory::base[tlb_addr]);
}


template <XCheckTLBFlag flag, typename T>
__forceinline void WriteToHardware(u32 em_address, const T data)
{
	int segment = em_address >> 28;
	// Quick check for an address that can't meet any of the following conditions,
	// to speed up the MMU path.
	if (!BitSet32(0xCFC)[segment])
	{
		// First, let's check for FIFO writes, since they are probably the most common
		// reason we end up in this function:
		if ((em_address & 0xFFFFF000) == 0xCC008000)
		{
			switch (sizeof(T))
			{
			case 1: GPFifo::Write8((u8)data, em_address); return;
			case 2: GPFifo::Write16((u16)data, em_address); return;
			case 4: GPFifo::Write32((u32)data, em_address); return;
			case 8: GPFifo::Write64((u64)data, em_address); return;
			}
		}
		if ((em_address & 0xC8000000) == 0xC8000000)
		{
			if (em_address < 0xcc000000)
			{
				int x = (em_address & 0xfff) >> 2;
				int y = (em_address >> 12) & 0x3ff;

				// TODO figure out a way to send data without falling into the template trap
				if (em_address & 0x00400000)
				{
					g_video_backend->Video_AccessEFB(POKE_Z, x, y, (u32)data);
					DEBUG_LOG(MEMMAP, "EFB Z Write %08x @ %i, %i", (u32)data, x, y);
				}
				else
				{
					g_video_backend->Video_AccessEFB(POKE_COLOR, x, y, (u32)data);
					DEBUG_LOG(MEMMAP, "EFB Color Write %08x @ %i, %i", (u32)data, x, y);
				}
				return;
			}
			else
			{
				mmio_mapping->Write(em_address, data);
				return;
			}
		}
		else if (segment == 0x8 || segment == 0xC || segment == 0x0)
		{
			*(T*)&m_pRAM[em_address & RAM_MASK] = bswap(data);
			return;
		}
		else if (m_pEXRAM && (segment == 0x9 || segment == 0xD || segment == 0x1))
		{
			*(T*)&m_pEXRAM[em_address & EXRAM_MASK] = bswap(data);
			return;
		}
		else if (segment == 0xE && (em_address < (0xE0000000 + L1_CACHE_SIZE)))
		{
			*(T*)&m_pL1Cache[em_address & L1_CACHE_MASK] = bswap(data);
			return;
		}
	}

	if (bFakeVMEM && (segment == 0x7 || segment == 0x4))
	{
		// fake VMEM
		*(T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK] = bswap(data);
		return;
	}

	// MMU: Do page table translation
	u32 tlb_addr = TranslateAddress<flag>(em_address);
	if (tlb_addr == 0)
	{
		if (flag == FLAG_WRITE)
			GenerateDSIException(em_address, true);
		return;
	}

	// Handle stores that cross page boundaries (ewwww)
	if (sizeof(T) > 1 && (em_address & (sizeof(T) - 1)) && (em_address & (HW_PAGE_SIZE - 1)) > HW_PAGE_SIZE - sizeof(T))
	{
		T val = bswap(data);

		// We need to check both addresses before writing in case there's a DSI.
		u32 em_address_next_page = (em_address + sizeof(T) - 1) & ~(HW_PAGE_SIZE - 1);
		u32 tlb_addr_next_page = TranslateAddress<flag>(em_address_next_page);
		if (tlb_addr_next_page == 0)
		{
			if (flag == FLAG_WRITE)
				GenerateDSIException(em_address_next_page, true);
			return;
		}
		for (u32 addr = em_address; addr < em_address + sizeof(T); addr++, tlb_addr++, val >>= 8)
		{
			if (addr == em_address_next_page)
				tlb_addr = tlb_addr_next_page;
			Memory::base[tlb_addr] = (u8)val;
		}
		return;
	}

	// The easy case!
	*(T*)&Memory::base[tlb_addr] = bswap(data);
}
// =====================


// =================================
/* These functions are primarily called by the Interpreter functions and are routed to the correct
   location through ReadFromHardware and WriteToHardware */
// ----------------

static void GenerateISIException(u32 effective_address);

u32 Read_Opcode(u32 address)
{
	if (address == 0x00000000)
	{
		// FIXME use assert?
		PanicAlert("Program tried to read an opcode from [00000000]. It has crashed.");
		return 0x00000000;
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU &&
		(address & ADDR_MASK_MEM1))
	{
		// TODO: Check for MSR instruction address translation flag before translating
		u32 tlb_addr = TranslateAddress<FLAG_OPCODE>(address);
		if (tlb_addr == 0)
		{
			GenerateISIException(address);
			return 0;
		}
		else
		{
			address = tlb_addr;
		}
	}

	return PowerPC::ppcState.iCache.ReadInstruction(address);
}

static __forceinline void Memcheck(u32 address, u32 var, bool write, int size)
{
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, var, address, write, size, PC);
	}
#endif
}

u8 Read_U8(const u32 address)
{
	u8 var = ReadFromHardware<FLAG_READ, u8>(address);
	Memcheck(address, var, false, 1);
	return (u8)var;
}

u16 Read_U16(const u32 address)
{
	u16 var = ReadFromHardware<FLAG_READ, u16>(address);
	Memcheck(address, var, false, 2);
	return (u16)var;
}

u32 Read_U32(const u32 address)
{
	u32 var = ReadFromHardware<FLAG_READ, u32>(address);
	Memcheck(address, var, false, 4);
	return var;
}

u64 Read_U64(const u32 address)
{
	u64 var = ReadFromHardware<FLAG_READ, u64>(address);
	Memcheck(address, (u32)var, false, 8);
	return var;
}

double Read_F64(const u32 address)
{
	union
	{
		u64 i;
		double d;
	} cvt;

	cvt.i = Read_U64(address);
	return cvt.d;
}

float Read_F32(const u32 address)
{
	union
	{
		u32 i;
		float d;
	} cvt;

	cvt.i = Read_U32(address);
	return cvt.d;
}

u32 Read_U8_ZX(const u32 address)
{
	return (u32)Read_U8(address);
}

u32 Read_U16_ZX(const u32 address)
{
	return (u32)Read_U16(address);
}

void Write_U8(const u8 var, const u32 address)
{
	Memcheck(address, var, true, 1);
	WriteToHardware<FLAG_WRITE, u8>(address, var);
}

void Write_U16(const u16 var, const u32 address)
{
	Memcheck(address, var, true, 2);
	WriteToHardware<FLAG_WRITE, u16>(address, var);
}
void Write_U16_Swap(const u16 var, const u32 address)
{
	Memcheck(address, var, true, 2);
	Write_U16(Common::swap16(var), address);
}


void Write_U32(const u32 var, const u32 address)
{
	Memcheck(address, var, true, 4);
	WriteToHardware<FLAG_WRITE, u32>(address, var);
}
void Write_U32_Swap(const u32 var, const u32 address)
{
	Memcheck(address, var, true, 4);
	Write_U32(Common::swap32(var), address);
}

void Write_U64(const u64 var, const u32 address)
{
	Memcheck(address, (u32)var, true, 8);
	WriteToHardware<FLAG_WRITE, u64>(address, var);
}
void Write_U64_Swap(const u64 var, const u32 address)
{
	Memcheck(address, (u32)var, true, 8);
	Write_U64(Common::swap64(var), address);
}

void Write_F64(const double var, const u32 address)
{
	union
	{
		u64 i;
		double d;
	} cvt;
	cvt.d = var;
	Write_U64(cvt.i, address);
}
u8 ReadUnchecked_U8(const u32 address)
{
	u8 var = ReadFromHardware<FLAG_NO_EXCEPTION, u8>(address);
	return var;
}


u32 ReadUnchecked_U32(const u32 address)
{
	u32 var = ReadFromHardware<FLAG_NO_EXCEPTION, u32>(address);
	return var;
}

void WriteUnchecked_U8(const u8 var, const u32 address)
{
	WriteToHardware<FLAG_NO_EXCEPTION, u8>(address, var);
}


void WriteUnchecked_U32(const u32 var, const u32 address)
{
	WriteToHardware<FLAG_NO_EXCEPTION, u32>(address, var);
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


#define PPC_EXC_DSISR_PAGE (1<<30)
#define PPC_EXC_DSISR_PROT (1<<27)
#define PPC_EXC_DSISR_STORE (1<<25)

#define SDR1_HTABORG(v) (((v)>>16)&0xffff)
#define SDR1_HTABMASK(v) ((v)&0x1ff)
#define SDR1_PAGETABLE_BASE(v) ((v)&0xffff)
#define SR_T  (1<<31)
#define SR_Ks (1<<30)
#define SR_Kp (1<<29)
#define SR_N  (1<<28)
#define SR_VSID(v)       ((v)&0xffffff)
#define SR_BUID(v)       (((v)>>20)&0x1ff)
#define SR_CNTRL_SPEC(v) ((v)&0xfffff)

#define EA_SR(v)         (((v)>>28)&0xf)
#define EA_PageIndex(v)  (((v)>>12)&0xffff)
#define EA_Offset(v)     ((v)&0xfff)
#define EA_API(v)        (((v)>>22)&0x3f)

#define PA_RPN(v)        (((v)>>12)&0xfffff)
#define PA_Offset(v)     ((v)&0xfff)

#define PTE1_V       (1<<31)
#define PTE1_VSID(v) (((v)>>7)&0xffffff)
#define PTE1_H       (1<<6)
#define PTE1_API(v)  ((v)&0x3f)

#define PTE2_RPN(v)  ((v)&0xfffff000)
#define PTE2_R       (1<<8)
#define PTE2_C       (1<<7)
#define PTE2_WIMG(v) (((v)>>3)&0xf)
#define PTE2_PP(v)   ((v)&3)

// Hey! these duplicate a structure in Gekko.h
union UPTE1
{
	struct
	{
		u32 API    : 6;
		u32 H      : 1;
		u32 VSID   : 24;
		u32 V      : 1;
	};
	u32 Hex;
};

union UPTE2
{
	struct
	{
		u32 PP     : 2;
		u32        : 1;
		u32 WIMG   : 4;
		u32 C      : 1;
		u32 R      : 1;
		u32        : 3;
		u32 RPN    : 20;
	};
	u32 Hex;
};

static void GenerateDSIException(u32 effectiveAddress, bool write)
{
	// DSI exceptions are only supported in MMU mode.
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
	{
		PanicAlertT("Invalid %s to 0x%08x, PC = 0x%08x ", write ? "Write to" : "Read from", effectiveAddress, PC);
		return;
	}

	if (effectiveAddress)
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
	else
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE;

	PowerPC::ppcState.spr[SPR_DAR] = effectiveAddress;

	Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_DSI);
}


static void GenerateISIException(u32 _EffectiveAddress)
{
	// Address of instruction could not be translated
	NPC = _EffectiveAddress;

	Common::AtomicOr(PowerPC::ppcState.Exceptions, EXCEPTION_ISI);
}


void SDRUpdated()
{
	u32 htabmask = SDR1_HTABMASK(PowerPC::ppcState.spr[SPR_SDR]);
	u32 x = 1;
	u32 xx = 0;
	int n = 0;
	while ((htabmask & x) && (n < 9))
	{
		n++;
		xx|=x;
		x<<=1;
	}
	if (htabmask & ~xx)
	{
		return;
	}
	u32 htaborg = SDR1_HTABORG(PowerPC::ppcState.spr[SPR_SDR]);
	if (htaborg & xx)
	{
		return;
	}
	PowerPC::ppcState.pagetable_base = htaborg<<16;
	PowerPC::ppcState.pagetable_hashmask = ((xx<<10)|0x3ff);
}

enum TLBLookupResult
{
	TLB_FOUND,
	TLB_NOTFOUND,
	TLB_UPDATE_C
};

static __forceinline TLBLookupResult LookupTLBPageAddress(const XCheckTLBFlag flag, const u32 vpa, u32 *paddr)
{
	int tag = vpa >> HW_PAGE_INDEX_SHIFT;
	PowerPC::tlb_entry *tlbe = &PowerPC::ppcState.tlb[flag == FLAG_OPCODE][tag & HW_PAGE_INDEX_MASK];
	if (tlbe->tag[0] == tag)
	{
		// Check if C bit requires updating
		if (flag == FLAG_WRITE)
		{
			UPTE2 PTE2;
			PTE2.Hex = tlbe->pte[0];
			if (PTE2.C == 0)
			{
				PTE2.C = 1;
				tlbe->pte[0] = PTE2.Hex;
				return TLB_UPDATE_C;
			}
		}

		if (flag != FLAG_NO_EXCEPTION)
			tlbe->recent = 0;

		*paddr = tlbe->paddr[0] | (vpa & 0xfff);

		return TLB_FOUND;
	}
	if (tlbe->tag[1] == tag)
	{
		// Check if C bit requires updating
		if (flag == FLAG_WRITE)
		{
			UPTE2 PTE2;
			PTE2.Hex = tlbe->pte[1];
			if (PTE2.C == 0)
			{
				PTE2.C = 1;
				tlbe->pte[1] = PTE2.Hex;
				return TLB_UPDATE_C;
			}
		}

		if (flag != FLAG_NO_EXCEPTION)
			tlbe->recent = 1;

		*paddr = tlbe->paddr[1] | (vpa & 0xfff);

		return TLB_FOUND;
	}
	return TLB_NOTFOUND;
}

static __forceinline void UpdateTLBEntry(const XCheckTLBFlag flag, UPTE2 PTE2, const u32 address)
{
	if (flag == FLAG_NO_EXCEPTION)
		return;

	int tag = address >> HW_PAGE_INDEX_SHIFT;
	PowerPC::tlb_entry *tlbe = &PowerPC::ppcState.tlb[flag == FLAG_OPCODE][tag & HW_PAGE_INDEX_MASK];
	int index = tlbe->recent == 0 && tlbe->tag[0] != TLB_TAG_INVALID;
	tlbe->recent = index;
	tlbe->paddr[index] = PTE2.RPN << HW_PAGE_INDEX_SHIFT;
	tlbe->pte[index] = PTE2.Hex;
	tlbe->tag[index] = tag;
}

void InvalidateTLBEntry(u32 address)
{
	PowerPC::tlb_entry *tlbe = &PowerPC::ppcState.tlb[0][(address >> HW_PAGE_INDEX_SHIFT) & HW_PAGE_INDEX_MASK];
	tlbe->tag[0] = TLB_TAG_INVALID;
	tlbe->tag[1] = TLB_TAG_INVALID;
	PowerPC::tlb_entry *tlbe_i = &PowerPC::ppcState.tlb[1][(address >> HW_PAGE_INDEX_SHIFT) & HW_PAGE_INDEX_MASK];
	tlbe_i->tag[0] = TLB_TAG_INVALID;
	tlbe_i->tag[1] = TLB_TAG_INVALID;
}

// Page Address Translation
static __forceinline u32 TranslatePageAddress(const u32 address, const XCheckTLBFlag flag)
{
	// TLB cache
	// This catches 99%+ of lookups in practice, so the actual page table entry code below doesn't benefit
	// much from optimization.
	u32 translatedAddress = 0;
	TLBLookupResult res = LookupTLBPageAddress(flag , address, &translatedAddress);
	if (res == TLB_FOUND)
		return translatedAddress;

	u32 sr = PowerPC::ppcState.sr[EA_SR(address)];

	u32 offset = EA_Offset(address);        // 12 bit
	u32 page_index = EA_PageIndex(address); // 16 bit
	u32 VSID = SR_VSID(sr);                  // 24 bit
	u32 api = EA_API(address);              //  6 bit (part of page_index)

	// Direct access to the fastmem Arena
	// FIXME: is this the best idea for clean code?
	u8* base_mem = Memory::base;

	// hash function no 1 "xor" .360
	u32 hash = (VSID ^ page_index);
	u32 pte1 = bswap((VSID << 7) | api | PTE1_V);

	for (int hash_func = 0; hash_func < 2; hash_func++)
	{
		// hash function no 2 "not" .360
		if (hash_func == 1)
		{
			hash = ~hash;
			pte1 |= PTE1_H << 24;
		}

		u32 pteg_addr = ((hash & PowerPC::ppcState.pagetable_hashmask) << 6) | PowerPC::ppcState.pagetable_base;

		for (int i = 0; i < 8; i++, pteg_addr += 8)
		{
			if (pte1 == *(u32*)&base_mem[pteg_addr])
			{
				UPTE2 PTE2;
				PTE2.Hex = bswap((*(u32*)&base_mem[(pteg_addr + 4)]));

				// set the access bits
				switch (flag)
				{
				case FLAG_NO_EXCEPTION: break;
				case FLAG_READ:     PTE2.R = 1; break;
				case FLAG_WRITE:    PTE2.R = 1; PTE2.C = 1; break;
				case FLAG_OPCODE:   PTE2.R = 1; break;
				}

				if (flag != FLAG_NO_EXCEPTION)
					*(u32*)&base_mem[(pteg_addr + 4)] = bswap(PTE2.Hex);

				// We already updated the TLB entry if this was caused by a C bit.
				if (res != TLB_UPDATE_C)
					UpdateTLBEntry(flag, PTE2, address);

				return (PTE2.RPN << 12) | offset;
			}
		}
	}
	return 0;
}

#define BATU_BEPI(v) ((v)&0xfffe0000)
#define BATU_BL(v)   (((v)&0x1ffc)>>2)
#define BATU_Vs      (1<<1)
#define BATU_Vp      (1)
#define BATL_BRPN(v) ((v)&0xfffe0000)

#define BAT_EA_OFFSET(v) ((v)&0x1ffff)
#define BAT_EA_11(v)     ((v)&0x0ffe0000)
#define BAT_EA_4(v)      ((v)&0xf0000000)

static inline bool CheckAddrBats(const u32 addr, u32* result, u32 batu, u32 spr)
{
	for (int i = 0; i < 4; i++)
	{
		u32 bl17 = ~(BATU_BL(PowerPC::ppcState.spr[spr + i * 2]) << 17);
		u32 addr2 = addr & (bl17 | 0xf001ffff);

		if (BATU_BEPI(addr2) == BATU_BEPI(PowerPC::ppcState.spr[spr + i * 2]))
		{
			// bat applies to this address
			if (PowerPC::ppcState.spr[spr + i * 2] & batu)
			{
				// bat entry valid
				u32 offset = BAT_EA_OFFSET(addr);
				u32 page = BAT_EA_11(addr);
				page &= ~bl17;
				page |= BATL_BRPN(PowerPC::ppcState.spr[spr + 1 + i * 2]);
				// fixme: check access rights
				*result = page | offset;
				return true;
			}
		}
	}
	return false;
}

// Block Address Translation
static u32 TranslateBlockAddress(const u32 address, const XCheckTLBFlag flag)
{
	u32 result = 0;
	UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
	u32 batu = (m_MSR.PR ? BATU_Vp : BATU_Vs);

	// Check for enhanced mode (secondary BAT enable) using 8 BATs
	bool enhanced_bats = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && HID4.SBE;

	if (flag != FLAG_OPCODE)
	{
		if (!CheckAddrBats(address, &result, batu, SPR_DBAT0U) && enhanced_bats)
			CheckAddrBats(address, &result, batu, SPR_DBAT4U);
	}
	else
	{
		if (!CheckAddrBats(address, &result, batu, SPR_IBAT0U) && enhanced_bats)
			CheckAddrBats(address, &result, batu, SPR_IBAT4U);
	}
	return result;
}

// Translate effective address using BAT or PAT.  Returns 0 if the address cannot be translated.
template <const XCheckTLBFlag flag>
u32 TranslateAddress(const u32 address)
{
	// Check MSR[IR] bit before translating instruction addresses.  Rogue Leader clears IR and DR??
	//if ((_Flag == FLAG_OPCODE) && !(MSR & (1 << (31 - 26)))) return _Address;

	// Check MSR[DR] bit before translating data addresses
	//if (((_Flag == FLAG_READ) || (_Flag == FLAG_WRITE)) && !(MSR & (1 << (31 - 27)))) return _Address;

	// Technically we should do this, but almost no games, even heavy MMU ones, use any custom BATs whatsoever,
	// so only do it where it's really needed.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bBAT)
	{
		u32 tlb_addr = TranslateBlockAddress(address, flag);
		if (tlb_addr)
			return tlb_addr;
	}
	return TranslatePageAddress(address, flag);
}

template u32 TranslateAddress<Memory::FLAG_NO_EXCEPTION>(const u32 address);
template u32 TranslateAddress<Memory::FLAG_READ>(const u32 address);
template u32 TranslateAddress<Memory::FLAG_WRITE>(const u32 address);
template u32 TranslateAddress<Memory::FLAG_OPCODE>(const u32 address);
} // namespace
