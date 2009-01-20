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

/*
These functions are primarily used by the interpreter versions of the LoadStore instructions.
However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
may be redirected here (for example to Read_U32()).
*/

#include "Common.h"
#include "MemoryUtil.h"
#include "MemArena.h"
#include "ChunkFile.h"

#include "Memmap.h"
#include "../Core.h"
#include "../PowerPC/PowerPC.h"
#include "../PowerPC/Jit64/Jit.h"
#include "../PowerPC/Jit64/JitCache.h"
#include "CPU.h"
#include "PeripheralInterface.h"
#include "DSP.h"
#include "DVDInterface.h"
#include "GPFifo.h"
#include "VideoInterface.h"
#include "SI.h"
#include "EXI.h"
#include "PixelEngine.h"
#include "CommandProcessor.h"
#include "AudioInterface.h"
#include "MemoryInterface.h"
#include "WII_IOB.h"
#include "WII_IPC.h"
#include "../ConfigManager.h"
#include "../Debugger/Debugger_BreakPoints.h"
#include "../Debugger/Debugger_SymbolMap.h"
/////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ----------------
namespace Memory
{

// =================================
// LOCAL SETTINGS
// ----------------

// Disable Memory Checks
// #define NOCHECK

// Always disable memory checks if the Release build
#ifndef LOGGING
	#define NOCHECK
#endif

/* Enable the Translation Lookaside Buffer functions. TLBHack = 1 in Dolphin.ini or a
   <GameID>.ini file will set this to true */
bool bFakeVMEM = false;
// ==============


// =================================
// Init() declarations
// ----------------
// Store the MemArena here
u8*	base = NULL;

// The MemArena class
MemArena g_arena;
// ==============


// STATE_TO_SAVE (applies to a lot of things in this file)

// Pointers to low memory
u8*	m_pRAM = NULL;
u8*	m_pFakeVMEM = NULL;
u8*	m_pEXRAM = NULL; //wii
u8*	m_pEFB = NULL;
u8*	m_pL1Cache = NULL;
bool m_IsInitialized = false; // Save the Init(), Shutdown() state

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

// =================================
// Read and write shortcuts
// ----------------
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
// ===============

/////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
// Default read and write functions
// ----------------
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
/////////////////////////////


///////////////////////////////////////////////////////////////////////////////////
/* Create shortcuts to the hardware devices' read and write functions. This can be seen
   as an alternative to a switch() or if() table. */
// ----------------
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
/////////////////////////////



///////////////////////////////////////////////////////////////////////////////////
// Init and Shutdown
// ----------------
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

	LOG(MEMMAP, "Memory system initialized. RAM at %p (mirrors at 0 @ %p, 0x80000000 @ %p , 0xC0000000 @ %p)",
		m_pRAM, m_pPhysicalRAM, m_pVirtualCachedRAM, m_pVirtualUncachedRAM);
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


u32 Read_Instruction(const u32 em_address)
{
	return jit.GetBlockCache()->GetOriginalCode(em_address);
}
//////////////////////////////////////////////////////////



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





///////////////////////////////////////////////////////////////////////////////////
// Other functions
// ----------------
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
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && (addr & 0x0FFFFFFF) < EXRAM_SIZE)
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
/////////////////////////////


}  // namespace
