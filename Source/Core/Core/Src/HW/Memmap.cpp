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
#include "../PowerPC/JitCommon/JitBase.h"
#include "../HLE/HLE.h"
#include "CPU.h"
#include "ProcessorInterface.h"
#include "DSP.h"
#include "DVDInterface.h"
#include "GPFifo.h"
#include "VideoInterface.h"
#include "SI.h"
#include "EXI.h"
#include "PluginVideo.h"
#include "AudioInterface.h"
#include "MemoryInterface.h"
#include "WII_IOB.h"
#include "WII_IPC.h"
#include "../ConfigManager.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "../PluginManager.h"

namespace Memory
{

// =================================
// LOCAL SETTINGS
// ----------------

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

// STATE_TO_SAVE
bool m_IsInitialized = false; // Save the Init(), Shutdown() state
// END STATE_TO_SAVE

// 64-bit: Pointers to low-mem (sub-0x10000000) mirror
// 32-bit: Same as the corresponding physical/virtual pointers.
u8 *m_pRAM;
u8 *m_pL1Cache;
u8 *m_pEXRAM;
u8 *m_pFakeVMEM;
u8 *m_pEFB;

// 64-bit: Pointers to high-mem mirrors
// 32-bit: Same as above
u8 *m_pPhysicalRAM;
u8 *m_pVirtualCachedRAM;
u8 *m_pVirtualUncachedRAM;
u8 *m_pPhysicalEXRAM;        // wii only
u8 *m_pVirtualCachedEXRAM;   // wii only
u8 *m_pVirtualUncachedEXRAM; // wii only
u8 *m_pVirtualEFB;
u8 *m_pVirtualL1Cache;
u8 *m_pVirtualFakeVMEM;

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

// Default read and write functions
template <class T>
void HW_Default_Write(const T _Data, const u32 _Address){	ERROR_LOG(MASTER_LOG, "Illegal HW Write%i %08x", sizeof(T)*8, _Address);_dbg_assert_(MEMMAP, 0);}

template <class T>
void HW_Default_Read(T _Data, const u32 _Address){	ERROR_LOG(MASTER_LOG, "Illegal HW Read%i %08x", sizeof(T)*8, _Address); _dbg_assert_(MEMMAP, 0);}

#define PAGE_SHIFT 10
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define PAGE_MASK (PAGE_SHIFT - 1)

template <class T, u8 *P> void HW_Read_Memory(T &_Data, const u32 _Address) {
	_Data = *(T *)&P[_Address & PAGE_MASK];
}
template <class T, u8 *P> void HW_Write_Memory(T _Data, const u32 _Address) {
	*(T *)&P[_Address & PAGE_MASK] = _Data;
}

// Create shortcuts to the hardware devices' read and write functions.
// This can be seen as an alternative to a switch() or if() table. 
#define BLOCKSIZE 4
#define CP_START		0x00 //0x0000 >> 10
#define WII_IPC_START	0x00 //0x0000 >> 10
#define PE_START		0x04 //0x1000 >> 10
#define VI_START		0x08 //0x2000 >> 10
#define PI_START		0x0C //0x3000 >> 10
#define MI_START		0x10 //0x4000 >> 10
#define DSP_START		0x14 //0x5000 >> 10
#define DVD_START		0x18 //0x6000 >> 10
#define SI_START		0x19 //0x6400 >> 10
#define EI_START		0x1A //0x6800 >> 10
#define AUDIO_START		0x1B //0x6C00 >> 10
#define GP_START		0x20 //0x8000 >> 10

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

		// To prevent Dolphin from crashing when accidentally running Wii
		// executables in GC mode (or running malicious GC executables...)
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
		hwRead16 [CP_START+i] = CPluginManager::GetInstance().GetVideo()->Video_CommandProcessorRead16;
		hwWrite16[CP_START+i] = CPluginManager::GetInstance().GetVideo()->Video_CommandProcessorWrite16;
 
		hwRead16 [PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineRead16;
		hwWrite16[PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineWrite16;
		hwWrite32[PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineWrite32;

		hwRead8  [VI_START+i] = VideoInterface::Read8;
		hwRead16 [VI_START+i] = VideoInterface::Read16;
		hwRead32 [VI_START+i] = VideoInterface::Read32;
		hwWrite16[VI_START+i] = VideoInterface::Write16;
		hwWrite32[VI_START+i] = VideoInterface::Write32;

		hwRead32 [PI_START+i] = ProcessorInterface::Read32;
		hwWrite32[PI_START+i] = ProcessorInterface::Write32;

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
	hwWrite64[GP_START] = GPFifo::Write64;
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
		hwRead16 [CP_START+i] = CPluginManager::GetInstance().GetVideo()->Video_CommandProcessorRead16;
		hwWrite16[CP_START+i] = CPluginManager::GetInstance().GetVideo()->Video_CommandProcessorWrite16;
 
		hwRead16 [PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineRead16;
		hwWrite16[PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineWrite16;
		hwWrite32[PE_START+i] = CPluginManager::GetInstance().GetVideo()->Video_PixelEngineWrite32;

		hwRead32 [PI_START+i] = ProcessorInterface::Read32;
		hwWrite32[PI_START+i] = ProcessorInterface::Write32;

		hwRead8  [VI_START+i] = VideoInterface::Read8;
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
	hwWrite64[GP_START] = GPFifo::Write64;

	for (int i = 0; i < BLOCKSIZE; i++)
	{
		hwReadWii32[WII_IPC_START+i] = WII_IPCInterface::Read32;
		hwWriteWii32[WII_IPC_START+i] = WII_IPCInterface::Write32;
	}

	hwRead32	[DVD_START] = DVDInterface::Read32;
	hwReadWii32	[DVD_START] = DVDInterface::Read32;
	hwWrite32	[DVD_START] = DVDInterface::Write32;
	hwWriteWii32[DVD_START] = DVDInterface::Write32;

	hwRead32	[SI_START] = SerialInterface::Read32;
	hwReadWii32	[SI_START] = SerialInterface::Read32;
	hwWrite32	[SI_START] = SerialInterface::Write32;
	hwWriteWii32[SI_START] = SerialInterface::Write32;

	hwRead32	[EI_START] = ExpansionInterface::Read32;
	hwReadWii32	[EI_START] = ExpansionInterface::Read32;
	hwWrite32	[EI_START] = ExpansionInterface::Write32;
	hwWriteWii32[EI_START] = ExpansionInterface::Write32;

	// [F|RES] i thought this doesn't exist anymore
	hwRead32	[AUDIO_START] = AudioInterface::Read32;
	hwReadWii32	[AUDIO_START] = AudioInterface::Read32;
	hwWrite32	[AUDIO_START] = AudioInterface::Write32;
	hwWriteWii32[AUDIO_START] = AudioInterface::Write32;
}

writeFn32 GetHWWriteFun32(const u32 _Address)
{
	return hwWrite32[(_Address >> HWSHIFT) & (NUMHWMEMFUN-1)];
}

bool IsInitialized()
{
	return m_IsInitialized;
}


// We don't declare the IO region in here since its handled by other means.
static const MemoryView views[] =
{
	{&m_pRAM,      &m_pPhysicalRAM,          0x00000000, RAM_SIZE, 0},
	{NULL,         &m_pVirtualCachedRAM,     0x80000000, RAM_SIZE, MV_MIRROR_PREVIOUS},
	{NULL,         &m_pVirtualUncachedRAM,   0xC0000000, RAM_SIZE, MV_MIRROR_PREVIOUS},

//  Don't map any memory for the EFB. We want all access to this area to go
//  through the hardware access handlers.
#ifndef _M_X64
//	{&m_pEFB,      &m_pVirtualEFB,           0xC8000000, EFB_SIZE, 0},
#endif
	{&m_pL1Cache,  &m_pVirtualL1Cache,       0xE0000000, L1_CACHE_SIZE, 0},

	{&m_pFakeVMEM, &m_pVirtualFakeVMEM,      0x7E000000, FAKEVMEM_SIZE, MV_FAKE_VMEM},

	{&m_pEXRAM,    &m_pPhysicalEXRAM,        0x10000000, EXRAM_SIZE, MV_WII_ONLY},
	{NULL,         &m_pVirtualCachedEXRAM,   0x90000000, EXRAM_SIZE, MV_WII_ONLY | MV_MIRROR_PREVIOUS},
	{NULL,         &m_pVirtualUncachedEXRAM, 0xD0000000, EXRAM_SIZE, MV_WII_ONLY | MV_MIRROR_PREVIOUS},
};
static const int num_views = sizeof(views) / sizeof(MemoryView);

bool Init()
{
	bool wii = Core::GetStartupParameter().bWii;
	bFakeVMEM = Core::GetStartupParameter().iTLBHack == 1;

	u32 flags = 0;
	if (wii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	base = MemoryMap_Setup(views, num_views, flags, &g_arena);

	if (wii)
		InitHWMemFuncsWii();
	else
		InitHWMemFuncs();

	INFO_LOG(MEMMAP, "Memory system initialized. RAM at %p (mirrors at 0 @ %p, 0x80000000 @ %p , 0xC0000000 @ %p)",
		m_pRAM, m_pPhysicalRAM, m_pVirtualCachedRAM, m_pVirtualUncachedRAM);
	m_IsInitialized = true;
	return true;
}

void DoState(PointerWrap &p)
{
	bool wii = Core::GetStartupParameter().bWii;
	p.DoArray(m_pPhysicalRAM, RAM_SIZE);
//	p.DoArray(m_pVirtualEFB, EFB_SIZE);
	p.DoArray(m_pVirtualL1Cache, L1_CACHE_SIZE);
	if (wii)
		p.DoArray(m_pEXRAM, EXRAM_SIZE);
}

bool Shutdown()
{
	m_IsInitialized = false;
	u32 flags = 0;
	if (Core::GetStartupParameter().bWii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	MemoryMap_Shutdown(views, num_views, flags, &g_arena);
	g_arena.ReleaseSpace();
	base = NULL;
	INFO_LOG(MEMMAP, "Memory system shut down.");
	return true;
}

void Clear()
{
	if (m_pRAM)
		memset(m_pRAM, 0, RAM_SIZE);
	if (m_pL1Cache)
		memset(m_pL1Cache, 0, L1_CACHE_SIZE);
	if (Core::GetStartupParameter().bWii && m_pEXRAM)
		memset(m_pEXRAM, 0, EXRAM_SIZE);
}

bool AreMemoryBreakpointsActivated()
{
#ifndef ENABLE_MEM_CHECK
	return false;
#else
	return true;
#endif
}

u32 Read_Instruction(const u32 em_address)
{
	UGeckoInstruction inst = ReadUnchecked_U32(em_address);	
	if (inst.OPCD == 1)
		return HLE::GetOrigInstruction(em_address);
	else
		return inst.hex;
}

u32 Read_Opcode_JIT(const u32 _Address)
{
#ifdef JIT_UNLIMITED_ICACHE	
	if ((_Address & ~JIT_ICACHE_MASK) != 0x80000000 && (_Address & ~JIT_ICACHE_MASK) != 0x00000000 &&
		(_Address & ~JIT_ICACHE_MASK) != 0x7e000000 && // TLB area
		(_Address & ~JIT_ICACHEEX_MASK) != 0x90000000 && (_Address & ~JIT_ICACHEEX_MASK) != 0x10000000)
	{
		PanicAlert("iCacheJIT: Reading Opcode from %x. Please report.", _Address);
		return 0;
	}
	u8* iCache;
	u32 addr;
	if (_Address & JIT_ICACHE_VMEM_BIT)
	{		
		iCache = jit->GetBlockCache()->GetICacheVMEM();
		addr = _Address & JIT_ICACHE_MASK;
	}
	else if (_Address & JIT_ICACHE_EXRAM_BIT)
	{		
		iCache = jit->GetBlockCache()->GetICacheEx();
		addr = _Address & JIT_ICACHEEX_MASK;
	}
	else
	{
		iCache = jit->GetBlockCache()->GetICache();
		addr = _Address & JIT_ICACHE_MASK;
	}
	u32 inst = *(u32*)(iCache + addr);
	if (inst == JIT_ICACHE_INVALID_WORD)
	{
		u32 cache_block_start = addr & ~0x1f;
		u32 mem_block_start = _Address & ~0x1f;
		u8 *pMem = Memory::GetPointer(mem_block_start);
		memcpy(iCache + cache_block_start, pMem, 32);
		inst = *(u32*)(iCache + addr);
	}
	inst = Common::swap32(inst);
#else
	u32 inst = Memory::ReadUnchecked_U32(_Address);
#endif
	if ((inst & 0xfc000000) == 0)
	{
		inst = jit->GetBlockCache()->GetOriginalFirstOp(inst);
	}
	
	// if a crash occured after that message
	// that means that we've compiled outdated code from the cache instead of memory
	// this could happen if a game forgot to icbi the new code
	u32 inst_mem = Memory::ReadUnchecked_U32(_Address);	
	if (inst_mem != inst)
			ERROR_LOG(POWERPC, "JIT: compiling outdated code. addr=%x, mem=%x, cache=%x", _Address, inst_mem, inst);
	return inst;
}

u32 Read_Opcode_JIT_LC(const u32 _Address)
{
#ifdef JIT_UNLIMITED_ICACHE	
	if ((_Address & ~JIT_ICACHE_MASK) != 0x80000000 && (_Address & ~JIT_ICACHE_MASK) != 0x00000000 &&
		(_Address & ~JIT_ICACHE_MASK) != 0x7e000000 && // TLB area
		(_Address & ~JIT_ICACHEEX_MASK) != 0x90000000 && (_Address & ~JIT_ICACHEEX_MASK) != 0x10000000)
	{
		PanicAlert("iCacheJIT: Reading Opcode from %x. Please report.", _Address);
		return 0;
	}
	u8* iCache;
	u32 addr;
	if (_Address & JIT_ICACHE_VMEM_BIT)
	{		
		iCache = jit->GetBlockCache()->GetICacheVMEM();
		addr = _Address & JIT_ICACHE_MASK;
	}
	else if (_Address & JIT_ICACHE_EXRAM_BIT)
	{		
		iCache = jit->GetBlockCache()->GetICacheEx();
		addr = _Address & JIT_ICACHEEX_MASK;
	}
	else
	{
		iCache = jit->GetBlockCache()->GetICache();
		addr = _Address & JIT_ICACHE_MASK;
	}
	u32 inst = *(u32*)(iCache + addr);
	if (inst == JIT_ICACHE_INVALID_WORD)
		inst = Memory::ReadUnchecked_U32(_Address);
	else
		inst = Common::swap32(inst);
#else
	u32 inst = Memory::ReadUnchecked_U32(_Address);
#endif
	if ((inst & 0xfc000000) == 0)
	{
		inst = jit->GetBlockCache()->GetOriginalFirstOp(inst);
	}	
	return inst;
}

// WARNING! No checks!
// We assume that _Address is cached
void Write_Opcode_JIT(const u32 _Address, const u32 _Value)
{
#ifdef JIT_UNLIMITED_ICACHE
	if (_Address & JIT_ICACHE_VMEM_BIT)
	{
		*(u32*)(jit->GetBlockCache()->GetICacheVMEM() + (_Address & JIT_ICACHE_MASK)) = Common::swap32(_Value);		
	}
	else if (_Address & JIT_ICACHE_EXRAM_BIT)
	{
		*(u32*)(jit->GetBlockCache()->GetICacheEx() + (_Address & JIT_ICACHEEX_MASK)) = Common::swap32(_Value);		
	}
	else
		*(u32*)(jit->GetBlockCache()->GetICache() + (_Address & JIT_ICACHE_MASK)) = Common::swap32(_Value);
#else
	Memory::WriteUnchecked_U32(_Value, _Address);
#endif	
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
			WARN_LOG(CONSOLE, "Read%i: Program tried to read [%08x] from [%08x]", Bits, Address);		
		}
		else
		{
			ERROR_LOG(CONSOLE, "Write%i: Program tried to write [%08x] to [%08x]", Bits, Data, Address);	
		}
	}

	if (Address == 0)																		
	{		
		if(Read)
		{
			WARN_LOG(CONSOLE, "Read%i: Program read [0x%08x] from [0x%08x]     * * *   0   * * *", Bits, Data, Address);	
		}
		else
		{
			WARN_LOG(CONSOLE, "Write%i: Program wrote [0x%08x] to [0x%08x]     * * *   0   * * *", Bits, Data, Address);	
		}
	}
	// Try to figure out where the dev/di Ioctl arguments are stored (including buffer out), so we can
	// find the bad one
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
			ERROR_LOG(CONSOLE, "Read%i: Program read [0x%08x] from [0x%08x]      * * * * * * * * * * * *", Bits, Data, Address);	
		}
		else
		{
			ERROR_LOG(CONSOLE, "Write%i: Program wrote [0x%08x] to [0x%08x]      * * * * * * * * * * * *", Bits,Data, Address);	
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
        for (u32 i = 0; i < _iLength; i++)
            Write_U8(_iValue, _Address + i);
    }
}

void DMA_LCToMemory(const u32 _MemAddr, const u32 _CacheAddr, const u32 _iNumBlocks)
{
	const u8 *src = GetCachePtr() + (_CacheAddr & 0x3FFFF);
	u8 *dst = GetPointer(_MemAddr);

    if ((dst != NULL) && (src != NULL) && (_MemAddr & 3) == 0 && (_CacheAddr & 3) == 0)
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
	const u8 *src = GetPointer(_MemAddr);
	u8 *dst = GetCachePtr() + (_CacheAddr & 0x3FFFF);

    if ((dst != NULL) && (src != NULL) && (_MemAddr & 3) == 0 && (_CacheAddr & 3) == 0)
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

void ReadBigEData(u8 *data, const u32 em_address, const u32 size)
{
	u8 *src = GetPointer(em_address);
	memcpy(data, src, size);
}

void GetString(std::string& _string, const u32 em_address)
{
	char stringBuffer[2048];
	char *string = stringBuffer;
	char c;
	u32 addr = em_address;
	while ((c = Read_U8(addr)))
	{
		*string++ = c;
		addr++;
	}
	*string++ = '\0';
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
		return (u8*)(((char*)m_pPhysicalRAM) + (_Address & RAM_MASK));

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
			return (u8*)(((char*)m_pPhysicalEXRAM) + (_Address & EXRAM_MASK));
		else
			return 0;

	case 0x7E:
	case 0x7F:
		return (u8*)(((char*)m_pVirtualFakeVMEM) + (_Address & RAM_MASK));

	case 0xE0:
		if (_Address < (0xE0000000 + L1_CACHE_SIZE))
			return GetCachePtr() + (_Address & L1_CACHE_MASK);
		else
			return 0;

	case 0xC8:
		return 0;  // EFB. We don't want to return a pointer here since we have no memory mapped for it.

	case 0xCC:
	case 0xCD:
		_dbg_assert_msg_(MEMMAP, 0, "Memory", "GetPointer from IO Bridge doesnt work");
		return NULL;
	//case 0x47: TODO
	case 0x7B:
	case 0xFF: 
		break;

	default:
		if (!PanicYesNo("Unknown pointer address prefix %02X, report this to the devs: 0x%08X \n Continue?", (_Address >> 24), _Address))
			Crash();
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

}  // namespace
