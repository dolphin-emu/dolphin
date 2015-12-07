// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// NOTE:
// These functions are primarily used by the interpreter versions of the LoadStore instructions.
// However, if a JITed instruction (for example lwz) wants to access a bad memory area that call
// may be redirected here (for example to Read_U32()).

#include <cstring>

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
// (See comment below describing memory map.)
bool bFakeVMEM = false;

// =================================
// Init() declarations
// ----------------
// Store the MemArena here
u8* physical_base = nullptr;
u8* logical_base = nullptr;

// The MemArena class
static MemArena g_arena;
// ==============

// STATE_TO_SAVE
static bool m_IsInitialized = false; // Save the Init(), Shutdown() state
// END STATE_TO_SAVE

u8* m_pRAM;
u8* m_pL1Cache;
u8* m_pEXRAM;
u8* m_pFakeVMEM;

// MMIO mapping object.
MMIO::Mapping* mmio_mapping;

static void InitMMIO(MMIO::Mapping* mmio)
{
	g_video_backend->RegisterCPMMIO(mmio, 0x0C000000);
	PixelEngine::RegisterMMIO(mmio, 0x0C001000);
	VideoInterface::RegisterMMIO(mmio, 0x0C002000);
	ProcessorInterface::RegisterMMIO(mmio, 0x0C003000);
	MemoryInterface::RegisterMMIO(mmio, 0x0C004000);
	DSP::RegisterMMIO(mmio, 0x0C005000);
	DVDInterface::RegisterMMIO(mmio, 0x0C006000);
	SerialInterface::RegisterMMIO(mmio, 0x0C006400);
	ExpansionInterface::RegisterMMIO(mmio, 0x0C006800);
	AudioInterface::RegisterMMIO(mmio, 0x0C006C00);
}

static void InitMMIOWii(MMIO::Mapping* mmio)
{
	InitMMIO(mmio);

	WII_IPCInterface::RegisterMMIO(mmio, 0x0D000000);
	DVDInterface::RegisterMMIO(mmio, 0x0D006000);
	SerialInterface::RegisterMMIO(mmio, 0x0D006400);
	ExpansionInterface::RegisterMMIO(mmio, 0x0D006800);
	AudioInterface::RegisterMMIO(mmio, 0x0D006C00);
}

bool IsInitialized()
{
	return m_IsInitialized;
}


// Dolphin allocates memory to represent four regions:
// - 32MB RAM (actually 24MB on hardware), available on Gamecube and Wii
// - 64MB "EXRAM", RAM only available on Wii
// - 32MB FakeVMem, allocated in GameCube mode when MMU support is turned off.
//   This is used to approximate the behavior of a common library which pages
//   memory to and from the DSP's dedicated RAM. The DSP's RAM (ARAM) isn't
//   directly addressable on GameCube.
// - 256KB Locked L1, to represent cache lines allocated out of the L1 data
//   cache in Locked L1 mode.  Dolphin does not emulate this hardware feature
//   accurately; it just pretends there is extra memory at 0xE0000000.
//
// The 4GB starting at physical_base represents access from the CPU
// with address translation turned off. (This is only used by the CPU;
// other devices, like the GPU, use other rules, approximated by
// Memory::GetPointer.) This memory is laid out as follows:
// [0x00000000, 0x02000000) - 32MB RAM
// [0x02000000, 0x08000000) - Mirrors of 32MB RAM
// [0x08000000, 0x0C000000) - EFB "mapping" (not handled here)
// [0x0C000000, 0x0E000000) - MMIO etc. (not handled here)
// [0x10000000, 0x14000000) - 64MB RAM (Wii-only; slightly slower)
//
// The 4GB starting at logical_base represents access from the CPU
// with address translation turned on.  Instead of changing the mapping
// based on the BAT registers, we approximate the common BAT configuration
// used by games:
// [0x00000000, 0x02000000) - 32MB RAM, cached access, normally only mapped
//                            during startup by Wii WADs
// [0x02000000, 0x08000000) - Mirrors of 32MB RAM (not implemented here)
// [0x40000000, 0x50000000) - FakeVMEM
// [0x70000000, 0x80000000) - FakeVMEM
// [0x80000000, 0x82000000) - 32MB RAM, cached access
// [0x82000000, 0x88000000) - Mirrors of 32MB RAM (not implemented here)
// [0x90000000, 0x94000000) - 64MB RAM, Wii-only, cached access
// [0xC0000000, 0xC2000000) - 32MB RAM, uncached access
// [0xC2000000, 0xC8000000) - Mirrors of 32MB RAM (not implemented here)
// [0xC8000000, 0xCC000000) - EFB "mapping" (not handled here)
// [0xCC000000, 0xCE000000) - MMIO etc. (not handled here)
// [0xD0000000, 0xD4000000) - 64MB RAM, Wii-only, uncached access
// [0xE0000000, 0xE0040000) - 256KB locked L1
//
// TODO: We shouldn't hardcode this mapping; we can generate it dynamically
// based on the BAT registers.
//
// Each of these 4GB regions is followed by 4GB of empty space so overflows
// in address computation in the JIT don't access the wrong memory.
//
// The neighboring mirrors of RAM ([0x02000000, 0x08000000), etc.) exist because
// the bus masks off the bits in question for RAM accesses; using them is a
// terrible idea because the CPU cache won't handle them correctly, but a
// few buggy games (notably Rogue Squadron 2) use them by accident. They
// aren't backed by memory mappings because they are used very rarely.
//
// Dolphin doesn't emulate the difference between cached and uncached access.
//
// TODO: The actual size of RAM is REALRAM_SIZE (24MB); the other 8MB shouldn't
// be backed by actual memory.
static MemoryView views[] =
{
	{&m_pRAM,      0x00000000, RAM_SIZE,      0},
	{nullptr,      0x200000000, RAM_SIZE,     MV_MIRROR_PREVIOUS},
	{nullptr,      0x280000000, RAM_SIZE,     MV_MIRROR_PREVIOUS},
	{nullptr,      0x2C0000000, RAM_SIZE,     MV_MIRROR_PREVIOUS},
	{&m_pL1Cache,  0x2E0000000, L1_CACHE_SIZE, 0},
	{&m_pFakeVMEM, 0x27E000000, FAKEVMEM_SIZE, MV_FAKE_VMEM},
	{&m_pEXRAM,    0x10000000, EXRAM_SIZE,    MV_WII_ONLY},
	{nullptr,      0x290000000, EXRAM_SIZE,   MV_WII_ONLY | MV_MIRROR_PREVIOUS},
	{nullptr,      0x2D0000000, EXRAM_SIZE,   MV_WII_ONLY | MV_MIRROR_PREVIOUS},
};
static const int num_views = sizeof(views) / sizeof(MemoryView);

void Init()
{
	bool wii = SConfig::GetInstance().bWii;
	bool bMMU = SConfig::GetInstance().bMMU;
#ifndef _ARCH_32
	// If MMU is turned off in GameCube mode, turn on fake VMEM hack.
	// The fake VMEM hack's address space is above the memory space that we
	// allocate on 32bit targets, so disable it there.
	bFakeVMEM = !wii && !bMMU;
#endif

	u32 flags = 0;
	if (wii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	physical_base = MemoryMap_Setup(views, num_views, flags, &g_arena);
#ifndef _ARCH_32
	logical_base = physical_base + 0x200000000;
#endif

	mmio_mapping = new MMIO::Mapping();

	if (wii)
		InitMMIOWii(mmio_mapping);
	else
		InitMMIO(mmio_mapping);

	INFO_LOG(MEMMAP, "Memory system initialized. RAM at %p", m_pRAM);
	m_IsInitialized = true;
}

void DoState(PointerWrap &p)
{
	bool wii = SConfig::GetInstance().bWii;
	p.DoArray(m_pRAM, RAM_SIZE);
	p.DoArray(m_pL1Cache, L1_CACHE_SIZE);
	p.DoMarker("Memory RAM");
	if (bFakeVMEM)
		p.DoArray(m_pFakeVMEM, FAKEVMEM_SIZE);
	p.DoMarker("Memory FakeVMEM");
	if (wii)
		p.DoArray(m_pEXRAM, EXRAM_SIZE);
	p.DoMarker("Memory EXRAM");
}

void Shutdown()
{
	m_IsInitialized = false;
	u32 flags = 0;
	if (SConfig::GetInstance().bWii) flags |= MV_WII_ONLY;
	if (bFakeVMEM) flags |= MV_FAKE_VMEM;
	MemoryMap_Shutdown(views, num_views, flags, &g_arena);
	g_arena.ReleaseSHMSegment();
	physical_base = nullptr;
	logical_base = nullptr;
	delete mmio_mapping;
	INFO_LOG(MEMMAP, "Memory system shut down.");
}

void Clear()
{
	if (m_pRAM)
		memset(m_pRAM, 0, RAM_SIZE);
	if (m_pL1Cache)
		memset(m_pL1Cache, 0, L1_CACHE_SIZE);
	if (SConfig::GetInstance().bWii && m_pEXRAM)
		memset(m_pEXRAM, 0, EXRAM_SIZE);
}

bool AreMemoryBreakpointsActivated()
{
#ifdef ENABLE_MEM_CHECK
	return true;
#else
	return false;
#endif
}

static inline u8* GetPointerForRange(u32 address, size_t size)
{
	// Make sure we don't have a range spanning 2 separate banks
	if (size >= EXRAM_SIZE)
		return nullptr;

	// Check that the beginning and end of the range are valid
	u8* pointer = GetPointer(address);
	if (!pointer || !GetPointer(address + u32(size) - 1))
		return nullptr;

	return pointer;
}

void CopyFromEmu(void* data, u32 address, size_t size)
{
	if (size == 0)
		return;

	void* pointer = GetPointerForRange(address, size);
	if (!pointer)
	{
		PanicAlert("Invalid range in CopyFromEmu. %zx bytes from 0x%08x", size, address);
		return;
	}
	memcpy(data, pointer, size);
}

void CopyToEmu(u32 address, const void* data, size_t size)
{
	if (size == 0)
		return;

	void* pointer = GetPointerForRange(address, size);
	if (!pointer)
	{
		PanicAlert("Invalid range in CopyToEmu. %zx bytes to 0x%08x", size, address);
		return;
	}
	memcpy(pointer, data, size);
}

void Memset(u32 address, u8 value, size_t size)
{
	if (size == 0)
		return;

	void* pointer = GetPointerForRange(address, size);
	if (!pointer)
	{
		PanicAlert("Invalid range in Memset. %zx bytes at 0x%08x", size, address);
		return;
	}
	memset(pointer, value, size);
}

std::string GetString(u32 em_address, size_t size)
{
	const char* ptr = reinterpret_cast<const char*>(GetPointer(em_address));
	if (ptr == nullptr)
		return "";

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

u8* GetPointer(u32 address)
{
	// TODO: Should we be masking off more bits here?  Can all devices access
	// EXRAM?
	address &= 0x3FFFFFFF;
	if (address < REALRAM_SIZE)
		return m_pRAM + address;

	if (SConfig::GetInstance().bWii)
	{
		if ((address >> 28) == 0x1 && (address & 0x0fffffff) < EXRAM_SIZE)
			return m_pEXRAM + (address & EXRAM_MASK);
	}

	PanicAlert("Unknown Pointer 0x%08x PC 0x%08x LR 0x%08x", address, PC, LR);

	return nullptr;
}

u8 Read_U8(u32 address)
{
	return *GetPointer(address);
}

u16 Read_U16(u32 address)
{
	return Common::swap16(GetPointer(address));
}

u32 Read_U32(u32 address)
{
	return Common::swap32(GetPointer(address));
}

u64 Read_U64(u32 address)
{
	return Common::swap64(GetPointer(address));
}

void Write_U8(u8 value, u32 address)
{
	*GetPointer(address) = value;
}

void Write_U16(u16 value, u32 address)
{
	u16 swapped_value = Common::swap16(value);
	std::memcpy(GetPointer(address), &swapped_value, sizeof(u16));
}

void Write_U32(u32 value, u32 address)
{
	u32 swapped_value = Common::swap32(value);
	std::memcpy(GetPointer(address), &swapped_value, sizeof(u32));
}

void Write_U64(u64 value, u32 address)
{
	u64 swapped_value = Common::swap64(value);
	std::memcpy(GetPointer(address), &swapped_value, sizeof(u64));
}

void Write_U32_Swap(u32 value, u32 address)
{
	std::memcpy(GetPointer(address), &value, sizeof(u32));
}

void Write_U64_Swap(u64 value, u32 address)
{
	std::memcpy(GetPointer(address), &value, sizeof(u64));
}

}  // namespace
