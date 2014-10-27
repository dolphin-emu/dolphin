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
// http://code.google.com/p/dolphin-emu/

#include "Common/Atomic.h"
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
inline u8 bswap(u8 val)   {return val;}
inline u16 bswap(u16 val) {return Common::swap16(val);}
inline u32 bswap(u32 val) {return Common::swap32(val);}
inline u64 bswap(u64 val) {return Common::swap64(val);}
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

template <typename T>
inline void ReadFromHardware(T &_var, const u32 em_address, const u32 effective_address, Memory::XCheckTLBFlag flag)
{
	// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
	if ((em_address & 0xC8000000) == 0xC8000000)
	{
		if (em_address < 0xcc000000)
			_var = EFB_Read(em_address);
		else
			_var = mmio_mapping->Read<T>(em_address);
	}
	else if (((em_address & 0xF0000000) == 0x80000000) ||
		((em_address & 0xF0000000) == 0xC0000000) ||
		((em_address & 0xF0000000) == 0x00000000))
	{
		_var = bswap((*(const T*)&m_pRAM[em_address & RAM_MASK]));
	}
	else if (m_pEXRAM && (((em_address & 0xF0000000) == 0x90000000) ||
		((em_address & 0xF0000000) == 0xD0000000) ||
		((em_address & 0xF0000000) == 0x10000000)))
	{
		_var = bswap((*(const T*)&m_pEXRAM[em_address & EXRAM_MASK]));
	}
	else if ((em_address >= 0xE0000000) && (em_address < (0xE0000000+L1_CACHE_SIZE)))
	{
		_var = bswap((*(const T*)&m_pL1Cache[em_address & L1_CACHE_MASK]));
	}
	else if ((bFakeVMEM && ((em_address &0xF0000000) == 0x70000000)) ||
		(bFakeVMEM && ((em_address &0xF0000000) == 0x40000000)))
	{
		// fake VMEM
		_var = bswap((*(const T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK]));
	}
	else
	{
		// MMU
		// Handle loads that cross page boundaries (ewwww)
		if (sizeof(T) > 1 && (em_address & (HW_PAGE_SIZE - 1)) > HW_PAGE_SIZE - sizeof(T))
		{
			_var = 0;
			// This could be unaligned down to the byte level... hopefully this is rare, so doing it this
			// way isn't too terrible.
			// TODO: floats on non-word-aligned boundaries should technically cause alignment exceptions.
			// Note that "word" means 32-bit, so paired singles or doubles might still be 32-bit aligned!
			u32 tlb_addr = TranslateAddress(em_address, flag);
			for (u32 addr = em_address; addr < em_address + sizeof(T); addr++, tlb_addr++)
			{
				// Start of the new page... translate the address again!
				if (!(addr & (HW_PAGE_SIZE-1)))
					tlb_addr = TranslateAddress(addr, flag);
				// Important: we need to generate the DSI on the first store that caused the fault, NOT
				// the address of the start of the load.
				if (tlb_addr == 0)
				{
					if (flag == FLAG_READ)
					{
						if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
							PanicAlertT("Invalid Read at 0x%08x, PC = 0x%08x ", em_address, PC);
						else
							GenerateDSIException(addr, false);
						break;
					}
				}
				else
				{
					if (m_pEXRAM && (tlb_addr & 0xF0000000) == 0x10000000)
					{
						_var <<= 8;
						_var |= m_pEXRAM[tlb_addr & EXRAM_MASK];
					}
					else
					{
						_var <<= 8;
						_var |= m_pRAM[tlb_addr & RAM_MASK];
					}
				}
			}
		}
		else
		{
			u32 tlb_addr = TranslateAddress(em_address, flag);
			if (tlb_addr == 0)
			{
				if (flag == FLAG_READ)
				{
					if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
						PanicAlertT("Invalid Read at 0x%08x, PC = 0x%08x ", em_address, PC);
					else
						GenerateDSIException(em_address, false);
				}
			}
			else
			{
				if (m_pEXRAM && (tlb_addr & 0xF0000000) == 0x10000000)
				{
					_var = bswap((*(const T*)&m_pEXRAM[tlb_addr & EXRAM_MASK]));
				}
				else
				{
					_var = bswap((*(const T*)&m_pRAM[tlb_addr & RAM_MASK]));
				}
			}
		}
	}
}


template <typename T>
inline void WriteToHardware(u32 em_address, const T data, u32 effective_address, Memory::XCheckTLBFlag flag)
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
				g_video_backend->Video_AccessEFB(POKE_COLOR, x, y,(u32)data);
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
	else if (((em_address & 0xF0000000) == 0x80000000) ||
		((em_address & 0xF0000000) == 0xC0000000) ||
		((em_address & 0xF0000000) == 0x00000000))
	{
		*(T*)&m_pRAM[em_address & RAM_MASK] = bswap(data);
		return;
	}
	else if (m_pEXRAM && (((em_address & 0xF0000000) == 0x90000000) ||
		((em_address & 0xF0000000) == 0xD0000000) ||
		((em_address & 0xF0000000) == 0x10000000)))
	{
		*(T*)&m_pEXRAM[em_address & EXRAM_MASK] = bswap(data);
		return;
	}
	else if ((em_address >= 0xE0000000) && (em_address < (0xE0000000+L1_CACHE_SIZE)))
	{
		*(T*)&m_pL1Cache[em_address & L1_CACHE_MASK] = bswap(data);
		return;
	}
	else if ((bFakeVMEM && ((em_address &0xF0000000) == 0x70000000)) ||
		(bFakeVMEM && ((em_address &0xF0000000) == 0x40000000)))
	{
		// fake VMEM
		*(T*)&m_pFakeVMEM[em_address & FAKEVMEM_MASK] = bswap(data);
	}
	else
	{
		// MMU
		// Handle stores that cross page boundaries (ewwww)
		if (sizeof(T) > 1 && (em_address & (HW_PAGE_SIZE-1)) > HW_PAGE_SIZE - sizeof(T))
		{
			T val = bswap(data);
			u32 tlb_addr = TranslateAddress(em_address, flag);
			for (u32 addr = em_address; addr < em_address + sizeof(T); addr++, tlb_addr++)
			{
				if (!(addr & (HW_PAGE_SIZE-1)))
					tlb_addr = TranslateAddress(addr, flag);
				if (tlb_addr == 0)
				{
					if (flag == FLAG_WRITE)
					{
						if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
							PanicAlertT("Invalid Write to 0x%08x, PC = 0x%08x ", em_address, PC);
						else
							GenerateDSIException(addr, true);
						break;
					}
				}
				else
				{
					if (m_pEXRAM && (tlb_addr & 0xF0000000) == 0x10000000)
					{
						m_pEXRAM[tlb_addr & EXRAM_MASK] = (u8)val;
						val >>= 8;
					}
					else
					{
						m_pRAM[tlb_addr & RAM_MASK] = (u8)val;
						val >>= 8;
					}
				}
			}
		}
		else
		{
			u32 tlb_addr = TranslateAddress(em_address, flag);
			if (tlb_addr == 0)
			{
				if (flag == FLAG_WRITE)
				{
					if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU)
						PanicAlertT("Invalid Write to 0x%08x, PC = 0x%08x ", em_address, PC);
					else
						GenerateDSIException(em_address, true);
				}
			}
			else
			{
				if (m_pEXRAM && (tlb_addr & 0xF0000000) == 0x10000000)
				{
					*(T*)&m_pEXRAM[tlb_addr & EXRAM_MASK] = bswap(data);
				}
				else
				{
					*(T*)&m_pRAM[tlb_addr & RAM_MASK] = bswap(data);
				}
			}
		}
	}
}
// =====================


// =================================
/* These functions are primarily called by the Interpreter functions and are routed to the correct
   location through ReadFromHardware and WriteToHardware */
// ----------------

static void GenerateISIException(u32 effective_address);

u32 Read_Opcode(u32 _Address)
{
	if (_Address == 0x00000000)
	{
		// FIXME use assert?
		PanicAlert("Program tried to read an opcode from [00000000]. It has crashed.");
		return 0x00000000;
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU &&
		(_Address & ADDR_MASK_MEM1))
	{
		// TODO: Check for MSR instruction address translation flag before translating
		u32 tlb_addr = Memory::TranslateAddress(_Address, FLAG_OPCODE);
		if (tlb_addr == 0)
		{
			GenerateISIException(_Address);
			return 0;
		}
		else
		{
			_Address = tlb_addr;
		}
	}

	return PowerPC::ppcState.iCache.ReadInstruction(_Address);
}

u8 Read_U8(const u32 _Address)
{
	u8 _var = 0;
	ReadFromHardware<u8>(_var, _Address, _Address, FLAG_READ);
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _var, _Address, false, 1, PC);
	}
#endif
	return (u8)_var;
}

u16 Read_U16(const u32 _Address)
{
	u16 _var = 0;
	ReadFromHardware<u16>(_var, _Address, _Address, FLAG_READ);
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _var, _Address, false, 2, PC);
	}
#endif
	return (u16)_var;
}

u32 Read_U32(const u32 _Address)
{
	u32 _var = 0;
	ReadFromHardware<u32>(_var, _Address, _Address, FLAG_READ);
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _var, _Address, false, 4, PC);
	}
#endif
	return _var;
}

u64 Read_U64(const u32 _Address)
{
	u64 _var = 0;
	ReadFromHardware<u64>(_var, _Address, _Address, FLAG_READ);
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, (u32)_var, _Address, false, 8, PC);
	}
#endif
	return _var;
}

double Read_F64(const u32 _Address)
{
	union
	{
		u64 i;
		double d;
	} cvt;

	cvt.i = Read_U64(_Address);
	return cvt.d;
}

float Read_F32(const u32 _Address)
{
	union
	{
		u32 i;
		float d;
	} cvt;

	cvt.i = Read_U32(_Address);
	return cvt.d;
}

u32 Read_U8_ZX(const u32 _Address)
{
	return (u32)Read_U8(_Address);
}

u32 Read_U16_ZX(const u32 _Address)
{
	return (u32)Read_U16(_Address);
}

void Write_U8(const u8 _Data, const u32 _Address)
{
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _Data,_Address,true,1,PC);
	}
#endif
	WriteToHardware<u8>(_Address, _Data, _Address, FLAG_WRITE);
}


void Write_U16(const u16 _Data, const u32 _Address)
{
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _Data,_Address,true,2,PC);
	}
#endif

	WriteToHardware<u16>(_Address, _Data, _Address, FLAG_WRITE);
}
void Write_U16_Swap(const u16 _Data, const u32 _Address)
{
	Write_U16(Common::swap16(_Data), _Address);
}


void Write_U32(const u32 _Data, const u32 _Address)
{
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, _Data,_Address,true,4,PC);
	}
#endif
	WriteToHardware<u32>(_Address, _Data, _Address, FLAG_WRITE);
}
void Write_U32_Swap(const u32 _Data, const u32 _Address)
{
	Write_U32(Common::swap32(_Data), _Address);
}

void Write_U64(const u64 _Data, const u32 _Address)
{
#ifdef ENABLE_MEM_CHECK
	TMemCheck *mc = PowerPC::memchecks.GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(&PowerPC::debug_interface, (u32)_Data,_Address,true,8,PC);
	}
#endif

	WriteToHardware<u64>(_Address, _Data, _Address + 4, FLAG_WRITE);
}
void Write_U64_Swap(const u64 _Data, const u32 _Address)
{
	Write_U64(Common::swap64(_Data), _Address);
}

void Write_F64(const double _Data, const u32 _Address)
{
	union
	{
		u64 i;
		double d;
	} cvt;
	cvt.d = _Data;
	Write_U64(cvt.i, _Address);
}
u8 ReadUnchecked_U8(const u32 _Address)
{
	u8 _var = 0;
	ReadFromHardware<u8>(_var, _Address, _Address, FLAG_NO_EXCEPTION);
	return _var;
}


u32 ReadUnchecked_U32(const u32 _Address)
{
	u32 _var = 0;
	ReadFromHardware<u32>(_var, _Address, _Address, FLAG_NO_EXCEPTION);
	return _var;
}

void WriteUnchecked_U8(const u8 _iValue, const u32 _Address)
{
	WriteToHardware<u8>(_Address, _iValue, _Address, FLAG_NO_EXCEPTION);
}


void WriteUnchecked_U32(const u32 _iValue, const u32 _Address)
{
	WriteToHardware<u32>(_Address, _iValue, _Address, FLAG_NO_EXCEPTION);
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

static void GenerateDSIException(u32 _EffectiveAddress, bool _bWrite)
{
	if (_bWrite)
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
	else
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE;

	PowerPC::ppcState.spr[SPR_DAR] = _EffectiveAddress;

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


// TLB cache
#define TLB_SIZE 128
#define TLB_WAYS 2
#define NUM_TLBS 2

#define HW_PAGE_INDEX_SHIFT 12
#define HW_PAGE_INDEX_MASK 0x3f
#define HW_PAGE_TAG_SHIFT 18

#define TLB_FLAG_MOST_RECENT 0x01
#define TLB_FLAG_INVALID 0x02

struct tlb_entry
{
	u32 tag;
	u32 paddr;
	u8 flags;
};

// TODO: tlb needs to be in ppcState for save-state purposes.
static tlb_entry tlb[NUM_TLBS][TLB_SIZE/TLB_WAYS][TLB_WAYS];

static u32 LookupTLBPageAddress(const XCheckTLBFlag _Flag, const u32 vpa, u32 *paddr)
{
	tlb_entry *tlbe = tlb[_Flag == FLAG_OPCODE][(vpa>>HW_PAGE_INDEX_SHIFT)&HW_PAGE_INDEX_MASK];
	if (tlbe[0].tag == (vpa & ~0xfff) && !(tlbe[0].flags & TLB_FLAG_INVALID))
	{
		if (_Flag != FLAG_NO_EXCEPTION)
		{
			tlbe[0].flags |= TLB_FLAG_MOST_RECENT;
			tlbe[1].flags &= ~TLB_FLAG_MOST_RECENT;
		}
		*paddr = tlbe[0].paddr | (vpa & 0xfff);
		return 1;
	}
	if (tlbe[1].tag == (vpa & ~0xfff) && !(tlbe[1].flags & TLB_FLAG_INVALID))
	{
		if (_Flag != FLAG_NO_EXCEPTION)
		{
			tlbe[1].flags |= TLB_FLAG_MOST_RECENT;
			tlbe[0].flags &= ~TLB_FLAG_MOST_RECENT;
		}
		*paddr = tlbe[1].paddr | (vpa & 0xfff);
		return 1;
	}
	return 0;
}

static void UpdateTLBEntry(const XCheckTLBFlag _Flag, UPTE2 PTE2, const u32 vpa)
{
	if (_Flag != FLAG_NO_EXCEPTION)
		return;

	tlb_entry *tlbe = tlb[_Flag == FLAG_OPCODE][(vpa>>HW_PAGE_INDEX_SHIFT)&HW_PAGE_INDEX_MASK];
	if ((tlbe[0].flags & TLB_FLAG_MOST_RECENT) == 0)
	{
		tlbe[0].flags = TLB_FLAG_MOST_RECENT;
		tlbe[1].flags &= ~TLB_FLAG_MOST_RECENT;
		tlbe[0].paddr = PTE2.RPN << HW_PAGE_INDEX_SHIFT;
		tlbe[0].tag = vpa & ~0xfff;
	}
	else
	{
		tlbe[1].flags = TLB_FLAG_MOST_RECENT;
		tlbe[0].flags &= ~TLB_FLAG_MOST_RECENT;
		tlbe[1].paddr = PTE2.RPN << HW_PAGE_INDEX_SHIFT;
		tlbe[1].tag = vpa & ~0xfff;
	}
}

void InvalidateTLBEntry(u32 vpa)
{
	tlb_entry *tlbe = tlb[0][(vpa>>HW_PAGE_INDEX_SHIFT)&HW_PAGE_INDEX_MASK];
	if (tlbe[0].tag == (vpa & ~0xfff))
	{
		tlbe[0].flags |= TLB_FLAG_INVALID;
	}
	if (tlbe[1].tag == (vpa & ~0xfff))
	{
		tlbe[1].flags |= TLB_FLAG_INVALID;
	}
	tlb_entry *tlbe_i = tlb[1][(vpa>>HW_PAGE_INDEX_SHIFT)&HW_PAGE_INDEX_MASK];
	if (tlbe_i[0].tag == (vpa & ~0xfff))
	{
		tlbe_i[0].flags |= TLB_FLAG_INVALID;
	}
	if (tlbe_i[1].tag == (vpa & ~0xfff))
	{
		tlbe_i[1].flags |= TLB_FLAG_INVALID;
	}
}

// Page Address Translation
static u32 TranslatePageAddress(const u32 _Address, const XCheckTLBFlag _Flag)
{
	// TLB cache
	u32 translatedAddress = 0;
	if (LookupTLBPageAddress(_Flag, _Address, &translatedAddress))
		return translatedAddress;

	u32 sr = PowerPC::ppcState.sr[EA_SR(_Address)];

	u32 offset = EA_Offset(_Address);        // 12 bit
	u32 page_index = EA_PageIndex(_Address); // 16 bit
	u32 VSID = SR_VSID(sr);                  // 24 bit
	u32 api = EA_API(_Address);              //  6 bit (part of page_index)

	u8* pRAM = GetPointer(0);

	// hash function no 1 "xor" .360
	u32 hash1 = (VSID ^ page_index);
	u32 pteg_addr = ((hash1 & PowerPC::ppcState.pagetable_hashmask) << 6) | PowerPC::ppcState.pagetable_base;

	// hash1
	for (int i = 0; i < 8; i++)
	{
		UPTE1 PTE1;
		PTE1.Hex = bswap(*(u32*)&pRAM[pteg_addr]);

		if (PTE1.V && !PTE1.H)
		{
			if (VSID == PTE1.VSID && (api == PTE1.API))
			{
				UPTE2 PTE2;
				PTE2.Hex = bswap((*(u32*)&pRAM[(pteg_addr + 4)]));

				UpdateTLBEntry(_Flag, PTE2, _Address);

				// set the access bits
				switch (_Flag)
				{
				case FLAG_READ:     PTE2.R = 1; break;
				case FLAG_WRITE:    PTE2.C = 1; break;
				case FLAG_NO_EXCEPTION: break;
				case FLAG_OPCODE: break;
				}
				*(u32*)&pRAM[(pteg_addr + 4)] = bswap(PTE2.Hex);

				return ((PTE2.RPN << 12) | offset);
			}
		}
		pteg_addr+=8;
	}

	// hash function no 2 "not" .360
	hash1 = ~hash1;
	pteg_addr = ((hash1 & PowerPC::ppcState.pagetable_hashmask) << 6) | PowerPC::ppcState.pagetable_base;
	for (int i = 0; i < 8; i++)
	{
		u32 pte = bswap(*(u32*)&pRAM[pteg_addr]);
		if ((pte & PTE1_V) && (pte & PTE1_H))
		{
			if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte)))
			{
				UPTE2 PTE2;
				PTE2.Hex = bswap((*(u32*)&pRAM[(pteg_addr + 4)]));

				UpdateTLBEntry(_Flag, PTE2, _Address);

				switch (_Flag)
				{
				case FLAG_READ:     PTE2.R = 1; break;
				case FLAG_WRITE:    PTE2.C = 1; break;
				case FLAG_NO_EXCEPTION: break;
				case FLAG_OPCODE: break;
				}
				*(u32*)&pRAM[(pteg_addr + 4)] = bswap(PTE2.Hex);

				return ((PTE2.RPN << 12) | offset);
			}
		}
		pteg_addr+=8;
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
static u32 TranslateBlockAddress(const u32 addr, const XCheckTLBFlag _Flag)
{
	u32 result = 0;
	UReg_MSR& m_MSR = ((UReg_MSR&)PowerPC::ppcState.msr);
	u32 batu = (m_MSR.PR ? BATU_Vp : BATU_Vs);

	// Check for enhanced mode (secondary BAT enable) using 8 BATs
	bool enhanced_bats = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && HID4.SBE;

	if (_Flag != FLAG_OPCODE)
	{
		if (!CheckAddrBats(addr, &result, batu, SPR_DBAT0U) && enhanced_bats)
			CheckAddrBats(addr, &result, batu, SPR_DBAT4U);
	}
	else
	{
		if (!CheckAddrBats(addr, &result, batu, SPR_IBAT0U) && enhanced_bats)
			CheckAddrBats(addr, &result, batu, SPR_IBAT4U);
	}
	return result;
}

// Translate effective address using BAT or PAT.  Returns 0 if the address cannot be translated.
u32 TranslateAddress(const u32 _Address, const XCheckTLBFlag _Flag)
{
	// Check MSR[IR] bit before translating instruction addresses.  Rogue Leader clears IR and DR??
	//if ((_Flag == FLAG_OPCODE) && !(MSR & (1 << (31 - 26)))) return _Address;

	// Check MSR[DR] bit before translating data addresses
	//if (((_Flag == FLAG_READ) || (_Flag == FLAG_WRITE)) && !(MSR & (1 << (31 - 27)))) return _Address;

	// Technically we should do this, but almost no games, even heavy MMU ones, use any custom BATs whatsoever,
	// so only do it where it's really needed.
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bBAT)
	{
		u32 tlb_addr = TranslateBlockAddress(_Address, _Flag);
		if (tlb_addr)
			return tlb_addr;
	}
	return TranslatePageAddress(_Address, _Flag);
}
} // namespace
