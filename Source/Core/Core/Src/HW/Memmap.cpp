// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// NOTE:
// These functions are primarily used by the interpreter versions of the LoadStore instructions.
// However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
// may be redirected here (for example to Read_U32()).


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
#include "AudioInterface.h"
#include "MemoryInterface.h"
#include "WII_IOB.h"
#include "WII_IPC.h"
#include "../ConfigManager.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "VideoBackendBase.h"

namespace Memory
{

// =================================
// LOCAL SETTINGS
// ----------------

/* Enable the Translation Lookaside Buffer functions. TLBHack = 1 in Dolphin.ini or a
   <GameID>.ini file will set this to true */
bool bFakeVMEM = false;
bool bMMU = false;
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
//u8 *m_pEFB;

// 64-bit: Pointers to high-mem mirrors
// 32-bit: Same as above
u8 *m_pPhysicalRAM;
u8 *m_pVirtualCachedRAM;
u8 *m_pVirtualUncachedRAM;
u8 *m_pPhysicalEXRAM;        // wii only
u8 *m_pVirtualCachedEXRAM;   // wii only
u8 *m_pVirtualUncachedEXRAM; // wii only
//u8 *m_pVirtualEFB;
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
void HW_Default_Write(const T _Data, const u32 _Address){	ERROR_LOG(MASTER_LOG, "Illegal HW Write%lu %08x", (unsigned long)sizeof(T)*8, _Address);_dbg_assert_(MEMMAP, 0);}

template <class T>
void HW_Default_Read(T _Data, const u32 _Address){	ERROR_LOG(MASTER_LOG, "Illegal HW Read%lu %08x", (unsigned long)sizeof(T)*8, _Address); _dbg_assert_(MEMMAP, 0);}

#define HW_PAGE_SHIFT 10
#define HW_PAGE_SIZE (1 << HW_PAGE_SHIFT)
#define HW_PAGE_MASK (HW_PAGE_SHIFT - 1)

template <class T, u8 *P> void HW_Read_Memory(T &_Data, const u32 _Address)
{
	_Data = *(T *)&P[_Address & HW_PAGE_MASK];
}

template <class T, u8 *P> void HW_Write_Memory(T _Data, const u32 _Address)
{
	*(T *)&P[_Address & HW_PAGE_MASK] = _Data;
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
		hwRead16 [CP_START+i] = g_video_backend->Video_CPRead16();
		hwWrite16[CP_START+i] = g_video_backend->Video_CPWrite16();

		hwRead16 [PE_START+i] = g_video_backend->Video_PERead16();
		hwWrite16[PE_START+i] = g_video_backend->Video_PEWrite16();
		hwWrite32[PE_START+i] = g_video_backend->Video_PEWrite32();

		hwRead8  [VI_START+i] = VideoInterface::Read8;
		hwRead16 [VI_START+i] = VideoInterface::Read16;
		hwRead32 [VI_START+i] = VideoInterface::Read32;
		hwWrite16[VI_START+i] = VideoInterface::Write16;
		hwWrite32[VI_START+i] = VideoInterface::Write32;

		hwRead16 [PI_START+i] = ProcessorInterface::Read16;
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
		hwRead16 [CP_START+i] = g_video_backend->Video_CPRead16();
		hwWrite16[CP_START+i] = g_video_backend->Video_CPWrite16();

		hwRead16 [PE_START+i] = g_video_backend->Video_PERead16();
		hwWrite16[PE_START+i] = g_video_backend->Video_PEWrite16();
		hwWrite32[PE_START+i] = g_video_backend->Video_PEWrite32();

		hwRead16 [PI_START+i] = ProcessorInterface::Read16;
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

void Init()
{
	bool wii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	bFakeVMEM = SConfig::GetInstance().m_LocalCoreStartupParameter.iTLBHack == 1;
	bMMU = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;

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
}

void DoState(PointerWrap &p)
{
	bool wii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	p.DoArray(m_pPhysicalRAM, RAM_SIZE);
//	p.DoArray(m_pVirtualEFB, EFB_SIZE);
	p.DoArray(m_pVirtualL1Cache, L1_CACHE_SIZE);
	p.DoMarker("Memory RAM");
	if (bFakeVMEM)
		p.DoArray(m_pVirtualFakeVMEM, FAKEVMEM_SIZE);
	p.DoMarker("Memory FakeVMEM");
	if (wii)
		p.DoArray(m_pEXRAM, EXRAM_SIZE);
	p.DoMarker("Memory EXRAM");
}

void Shutdown()
{
	m_IsInitialized = false;
	u32 flags = 0;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	MemoryMap_Shutdown(views, num_views, flags, &g_arena);
	g_arena.ReleaseSpace();
	base = NULL;
	INFO_LOG(MEMMAP, "Memory system shut down.");
}

void Clear()
{
	if (m_pRAM)
		memset(m_pRAM, 0, RAM_SIZE);
	if (m_pL1Cache)
		memset(m_pL1Cache, 0, L1_CACHE_SIZE);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && m_pEXRAM)
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
	return inst.hex;
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
// TODO re-think with respect to other BAT setups...
u8 *GetPointer(const u32 _Address)
{
	switch (_Address >> 28)
	{
	case 0x0:
	case 0x8:
		if ((_Address & 0xfffffff) < REALRAM_SIZE)
			return m_pPhysicalRAM + (_Address & RAM_MASK);
	case 0xc:
		switch (_Address >> 24)
		{
		case 0xcc:
		case 0xcd:
			_dbg_assert_msg_(MEMMAP, 0, "GetPointer from IO Bridge doesnt work");
		case 0xc8:
			// EFB. We don't want to return a pointer here since we have no memory mapped for it.
			break;

		default:
			if ((_Address & 0xfffffff) < REALRAM_SIZE)
				return m_pPhysicalRAM + (_Address & RAM_MASK);
		}

	case 0x1:
	case 0x9:
	case 0xd:
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
		{
			if ((_Address & 0xfffffff) < EXRAM_SIZE)
				return m_pPhysicalEXRAM + (_Address & EXRAM_MASK);
		}
		else
			break;

	case 0xe:
		if (_Address < (0xE0000000 + L1_CACHE_SIZE))
			return GetCachePtr() + (_Address & L1_CACHE_MASK);
		else
			break;

	default:
		if (bFakeVMEM)
			return m_pVirtualFakeVMEM + (_Address & FAKEVMEM_MASK);
	}

	ERROR_LOG(MEMMAP, "Unknown Pointer %#8x PC %#8x LR %#8x", _Address, PC, LR);

	return NULL;
}


bool IsRAMAddress(const u32 addr, bool allow_locked_cache, bool allow_fake_vmem)
{
	switch ((addr >> 24) & 0xFC)
	{
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
		if (allow_fake_vmem && bFakeVMEM && addr >= 0x7E000000)
			return true;
		else
			return false;
	default:
		return false;
	}
}

}  // namespace
