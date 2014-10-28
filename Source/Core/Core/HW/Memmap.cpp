// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// NOTE:
// These functions are primarily used by the interpreter versions of the LoadStore instructions.
// However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
// may be redirected here (for example to Read_U32()).

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MemArena.h"
#include "Common/MemoryUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/AudioInterface.h"
#include "Core/HW/CPU.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/MemoryInterface.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/SI.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WII_IPC.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VideoBackendBase.h"

namespace Memory
{

// =================================
// LOCAL SETTINGS
// ----------------

// Enable the Translation Lookaside Buffer functions.
bool bFakeVMEM = false;
static bool bMMU = false;
// ==============


// =================================
// Init() declarations
// ----------------
// Store the MemArena here
u8* base = nullptr;

// The MemArena class
static MemArena g_arena;
// ==============

// STATE_TO_SAVE
static bool m_IsInitialized = false; // Save the Init(), Shutdown() state
// END STATE_TO_SAVE

// 64-bit: Pointers to low-mem (sub-0x10000000) mirror
// 32-bit: Same as the corresponding physical/virtual pointers.
u8* m_pRAM;
u8* m_pL1Cache;
u8* m_pEXRAM;
u8* m_pFakeVMEM;

// 64-bit: Pointers to high-mem mirrors
// 32-bit: Same as above
static u8* m_pPhysicalRAM;
static u8* m_pVirtualCachedRAM;
static u8* m_pVirtualUncachedRAM;
static u8* m_pPhysicalEXRAM;        // wii only
static u8* m_pVirtualCachedEXRAM;   // wii only
static u8* m_pVirtualUncachedEXRAM; // wii only
static u8* m_pVirtualL1Cache;
static u8* m_pVirtualFakeVMEM;

// MMIO mapping object.
MMIO::Mapping* mmio_mapping;

static void InitMMIO(MMIO::Mapping* mmio)
{
	g_video_backend->RegisterCPMMIO(mmio, 0xCC000000);
	PixelEngine::RegisterMMIO(mmio, 0xCC001000);
	VideoInterface::RegisterMMIO(mmio, 0xCC002000);
	ProcessorInterface::RegisterMMIO(mmio, 0xCC003000);
	MemoryInterface::RegisterMMIO(mmio, 0xCC004000);
	DSP::RegisterMMIO(mmio, 0xCC005000);
	DVDInterface::RegisterMMIO(mmio, 0xCC006000);
	SerialInterface::RegisterMMIO(mmio, 0xCC006400);
	ExpansionInterface::RegisterMMIO(mmio, 0xCC006800);
	AudioInterface::RegisterMMIO(mmio, 0xCC006C00);
}

static void InitMMIOWii(MMIO::Mapping* mmio)
{
	InitMMIO(mmio);

	WII_IPCInterface::RegisterMMIO(mmio, 0xCD000000);
	DVDInterface::RegisterMMIO(mmio, 0xCD006000);
	SerialInterface::RegisterMMIO(mmio, 0xCD006400);
	ExpansionInterface::RegisterMMIO(mmio, 0xCD006800);
	AudioInterface::RegisterMMIO(mmio, 0xCD006C00);
}

bool IsInitialized()
{
	return m_IsInitialized;
}


// We don't declare the IO region in here since its handled by other means.
static const MemoryView views[] =
{
	{&m_pRAM,      &m_pPhysicalRAM,          0x00000000, RAM_SIZE, 0},
	{nullptr,      &m_pVirtualCachedRAM,     0x80000000, RAM_SIZE, MV_MIRROR_PREVIOUS},
	{nullptr,      &m_pVirtualUncachedRAM,   0xC0000000, RAM_SIZE, MV_MIRROR_PREVIOUS},

	{&m_pL1Cache,  &m_pVirtualL1Cache,       0xE0000000, L1_CACHE_SIZE, 0},

	{&m_pFakeVMEM, &m_pVirtualFakeVMEM,      0x7E000000, FAKEVMEM_SIZE, MV_FAKE_VMEM},

	{&m_pEXRAM,    &m_pPhysicalEXRAM,        0x10000000, EXRAM_SIZE, MV_WII_ONLY},
	{nullptr,      &m_pVirtualCachedEXRAM,   0x90000000, EXRAM_SIZE, MV_WII_ONLY | MV_MIRROR_PREVIOUS},
	{nullptr,      &m_pVirtualUncachedEXRAM, 0xD0000000, EXRAM_SIZE, MV_WII_ONLY | MV_MIRROR_PREVIOUS},
};
static const int num_views = sizeof(views) / sizeof(MemoryView);

void Init()
{
	bool wii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	bMMU = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;
#ifndef _ARCH_32
	// The fake VMEM hack's address space is above the memory space that we allocate on 32bit targets
	// Disable it entirely on 32bit targets.
	bFakeVMEM = !bMMU;
#endif

	u32 flags = 0;
	if (wii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	base = MemoryMap_Setup(views, num_views, flags, &g_arena);

	mmio_mapping = new MMIO::Mapping();

	if (wii)
		InitMMIOWii(mmio_mapping);
	else
		InitMMIO(mmio_mapping);

	INFO_LOG(MEMMAP, "Memory system initialized. RAM at %p (mirrors at 0 @ %p, 0x80000000 @ %p , 0xC0000000 @ %p)",
		m_pRAM, m_pPhysicalRAM, m_pVirtualCachedRAM, m_pVirtualUncachedRAM);
	m_IsInitialized = true;
}

void DoState(PointerWrap &p)
{
	bool wii = SConfig::GetInstance().m_LocalCoreStartupParameter.bWii;
	p.DoArray(m_pPhysicalRAM, RAM_SIZE);
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
	base = nullptr;
	delete mmio_mapping;
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

static inline bool ValidCopyRange(u32 address, size_t size)
{
	return (GetPointer(address) != nullptr &&
	        GetPointer(address + u32(size)) != nullptr &&
	        size < EXRAM_SIZE); // Make sure we don't have a range spanning seperate 2 banks
}

void CopyFromEmu(void* data, u32 address, size_t size)
{
	if (!ValidCopyRange(address, size))
	{
		PanicAlert("Invalid range in CopyFromEmu. %lx bytes from 0x%08x", size, address);
		return;
	}
	memcpy(data, GetPointer(address), size);
}

void CopyToEmu(u32 address, const void* data, size_t size)
{
	if (!ValidCopyRange(address, size))
	{
		PanicAlert("Invalid range in CopyToEmu. %lx bytes to 0x%08x", size, address);
		return;
	}
	memcpy(GetPointer(address), data, size);
}

void Memset(const u32 _Address, const u8 _iValue, const u32 _iLength)
{
	u8* ptr = GetPointer(_Address);
	if (ptr != nullptr)
	{
		memset(ptr,_iValue,_iLength);
	}
	else
	{
		for (u32 i = 0; i < _iLength; i++)
			Write_U8(_iValue, _Address + i);
	}
}

void ClearCacheLine(const u32 _Address)
{
	u8* ptr = GetPointer(_Address);
	if (ptr != nullptr)
	{
		memset(ptr, 0, 32);
	}
	else
	{
		for (u32 i = 0; i < 32; i += 8)
			Write_U64(0, _Address + i);
	}
}

void DMA_LCToMemory(const u32 _MemAddr, const u32 _CacheAddr, const u32 _iNumBlocks)
{
	const u8* src = m_pL1Cache + (_CacheAddr & 0x3FFFF);
	u8* dst = GetPointer(_MemAddr);

	if ((dst != nullptr) && (src != nullptr) && (_MemAddr & 3) == 0 && (_CacheAddr & 3) == 0)
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
	const u8* src = GetPointer(_MemAddr);
	u8* dst = m_pL1Cache + (_CacheAddr & 0x3FFFF);

	if ((dst != nullptr) && (src != nullptr) && (_MemAddr & 3) == 0 && (_CacheAddr & 3) == 0)
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

std::string GetString(u32 em_address, size_t size)
{
	const char* ptr = reinterpret_cast<const char*>(GetPointer(em_address));
	if (size == 0) // Null terminated string.
	{
		return std::string(ptr);
	}
	else // Fixed size string, potentially null terminated or null padded.
	{
		size_t length = strnlen(ptr, size);
		return std::string(ptr, length);
	}
}

// GetPointer must always return an address in the bottom 32 bits of address space, so that 64-bit
// programs don't have problems directly addressing any part of memory.
// TODO re-think with respect to other BAT setups...
u8* GetPointer(const u32 _Address)
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
			return m_pL1Cache + (_Address & L1_CACHE_MASK);
		else
			break;

	default:
		if (bFakeVMEM)
			return m_pVirtualFakeVMEM + (_Address & FAKEVMEM_MASK);
	}

	ERROR_LOG(MEMMAP, "Unknown Pointer %#8x PC %#8x LR %#8x", _Address, PC, LR);

	return nullptr;
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
