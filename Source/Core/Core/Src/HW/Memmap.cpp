// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Common.h"
#include "MemoryUtil.h"
#include "MemArena.h"
#include "ChunkFile.h"

#include "Memmap.h"
#include "../Core.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/Jit64/JitCache.h"
#include "CPU.h"
#include "PeripheralInterface.h"
#include "DSP.h"
#include "DVDInterface.h"
#include "GPFifo.h"
#include "VideoInterface.h"
#include "SerialInterface.h"
#include "EXI.h"
#include "PixelEngine.h"
#include "CommandProcessor.h"
#include "AudioInterface.h"
#include "MemoryInterface.h"
#include "WII_IOB.h"
#include "WII_IPC.h"

#include "../Debugger/Debugger_BreakPoints.h"
#include "../Debugger/Debugger_SymbolMap.h"

namespace Memory
{

// GLOBAL DEFINES

// #define NOCHECK

// TLBHack = 1 in a patch ini will set this to true.
static bool bFakeVMEM = false;

#ifndef LOGGING
	#define NOCHECK
#endif

// END: GLOBAL DEFINES


u8*	base = NULL;

// STATE_TO_SAVE (applies to a lot of things in this file)

// Pointers to low memory
u8*	m_pRAM = NULL;
u8*	m_pFakeVMEM = NULL;
u8*	m_pEXRAM = NULL; //wii
u8*	m_pEFB = NULL;
u8*	m_pL1Cache = NULL;
bool m_IsInitialized = false;

MemArena g_arena;

// Pointers into the "View"  (rarely used)
u8* m_pPhysicalFakeVMEM;
u8* m_pPhysicalRAM;
u8* m_pPhysicalEXRAM; //wii
u8* m_pVirtualCachedRAM;
u8* m_pVirtualUncachedRAM;
u8* m_pVirtualCachedEXRAM;
u8* m_pVirtualUncachedEXRAM;
u8* m_pVirtualEFB;
u8* m_pVirtualL1Cache;

#define NUMHWMEMFUN 64
#define HWSHIFT 10
#define HW_MASK 0x3FF

writeFn8  hwWrite8 [NUMHWMEMFUN];
writeFn16 hwWrite16[NUMHWMEMFUN];
writeFn32 hwWrite32[NUMHWMEMFUN];
writeFn64 hwWrite64[NUMHWMEMFUN];

readFn8   hwRead8 [NUMHWMEMFUN];
readFn16  hwRead16[NUMHWMEMFUN];
readFn32  hwRead32[NUMHWMEMFUN];
readFn64  hwRead64[NUMHWMEMFUN];

writeFn8  hwWriteWii8 [NUMHWMEMFUN];
writeFn16 hwWriteWii16[NUMHWMEMFUN];
writeFn32 hwWriteWii32[NUMHWMEMFUN];
writeFn64 hwWriteWii64[NUMHWMEMFUN];

readFn8   hwReadWii8 [NUMHWMEMFUN];
readFn16  hwReadWii16[NUMHWMEMFUN];
readFn32  hwReadWii32[NUMHWMEMFUN];
readFn64  hwReadWii64[NUMHWMEMFUN];


inline u8 bswap(u8 val) {return val;}
inline u16 bswap(u16 val) {return Common::swap16(val);}
inline u32 bswap(u32 val) {return Common::swap32(val);}
inline u64 bswap(u64 val) {return Common::swap64(val);}

u32 CheckDTLB(u32 _Address, XCheckTLBFlag _Flag);

template <class T>
void HWCALL HW_Default_Write(const T _Data, const u32 _Address){	LOG(MASTER_LOG, "Illegal HW Write%i %08x", sizeof(T)*8, _Address);_dbg_assert_(MEMMAP, 0);}

template <class T>
void HWCALL HW_Default_Read(T _Data, const u32 _Address){	LOG(MASTER_LOG, "Illegal HW Read%i %08x", sizeof(T)*8, _Address); _dbg_assert_(MEMMAP, 0);}

#define PAGE_SHIFT 10
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SHIFT - 1)


template <class T, u8* P> void HWCALL HW_Read_Memory(T &_Data, const u32 _Address)	{	_Data = *(T*)&P[_Address & PAGE_MASK];	}
template <class T, u8* P> void HWCALL HW_Write_Memory(T _Data, const u32 _Address)	{	*(T*)&P[_Address & PAGE_MASK] = _Data;	}


#define BLOCKSIZE 4
#define CP_START		0x00  //0x0000 >> 10
#define WII_IPC_START	0x00  //0x0000 >> 10
#define PE_START		0x04  //0x1000 >> 10
#define VI_START		0x08  //0x2000 >> 10
#define PI_START		0x0C  //0x3000 >> 10
#define MI_START		0x10  //0x4000 >> 10
#define DSP_START		0x14  //0x5000 >> 10
#define DVD_START		0x18  //0x6000 >> 10
#define SI_START		0x19
#define EI_START		0x1A
#define AUDIO_START		0x1B 
#define GP_START		0x20 

void InitHWMemFuncs()
{
	for (int i = 0; i < NUMHWMEMFUN; i++)
	{
		hwWrite8 [i] = HW_Default_Write<u8>;
		hwWrite16[i] = HW_Default_Write<u16>;
		hwWrite32[i] = HW_Default_Write<u32>;
		hwWrite64[i] = HW_Default_Write<u64>;
		hwRead8  [i] = HW_Default_Read<u8&>;
		hwRead16 [i] = HW_Default_Read<u16&>;
		hwRead32 [i] = HW_Default_Read<u32&>;
		hwRead64 [i] = HW_Default_Read<u64&>;

		// To prevent Dolphin from crashing when running Wii executables in Gc mode.
		hwWriteWii8 [i] = HW_Default_Write<u8>;
		hwWriteWii16[i] = HW_Default_Write<u16>;
		hwWriteWii32[i] = HW_Default_Write<u32>;
		hwWriteWii64[i] = HW_Default_Write<u64>;
		hwReadWii8  [i] = HW_Default_Read<u8&>;
		hwReadWii16 [i] = HW_Default_Read<u16&>;
		hwReadWii32 [i] = HW_Default_Read<u32&>;
		hwReadWii64 [i] = HW_Default_Read<u64&>;
	}

	for (int i = 0; i < BLOCKSIZE; i++)
	{
		hwRead16 [CP_START+i] = CommandProcessor::Read16;
		hwWrite16[CP_START+i] = CommandProcessor::Write16;
 
		hwRead16 [PE_START+i] = PixelEngine::Read16;
		hwWrite16[PE_START+i] = PixelEngine::Write16;
		hwWrite32[PE_START+i] = PixelEngine::Write32;

		hwRead16 [VI_START+i] = VideoInterface::Read16;
		hwRead32 [VI_START+i] = VideoInterface::Read32;
		hwWrite16[VI_START+i] = VideoInterface::Write16;
		hwWrite32[VI_START+i] = VideoInterface::Write32;

		hwRead32 [PI_START+i] = CPeripheralInterface::Read32;
		hwWrite32[PI_START+i] = CPeripheralInterface::Write32;

		hwRead16 [MI_START+i] = MemoryInterface::Read16;
		hwRead32 [MI_START+i] = MemoryInterface::Read32;
		hwWrite32[MI_START+i] = MemoryInterface::Write32;
		hwWrite16[MI_START+i] = MemoryInterface::Write16;

		hwRead16 [DSP_START+i] = DSP::Read16;
		hwWrite16[DSP_START+i] = DSP::Write16;
		hwRead32 [DSP_START+i] = DSP::Read32;
		hwWrite32[DSP_START+i] = DSP::Write32;
	}

	hwRead32 [DVD_START] = DVDInterface::Read32;
	hwWrite32[DVD_START] = DVDInterface::Write32;

	hwRead32 [SI_START] = SerialInterface::Read32;
	hwWrite32[SI_START] = SerialInterface::Write32;

	hwRead32 [EI_START] = ExpansionInterface::Read32;
	hwWrite32[EI_START] = ExpansionInterface::Write32;

	hwRead32 [AUDIO_START] = AudioInterface::Read32;
	hwWrite32[AUDIO_START] = AudioInterface::Write32;

	hwWrite8 [GP_START] = GPFifo::Write8;
	hwWrite16[GP_START] = GPFifo::Write16;
	hwWrite32[GP_START] = GPFifo::Write32;
}


void InitHWMemFuncsWii()
{
	for (int i = 0; i < NUMHWMEMFUN; i++)
	{
		hwWrite8 [i] = HW_Default_Write<u8>;
		hwWrite16[i] = HW_Default_Write<u16>;
		hwWrite32[i] = HW_Default_Write<u32>;
		hwWrite64[i] = HW_Default_Write<u64>;
		hwRead8  [i] = HW_Default_Read<u8&>;
		hwRead16 [i] = HW_Default_Read<u16&>;
		hwRead32 [i] = HW_Default_Read<u32&>;
		hwRead64 [i] = HW_Default_Read<u64&>;

		hwWriteWii8 [i] = HW_Default_Write<u8>;
		hwWriteWii16[i] = HW_Default_Write<u16>;
		hwWriteWii32[i] = HW_Default_Write<u32>;
		hwWriteWii64[i] = HW_Default_Write<u64>;
		hwReadWii8  [i] = HW_Default_Read<u8&>;
		hwReadWii16 [i] = HW_Default_Read<u16&>;
		hwReadWii32 [i] = HW_Default_Read<u32&>;
		hwReadWii64 [i] = HW_Default_Read<u64&>;
	}

	// MI, PI, DSP are still mapped to 0xCCxxxxxx
	for (int i = 0; i < BLOCKSIZE; i++)
	{
		hwRead16 [CP_START+i] = CommandProcessor::Read16;
		hwWrite16[CP_START+i] = CommandProcessor::Write16;

		hwRead16 [PE_START+i] = PixelEngine::Read16;
		hwWrite16[PE_START+i] = PixelEngine::Write16;
		hwWrite32[PE_START+i] = PixelEngine::Write32;

		hwRead32 [PI_START+i] = CPeripheralInterface::Read32;
		hwWrite32[PI_START+i] = CPeripheralInterface::Write32;

		hwRead16 [VI_START+i] = VideoInterface::Read16;
		hwRead32 [VI_START+i] = VideoInterface::Read32;
		hwWrite16[VI_START+i] = VideoInterface::Write16;
		hwWrite32[VI_START+i] = VideoInterface::Write32;

		hwRead16 [MI_START+i] = MemoryInterface::Read16;
		hwRead32 [MI_START+i] = MemoryInterface::Read32;
		hwWrite32[MI_START+i] = MemoryInterface::Write32;
		hwWrite16[MI_START+i] = MemoryInterface::Write16;

		hwRead16 [DSP_START+i] = DSP::Read16;
		hwWrite16[DSP_START+i] = DSP::Write16;
		hwRead32 [DSP_START+i] = DSP::Read32;
		hwWrite32[DSP_START+i] = DSP::Write32;
	}

	hwWrite8 [GP_START] = GPFifo::Write8;
	hwWrite16[GP_START] = GPFifo::Write16;
	hwWrite32[GP_START] = GPFifo::Write32;

	for (int i = 0; i < BLOCKSIZE; i++)
	{
		hwReadWii32[WII_IPC_START+i] = WII_IPCInterface::Read32;
		hwWriteWii32[WII_IPC_START+i] = WII_IPCInterface::Write32;
	}

	hwReadWii32 [DVD_START] = DVDInterface::Read32;
	hwWriteWii32[DVD_START] = DVDInterface::Write32;

	hwReadWii32 [SI_START] = SerialInterface::Read32;
	hwWriteWii32[SI_START] = SerialInterface::Write32;

	hwReadWii32 [EI_START] = ExpansionInterface::Read32;
	hwWriteWii32[EI_START] = ExpansionInterface::Write32;

	// [F|RES] i thought this doesn't exist anymore
	hwReadWii32 [AUDIO_START] = AudioInterface::Read32;			
	hwWriteWii32[AUDIO_START] = AudioInterface::Write32;
}


writeFn32 GetHWWriteFun32(const u32 _Address)
{
	return hwWrite32[(_Address >> HWSHIFT) & (NUMHWMEMFUN-1)];
}






// =======================================================
/* Functions to detect and trace memory read/write errors. Turn of JIT LoadStore to
   make it work, and add a return 0 at the beginning of CheckDTLB to avoid closing
   Dolphin just as it starts to get interesting. You can also try to change
   the TitleID write in IOCTL_ES_GETTITLEID to 0x00000000, otherwise it will never even
   get to making the bad dev/di request.
   
   I'm currently at (---, 8021347c) : Write32: Program wrote [0x7fd5d340] to [0x933e00f8],
   0x8021347c seems to write the out buffer to a 0x933e.... address before it is copies
   to the 0x133e.... address for the Ioctlv. But why does it generate this bad address
   when it made a good one 120 milliseconds earlier?
   */
// -------------
bool ValidMemory(const u32 _Address)
{
	switch (_Address >> 24)
	{
	case 0x00:
	case 0x01:
	case 0x80:
	case 0x81:
	case 0xC0:
	case 0xC1:
		return true;

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
		if (Core::GetStartupParameter().bWii)
			return true;
		else
			return false;
	case 0x7e:
	case 0x7f:
		if (bFakeVMEM)
			return true;
		else
			return false;

	case 0xE0:
		if (_Address < (0xE0000000 + L1_CACHE_SIZE))
			return true;
		else
			return false;
	case 0xCC:
	case 0xCD:
	case 0xC8:
		return true;
	}
	return false;
}

void CheckForBadAddresses(u32 Address, u32 Data, bool Read, int Bits)
{
	if (!ValidMemory(Address))											
	{		
		if(Read)
		{
			LOG(CONSOLE, "Read%i: Program tried to read [%08x] from [%08x]", Bits, Address);		
			//PanicAlert("Write_U32: Program tried to write [%08x] to [%08x]", _Address);	
		}
		else
		{
			LOGV(CONSOLE, 0, "Write%i: Program tried to write [%08x] to [%08x]", Bits, Data, Address);	
			//PanicAlert("Read: Program tried to write [%08x] to [%08x]", Data, Address);
		}
	}

	if (Address == 0)																		
	{		
		if(Read)
		{
			LOGV(CONSOLE, 1, "Read%i: Program read [0x%08x] from [0x%08x]     * * *   0   * * *", Bits, Data, Address);	
			//PanicAlert("Read: Program read [%08x] from [%08x]", Data, Address);
		}
		else
		{
			LOGV(CONSOLE, 1, "Write%i: Program wrote [0x%08x] to [0x%08x]     * * *   0   * * *", Bits, Data, Address);	
			//PanicAlert("Read: Program wrote [%08x] to [%08x]", Data, Address);
		}
	}
/* Try to figure out where the dev/di Ioctl arguments are stored (including buffer out), so we can
   find the bad one */
	if(	
		Data == 0x1090f4c0 // good out buffer right before it, for sound/smashbros_sound.brsar
		|| Data == 0x10913b00 // second one
		|| Data == 0x7fd5d340 // first bad out buffer
		|| Data == 0x133e00f8 // the address that store the bad 0x7fd5d340, this changes every time
		|| Data == 0x2a24aa // menu2\sc_title_en.pac byte size
		|| (
			(PC == 0x8021347c || PC == 0x801f6a20  || PC == 0x800202d0  || PC == 0x80229964 
			|| PC == 0x801d88bc) /* this could be interesting, because the bad out buffer 0x7fd5d340
			is 0x80000000 - size = 0x7fd5d340 perhaps some function read 0x80000000, I dunno */
			&& Data == 0x80000000)
		)																		
	{		
		if(Read)
		{
			LOGV(CONSOLE, 0, "Read%i: Program read [0x%08x] from [0x%08x]      * * * * * * * * * * * *", Bits, Data, Address);	
			//PanicAlert("Read%i: Program read [%08x] from [%08x]", Bits, Data, Address);
		}
		else
		{
			LOGV(CONSOLE, 0, "Write%i: Program wrote [0x%08x] to [0x%08x]      * * * * * * * * * * * *", Bits,Data, Address);	
			//PanicAlert("Write%i: Program wrote [0x%08x] to [0x%08x]", Bits, Data, Address);
		}
	}
}


void CheckForBadAddresses8(u32 Address, u8 Data, bool Read)
{CheckForBadAddresses(Address, (u32)Data, Read, 8);}

void CheckForBadAddresses16(u32 Address, u16 Data, bool Read)
{CheckForBadAddresses(Address, (u32)Data, Read, 16);}

void CheckForBadAddresses32(u32 Address, u32 Data, bool Read)
{CheckForBadAddresses(Address, (u32)Data, Read, 32);}

void CheckForBadAddresses64(u32 Address, u64 Data, bool Read)
{CheckForBadAddresses(Address, (u32)Data, Read, 64);}
// =============






#define ReadFromHardware2(_var, _type, _Address, EffectiveAddress, flag)				\
{																						\
	if ((_Address & 0xC8000000) == 0xC8000000)											\
		if (_Address < 0xcc000000)														\
		{                                                                               \
			_var = bswap((*(u##_type*)&m_pEFB[_Address & EFB_MASK]));					\
		}                                                                               \
		else if (_Address <= 0xcc009000)												\
			hwRead##_type[(_Address>>HWSHIFT) & (NUMHWMEMFUN-1)](_var, _Address);		\
		/* WIIMODE */																	\
		else if (((_Address & 0xFF000000) == 0xCD000000) &&								\
				(_Address <= 0xcd009000))  												\
			hwReadWii##_type[(_Address>>HWSHIFT) & (NUMHWMEMFUN-1)](_var, _Address);	\
		else if (((_Address & 0xFFF00000) == 0xCD800000) &&								\
					(_Address <= 0xCD809000))											\
			WII_IOBridge::Read##_type(_var, _Address);									\
		else																			\
		{																				\
			/* Disabled because the debugger makes trouble with */						\
			/*_dbg_assert_(MEMMAP,0); */												\
		}																				\
	else if (((_Address & 0xF0000000) == 0x80000000) ||									\
			((_Address & 0xF0000000) == 0xC0000000) ||									\
			((_Address & 0xF0000000) == 0x00000000))									\
		_var = bswap((*(u##_type*)&m_pRAM[_Address & RAM_MASK]));						\
	else if (((_Address & 0xF0000000) == 0x90000000) ||									\
			((_Address & 0xF0000000) == 0xD0000000) ||                                  \
            ((_Address & 0xF0000000) == 0x10000000)) 									\
		_var = bswap((*(u##_type*)&m_pEXRAM[_Address & EXRAM_MASK]));					\
	else if ((_Address >= 0xE0000000) && (_Address < (0xE0000000+L1_CACHE_SIZE)))		\
	{																					\
		_var = bswap((*(u##_type*)&m_pL1Cache[_Address & L1_CACHE_MASK]));				\
	}																					\
	else if (_Address >= 0xE0000000)													\
		PanicAlert("READ: Invalid address: %08x", _Address);							\
	else																				\
	{																					\
		if (bFakeVMEM && ((_Address & 0xFE000000) == 0x7e000000) )	                    \
		{                                                                               \
			_var = bswap((*(u##_type*)&m_pFakeVMEM[_Address & FAKEVMEM_MASK]));			\
		}                                                                               \
		else {/* LOG(MEMMAP,"READ (unknown): %08x (PC: %08x)",_Address,PC);*/			\
			/*CCPU::EnableStepping(TRUE);*/												\
		/*PanicAlert("READ: Unknown Address", "1", MB_OK);*/							\
		u32 tmpAddress = CheckDTLB(EffectiveAddress, flag);								\
		tmpAddress =(tmpAddress & 0xFFFFFFF0) | (_Address & 0xF);						\
		if (!(PowerPC::ppcState.Exceptions & EXCEPTION_DSI))							\
		_var = bswap((*(u##_type*)&m_pRAM[tmpAddress & RAM_MASK]));						\
		}                                                                               \
	}																					\
	/* Uncomment this: CheckForBadAddresses##_type(_Address, _var, true);*/				\
}


#define WriteToHardware2(_type, _Address, _Data, EffectiveAddress, flag)				\
{																						\
	/* Uncomment this: CheckForBadAddresses##_type(_Address, _Data, false);*/			\
	if ((_Address & 0xC8000000) == 0xC8000000)											\
	{																					\
		if (_Address < 0xcc000000)														\
		{                                                                               \
			*(u##_type*)&m_pEFB[_Address & EFB_MASK] = bswap(_Data);	                \
			return;                                                                     \
		}                                                                               \
		else if (_Address <= 0xcc009000) {												\
			hwWrite##_type[(_Address>>HWSHIFT) & (NUMHWMEMFUN-1)](_Data,_Address);		\
			return;																		\
		}																				\
		/* WIIMODE */																	\
		else if (((_Address & 0xFF000000) == 0xCD000000) &&								\
				(_Address <= 0xcd009000)) {												\
			hwWriteWii##_type[(_Address>>HWSHIFT) & (NUMHWMEMFUN-1)](_Data,_Address);	\
			return;																		\
		}																				\
		else if (((_Address & 0xFFF00000) == 0xCD800000) &&								\
				(_Address <= 0xCD809000)) {												\
			WII_IOBridge::Write##_type(_Data,_Address);									\
			return;																		\
		}																				\
		else {																			\
			LOG(MEMMAP, "hwwrite [%08x] := %08x (PC: %08x)", _Address, _Data, PC);		\
			_dbg_assert_msg_(MEMMAP,0,"Memory - Unknown HW address %08x", _Address);	\
		} 																				\
	}																					\
	else if (((_Address & 0xF0000000) == 0x80000000) ||									\
			((_Address & 0xF0000000) == 0xC0000000) ||									\
			((_Address & 0xF0000000) == 0x00000000))									\
	{																					\
		*(u##_type*)&m_pRAM[_Address & RAM_MASK] = bswap(_Data);						\
		return;																			\
	}																					\
	else if (((_Address & 0xF0000000) == 0x90000000) ||									\
			((_Address & 0xF0000000) == 0xD0000000) ||									\
            ((_Address & 0xF0000000) == 0x10000000))                                    \
	{																					\
		*(u##_type*)&m_pEXRAM[_Address & EXRAM_MASK] = bswap(_Data);					\
		return;																			\
	}																					\
	else if ((_Address >= 0xE0000000) && (_Address < (0xE0000000+L1_CACHE_SIZE)))		\
	{																					\
		*(u##_type*)&m_pL1Cache[_Address & L1_CACHE_MASK] = bswap(_Data);				\
		return;																			\
	}																					\
	else if (_Address >= 0xE0000000)													\
	{																					\
		LOG(MEMMAP,"WRITE: Cache address out of bounds (addr: %08x data: %08x)",_Address,_Data);	\
/*	    PanicAlert("WRITE: Cache address %08x out of bounds", _Address);		*/		\
	}																					\
	else																				\
	{																					\
		if (bFakeVMEM && ((_Address & 0xFE000000) == 0x7e000000))	                    \
		{                                                                               \
			*(u##_type*)&m_pFakeVMEM[_Address & FAKEVMEM_MASK] = bswap(_Data);			\
			return;                                                                     \
		}                                                                               \
		/* LOG(MEMMAP,"WRITE: %08x (PC: %08x)",_Address,PC);*/							\
		/*MessageBox(NULL, "WRITE: unknown Address", "1", MB_OK);*/						\
		/*CCPU::EnableStepping(TRUE);*/													\
		u32 tmpAddress = CheckDTLB(EffectiveAddress, flag);								\
		tmpAddress = (tmpAddress & 0xFFFFFFF0) | (_Address & 0xF);						\
		*(u##_type*)&m_pRAM[tmpAddress & RAM_MASK] = bswap(_Data);						\
	}																					\
}


bool IsInitialized()
{
	return m_IsInitialized;
}


bool Init()
{
	bool wii = Core::GetStartupParameter().bWii;
	bFakeVMEM = Core::GetStartupParameter().iTLBHack == 1;

	int totalMemSize = RAM_SIZE + EFB_SIZE + L1_CACHE_SIZE + IO_SIZE;
	if (bFakeVMEM)
		totalMemSize += FAKEVMEM_SIZE;
	if (wii)
		totalMemSize += EXRAM_SIZE;

	//Grab some pagefile backed memory out of the void ...
	g_arena.GrabLowMemSpace(totalMemSize);

	//First, map in our regular pointers
	int position = 0;
	m_pRAM     = (u8*)g_arena.CreateView(position, RAM_SIZE);
	position += RAM_SIZE;
	m_pEFB     = (u8*)g_arena.CreateView(position, EFB_SIZE);
	position += EFB_SIZE;
	m_pL1Cache = (u8*)g_arena.CreateView(position, L1_CACHE_SIZE);
	position += L1_CACHE_SIZE;
	if (bFakeVMEM)
	{
		m_pFakeVMEM = (u8*)g_arena.CreateView(position, FAKEVMEM_SIZE);
		position += FAKEVMEM_SIZE;
	}

	if (wii)
		m_pEXRAM = (u8*)g_arena.CreateView(position, EXRAM_SIZE);

#ifdef _M_X64
	//Then, in x64 mode where we have space, grab a 4GB virtual address space
	base = MemArena::Find4GBBase();
	//OK, we know where to find free space. Now grab it!
	
	//Physical should be unmapped when not in "real mode"
	//All in all, we should obey IBAT and DBAT. Maybe IBAT and DBAT should have a 4GB space each?
	//It's not like 4GB is anything these days...
	position = 0;
	m_pPhysicalRAM        = (u8*)g_arena.CreateViewAt(position, RAM_SIZE, base + 0x00000000);
	m_pVirtualCachedRAM   = (u8*)g_arena.CreateViewAt(position, RAM_SIZE, base + 0x80000000);
	m_pVirtualUncachedRAM = (u8*)g_arena.CreateViewAt(position, RAM_SIZE, base + 0xC0000000);
	position += RAM_SIZE;
	m_pVirtualEFB         = (u8*)g_arena.CreateViewAt(position, EFB_SIZE, base + 0xC8000000);
	position += EFB_SIZE;
	m_pVirtualL1Cache     = (u8*)g_arena.CreateViewAt(position, L1_CACHE_SIZE, base + 0xE0000000);
	position += L1_CACHE_SIZE;
	if (bFakeVMEM) {
		m_pPhysicalFakeVMEM   = (u8*)g_arena.CreateViewAt(position, FAKEVMEM_SIZE, base + 0x7E000000);
		position += FAKEVMEM_SIZE;
	}
	
	if (wii)
	{	
		m_pPhysicalEXRAM        = (u8*)g_arena.CreateViewAt(position, EXRAM_SIZE, base + 0x10000000);
		m_pVirtualCachedEXRAM   = (u8*)g_arena.CreateViewAt(position, EXRAM_SIZE, base + 0x90000000);
		m_pVirtualUncachedEXRAM = (u8*)g_arena.CreateViewAt(position, EXRAM_SIZE, base + 0xD0000000);
	}
#else
	// Do a poor mans version - just grab 1GB, possibly discontiguous, and use &0x3FFFFFFF as the mask whenever it is accessed.
	base = MemArena::Find4GBBase();
	if (!base) {
		PanicAlert("Failed to grab 1 GB of contiguous memory!\nDo you have an antivirus program or any other program\n"
			       "that injects itself into every process, consuming address space?\nOr simply a bad graphics driver?\n\n"
				   "Dolphin will handle this better in the future by falling back to slow memory emulation.\n"
				   "For now, sorry, but it won't work. Try the 64-bit build if you can.");
	}
	position = 0;
	m_pPhysicalRAM        = (u8*)g_arena.CreateViewAt(position, RAM_SIZE, base + (0x00000000 & MEMVIEW32_MASK));
	m_pVirtualCachedRAM   = m_pPhysicalRAM;
	m_pVirtualUncachedRAM = m_pPhysicalRAM;
	position += RAM_SIZE;
	m_pVirtualEFB         = (u8*)g_arena.CreateViewAt(position, EFB_SIZE, base + (0xC8000000 & MEMVIEW32_MASK));
	position += EFB_SIZE;
	m_pVirtualL1Cache     = (u8*)g_arena.CreateViewAt(position, L1_CACHE_SIZE, base + (0xE0000000 & MEMVIEW32_MASK));
	position += L1_CACHE_SIZE;
	if (bFakeVMEM) {
		m_pPhysicalFakeVMEM   = (u8*)g_arena.CreateViewAt(position, FAKEVMEM_SIZE, base + (0x7E000000 & MEMVIEW32_MASK));
		position += FAKEVMEM_SIZE;
	}
	//WriteProtectMemory(base + 24*1024*1024, 8*1024*1024);
	if (wii)
	{	
		m_pPhysicalEXRAM        = (u8*)g_arena.CreateViewAt(position, EXRAM_SIZE, base + (0x10000000 & MEMVIEW32_MASK));
		m_pVirtualCachedEXRAM   = m_pPhysicalEXRAM;
		m_pVirtualUncachedEXRAM = m_pPhysicalEXRAM;
	}
#endif

	memset(m_pRAM, 0, RAM_SIZE);
	if (wii) {
		memset(m_pPhysicalEXRAM, 0, EXRAM_SIZE);
	}
	memset(m_pEFB, 0, EFB_SIZE);
	memset(m_pL1Cache, 0, L1_CACHE_SIZE);

	if (wii)
		InitHWMemFuncsWii();
	else
		InitHWMemFuncs();

	LOG(MEMMAP, "Memory system initialized. RAM at %p (0x80000000 @ %p)", base, base + 0x80000000);
	m_IsInitialized = true;
	return true;
}

void DoState(PointerWrap &p)
{
	bool wii = Core::GetStartupParameter().bWii;
	p.DoArray(m_pRAM, RAM_SIZE);
	p.DoArray(m_pEFB, EFB_SIZE);
	p.DoArray(m_pL1Cache, L1_CACHE_SIZE);
	if (wii)
		p.DoArray(m_pEXRAM, EXRAM_SIZE);
}

bool Shutdown()
{
	m_IsInitialized = false;
	bool wii = Core::GetStartupParameter().bWii;

	g_arena.ReleaseView(m_pRAM, RAM_SIZE);
	g_arena.ReleaseView(m_pEFB, EFB_SIZE); 
	g_arena.ReleaseView(m_pL1Cache, L1_CACHE_SIZE);
	if (wii) {
		g_arena.ReleaseView(m_pEXRAM, EXRAM_SIZE);	
	}
	if (bFakeVMEM) { 
		g_arena.ReleaseView(m_pFakeVMEM, FAKEVMEM_SIZE);
	}
	
#ifdef _M_X64
	g_arena.ReleaseView(m_pPhysicalRAM, RAM_SIZE);
	g_arena.ReleaseView(m_pVirtualCachedRAM, RAM_SIZE);
	g_arena.ReleaseView(m_pVirtualUncachedRAM, RAM_SIZE);
	g_arena.ReleaseView(m_pVirtualEFB, EFB_SIZE);
	g_arena.ReleaseView(m_pVirtualL1Cache, L1_CACHE_SIZE);
	if (wii)
	{
		g_arena.ReleaseView(m_pPhysicalEXRAM, EXRAM_SIZE);
		g_arena.ReleaseView(m_pVirtualCachedEXRAM, EXRAM_SIZE);
		g_arena.ReleaseView(m_pVirtualUncachedEXRAM, EXRAM_SIZE);
	}
	if (bFakeVMEM) {
		g_arena.ReleaseView(m_pPhysicalFakeVMEM, FAKEVMEM_SIZE);
	}
#else
	g_arena.ReleaseView(m_pPhysicalRAM, RAM_SIZE);
	g_arena.ReleaseView(m_pVirtualEFB, EFB_SIZE);
	g_arena.ReleaseView(m_pVirtualL1Cache, L1_CACHE_SIZE);
	if (wii)
		g_arena.ReleaseView(m_pPhysicalEXRAM, EXRAM_SIZE);
	if (bFakeVMEM)
		g_arena.ReleaseView(m_pPhysicalFakeVMEM, FAKEVMEM_SIZE);
#endif
	g_arena.ReleaseSpace();
	LOG(MEMMAP, "Memory system shut down.");
	return true;
}


void Clear()
{
	if (m_pRAM)
		memset(m_pRAM, 0, RAM_SIZE);
	if (m_pL1Cache)
		memset(m_pL1Cache, 0, L1_CACHE_SIZE);
	if (m_pEFB)
		memset(m_pEFB, 0, EFB_SIZE);
	if (Core::GetStartupParameter().bWii && m_pEXRAM)
		memset(m_pEXRAM, 0, EXRAM_SIZE);
}


bool AreMemoryBreakpointsActivated()
{
#ifdef NOCHECK
	return false;
#else
	return true;
#endif
}


u32 Read_Instruction(const u32 _Address)
{
	return Jit64::GetOriginalCode(_Address);
}

u32 Read_Opcode(const u32 _Address)
{
#ifdef LOGGING
	if (_Address == 0x00000000)
	{
		PanicAlert("Program tried to read from [00000000]");
		return 0x00000000;
	}
#endif

	u32 _var = 0;	
	ReadFromHardware2(_var, 32, _Address, _Address, FLAG_OPCODE);

	return _var;
}

u8 Read_U8(const u32 _Address)
{    
	u8 _var = (u8)0xAFFEAFFE;
	ReadFromHardware2(_var, 8, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
    TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address,false,1,PC);
	}
#endif
	return (u8)_var;
}

u16 Read_U16(const u32 _Address)
{
	u16 _var = 0;
	ReadFromHardware2(_var, 16, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address,false,2,PC);
	}
#endif
	return (u16)_var;
}

u32 Read_U32(const u32 _Address)
{
#ifdef LOGGING
	if (_Address == 0x00000000)
	{
		//PanicAlert("Program tried to read from [00000000]");
		//return 0x00000000;
	}
#endif

	u32 _var = 0;	
	ReadFromHardware2(_var, 32, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_var, _Address,false,4,PC);
	}
#endif
	return _var;
}


u64 Read_U64(const u32 _Address)
{
	u64 _var = 0;
	ReadFromHardware2(_var, 64, _Address, _Address, FLAG_READ);
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action((u32)_var, _Address,false,8,PC);
	}
#endif
	return _var;
}


void Write_U8(const u8 _Data, const u32 _Address)	
{
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,1,PC);
	}
#endif
	WriteToHardware2(8, _Address, _Data, _Address, FLAG_WRITE);
}


void Write_U16(const u16 _Data, const u32 _Address)
{
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,2,PC);
	}
#endif

	WriteToHardware2(16, _Address, _Data, _Address, FLAG_WRITE);
}


void Write_U32(const u32 _Data, const u32 _Address)
{	
#ifndef NOCHECK
	TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action(_Data,_Address,true,4,PC);
	}
#endif
	WriteToHardware2(32, _Address, _Data, _Address, FLAG_WRITE);
}


void WriteHW_U32(const u32 _Data, const u32 _Address)
{
	hwWrite32[(_Address>>HWSHIFT) & (NUMHWMEMFUN-1)](_Data,_Address);
}


void Write_U64(const u64 _Data, const u32 _Address)
{
#ifndef NOCHECK
    TMemCheck *mc = CBreakPoints::GetMemCheck(_Address);
	if (mc)
	{
		mc->numHits++;
		mc->Action((u32)_Data,_Address,true,8,PC);
	}
#endif

	WriteToHardware2(64, _Address, _Data, _Address + 4, FLAG_WRITE);
}


u8 ReadUnchecked_U8(const u32 _Address)
{    
	u8 _var = (u8)0xAFFEAFFE;
	ReadFromHardware2(_var, 8, _Address, _Address, FLAG_NO_EXCEPTION);
	return (u8)_var;
}


u32 ReadUnchecked_U32(const u32 _Address)
{
	u32 _var = 0;
	ReadFromHardware2(_var, 32, _Address, _Address, FLAG_NO_EXCEPTION);
	return _var;
}

void WriteUnchecked_U8(const u8 _iValue, const u32 _Address)
{
	WriteToHardware2(8, _Address, _iValue, _Address, FLAG_NO_EXCEPTION);
}
 

void WriteUnchecked_U32(const u32 _iValue, const u32 _Address)
{
	WriteToHardware2(32, _Address, _iValue, _Address, FLAG_NO_EXCEPTION);
}


void WriteBigEData(const u8 *_pData, const u32 _Address, const u32 _iSize)
{
	memcpy(GetPointer(_Address), _pData, _iSize);
}


void Memset(const u32 _Address, const u8 _iValue, const u32 _iLength)
{	
    u8 *ptr = GetPointer(_Address);
    if (ptr != NULL)
    {
	    memset(ptr,_iValue,_iLength);
    }
    else
    {
        // (comment for old implementation) : F|RES: rouge squadron and other games use the TLB ... so this cant work
        
        // fixed implementation:
        for (u32 i = 0; i < _iLength; i++)
            Write_U8(_iValue, _Address + i);
    }
}


void DMA_LCToMemory(const u32 _MemAddr, const u32 _CacheAddr, const u32 _iNumBlocks)
{
	u8 *src = GetCachePtr() + (_CacheAddr & 0x3FFFF);
	u8 *dst = GetPointer(_MemAddr);

    if ((dst != NULL) && (src != NULL))
    {
	    memcpy(dst, src, 32 * _iNumBlocks);
    }
    else
    {
        for (u32 i = 0; i < 32 * _iNumBlocks; i++)
        {
            u8 Temp = Read_U8(_CacheAddr + i);
            Write_U8(Temp, _MemAddr + i);
        }
    }
}


void DMA_MemoryToLC(const u32 _CacheAddr, const u32 _MemAddr, const u32 _iNumBlocks)
{
	u8 *src = GetPointer(_MemAddr);
	u8 *dst = GetCachePtr() + (_CacheAddr & 0x3FFFF);

    if ((dst != NULL) && (src != NULL))
    {
        memcpy(dst, src, 32 * _iNumBlocks);
    }
    else
    {
        for (u32 i = 0; i < 32 * _iNumBlocks; i++)
        {
            u8 Temp = Read_U8(_MemAddr + i);
            Write_U8(Temp, _CacheAddr + i);
        }
    }
}


void ReadBigEData( u8 *_pData, const u32 _Address, const u32 size)
{
	u8 *src = GetPointer(_Address);
	memcpy(_pData, src, size);
}


void GetString(std::string& _string, const u32 _Address)
{
	char stringBuffer[2048];
	char *string = stringBuffer;
	char c;
	int addr = _Address;
	while ((c = Read_U8(addr)))
	{
		*string++ = c;
		addr++;
	}
	*string++=0;

	_string = stringBuffer;
}


// GetPointer must always return an address in the bottom 32 bits of address space, so that 64-bit
// programs don't have problems directly addressing any part of memory.
u8 *GetPointer(const u32 _Address)
{
	switch (_Address >> 24)
	{
	case 0x00:
	case 0x01:
	case 0x80:
	case 0x81:
	case 0xC0:
	case 0xC1:
		return (u8*)(((char*)m_pRAM) + (_Address & RAM_MASK));

    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
		if (Core::GetStartupParameter().bWii)
			return (u8*)(((char*)m_pEXRAM) + (_Address & EXRAM_MASK));
		else
			return 0;

	case 0xE0:
		if (_Address < (0xE0000000 + L1_CACHE_SIZE))
			return GetCachePtr() + (_Address & L1_CACHE_MASK);
		else
			return 0;

	case 0xC8:
		return m_pEFB + (_Address & 0xFFFFFF);
	case 0xCC:
	case 0xCD:
		_dbg_assert_msg_(MEMMAP, 0, "Memory", "GetPointer from IO Bridge doesnt work");
		return NULL;
	default:
		PanicAlert("Tried to get pointer for unknown address %08x", _Address);
		break;
	}
	return NULL;
}


bool IsRAMAddress(const u32 addr, bool allow_locked_cache) 
{
	switch ((addr >> 24) & 0xFC) {
	case 0x00:
	case 0x80:
	case 0xC0:
		if ((addr & 0x1FFFFFFF) < RAM_SIZE)
			return true;
		else
			return false;
	case 0x10:
	case 0x90:
	case 0xD0:
		if (Core::g_CoreStartupParameter.bWii && (addr & 0x0FFFFFFF) < EXRAM_SIZE)
			return true;
		else
			return false;
	case 0xE0:
		if (allow_locked_cache && addr - 0xE0000000 < L1_CACHE_SIZE)
			return true;
		else
			return false;
	case 0x7C:
		if (bFakeVMEM && addr >= 0x7e000000)
			return true;
		else
			return false;

	default:
		return false;
	}
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
*	PearPC
*	ppc_mmu.cc
*
*	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License version 2 as
*	published by the Free Software Foundation.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program; if not, write to the Free Software
*	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#define EA_Offset(v)	((v)&0xfff)
#define EA_API(v)		(((v)>>22)&0x3f) 

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

union UPTE1
{
	struct 
	{
		unsigned API    : 6;
		unsigned H      : 1;
		unsigned VSID   : 24;
		unsigned V      : 1;        
	};
	u32 Hex;
};

union UPTE2
{
	struct 
	{
		unsigned PP     : 2;
		unsigned        : 1;
		unsigned WIMG   : 4;
		unsigned C      : 1;
		unsigned R      : 1;
		unsigned        : 3;
		unsigned RPN    : 20;
	};
	u32 Hex;
};

u32 pagetable_base = 0;
u32 pagetable_hashmask = 0;

void GenerateDSIException(u32 _EffectiveAdress, bool _bWrite)
{
	if (_bWrite)
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE;
	else
		PowerPC::ppcState.spr[SPR_DSISR] = PPC_EXC_DSISR_PAGE;

	PowerPC::ppcState.spr[SPR_DAR] = _EffectiveAdress;

	LOG(MEMMAP, "Generate DSI Exception 0x%08x", _EffectiveAdress);
	PowerPC::ppcState.Exceptions |= EXCEPTION_DSI;
}


void GenerateISIException()
{
	// 4 bit for Set if the translation of an attempted access is not found in the primary hash table entry group
	// (HTEG), or in the rehashed secondary HTEG, or in the range of a DBAT register (page fault
	// condition); otherwise cleared.
	PowerPC::ppcState.spr[SPR_DSISR] = 0x4000000; 
	LOG(MEMMAP, "Generate ISI Exception");
	PowerPC::ppcState.Exceptions |= EXCEPTION_ISI;
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
	pagetable_base = htaborg<<16;
	pagetable_hashmask = ((xx<<10)|0x3ff);
} 


u32 CheckDTLB(u32 _Address, XCheckTLBFlag _Flag)
{
	//return 0;
	if (Core::GetStartupParameter().bWii) {
		// TLB is never used on Wii (except linux and stuff, but we don't care about that)
		PanicAlert("%s invalid memory region (0x%08x)\n\n"
			"There is no way to recover from this error,"
			"so Dolphin will now exit. Sorry!",
			_Flag == FLAG_WRITE ? "Write to" : "Read from", _Address);
	}
	else {
		PanicAlert("%s invalid memory region (0x%08x)\n\n"
			"This is either the game crashing randomly, or a TLB write."
			"Several games uses the TLB to map memory. This\n"
			"function is not supported in Dolphin. "
			"Unfortunately there is no way to recover from this error,"
			"so Dolphin will now exit abruptly. Sorry!",
			 _Flag == FLAG_WRITE ? "Write to" : "Read from", _Address);
	}
	exit(0);
	u32 sr = PowerPC::ppcState.sr[EA_SR(_Address)]; 

	u32 offset = EA_Offset(_Address);			// 12 bit 
	u32 page_index = EA_PageIndex(_Address);	// 16 bit
	u32 VSID = SR_VSID(sr);						// 24 bit 
	u32 api = EA_API(_Address);					//  6 bit (part of page_index) 

	u8* pRAM = GetPointer(0);

	// hash function no 1 "xor" .360
	u32 hash1 = (VSID ^ page_index);
	u32 pteg_addr = ((hash1 & pagetable_hashmask)<<6) | pagetable_base; 

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
	pteg_addr = ((hash1 & pagetable_hashmask)<<6) | pagetable_base;
	for (int i = 0; i < 8; i++) 
	{ 
		u32 pte = bswap(*(u32*)&pRAM[pteg_addr]);
		if ((pte & PTE1_V) && (pte & PTE1_H)) 
		{
			if (VSID == PTE1_VSID(pte) && (api == PTE1_API(pte))) 
			{
				PanicAlert("TLB: Address found at the second hash function.\n"
						   "i have never seen this before");

				pte = bswap(*(u32*)&pRAM[(pteg_addr+4)]);

				u32 physAddress = PTE2_RPN(pte) | offset; 

				// missing access bits
				return physAddress;
			}
		}
		pteg_addr+=8; 
	}


	// exception generation
	switch(_Flag)
	{
	case FLAG_NO_EXCEPTION:
		break;

	case FLAG_READ:
		GenerateDSIException(_Address, false);
		break;

	case FLAG_WRITE:
		GenerateDSIException(_Address, true);
		break;

	case FLAG_OPCODE:
		GenerateISIException();
		break;
	}

	return 0;
}


}  // namespace
