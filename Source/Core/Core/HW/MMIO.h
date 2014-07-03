// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <type_traits>

#include "Common/Common.h"
#include "Core/HW/MMIOHandlers.h"

namespace MMIO
{

// There are three main MMIO blocks on the Wii (only one on the GameCube):
//  - 0xCC00xxxx: GameCube MMIOs (CP, PE, VI, PI, MI, DSP, DVD, SI, EI, AI, GP)
//  - 0xCD00xxxx: Wii MMIOs and GC mirrors (IPC, DVD, SI, EI, AI)
//  - 0xCD80xxxx: Mirror of 0xCD00xxxx.
//
// In practice, since the third block is a mirror of the second one, we can
// assume internally that there are only two blocks: one for GC, one for Wii.
enum Block
{
	GC_BLOCK = 0,
	WII_BLOCK = 1,

	NUM_BLOCKS
};
const u32 BLOCK_SIZE = 0x10000;
const u32 NUM_MMIOS = NUM_BLOCKS * BLOCK_SIZE;

// Checks if a given physical memory address refers to the MMIO address range.
// In practice, most games use a virtual memory mapping (via BATs set in the
// IPL) that matches the physical memory mapping for MMIOs.
//
// We have a special exception here for FIFO writes: these are handled via a
// different mechanism and should not go through the normal MMIO access
// interface.
inline bool IsMMIOAddress(u32 address)
{
	return ((address & 0xE0000000) == 0xC0000000) &&
	       ((address & 0x0000FFFF) != 0x00008000);
}

// Compute the internal unique ID for a given MMIO address. This ID is computed
// from a very simple formula: (block_id << 16) | lower_16_bits(address).
//
// The block ID can easily be computed by simply checking bit 24 (CC vs. CD).
inline u32 UniqueID(u32 address)
{
	_dbg_assert_msg_(MEMMAP, ((address & 0xFFFF0000) == 0xCC000000) ||
	                         ((address & 0xFFFF0000) == 0xCD000000) ||
	                         ((address & 0xFFFF0000) == 0xCD800000),
	                 "Trying to get the ID of a non-existing MMIO address.");

	return (((address >> 24) & 1) << 16) | (address & 0xFFFF);
}

// Some utilities functions to define MMIO mappings.
namespace Utils
{
// Allow grabbing pointers to the high and low part of a 32 bits pointer.
inline u16* LowPart(u32* ptr) { return (u16*)ptr; }
inline u16* LowPart(volatile u32* ptr) { return (u16*)ptr; }
inline u16* HighPart(u32* ptr) { return LowPart(ptr) + 1; }
inline u16* HighPart(volatile u32* ptr) { return LowPart(ptr) + 1; }
}

class Mapping
{
public:
	// MMIO registration interface. Use this to register new MMIO handlers.
	//
	// Example usages can be found in just about any HW/ module in Dolphin's
	// codebase.
#define REGISTER_FUNCS(Size) \
	void RegisterRead(u32 addr, ReadHandlingMethod<u##Size>* read) \
	{ \
		u32 id = UniqueID(addr) / sizeof (u##Size); \
		m_Read##Size##Handlers[id].ResetMethod(read); \
	} \
	void RegisterWrite(u32 addr, WriteHandlingMethod<u##Size>* write) \
	{ \
		u32 id = UniqueID(addr) / sizeof (u##Size); \
		m_Write##Size##Handlers[id].ResetMethod(write); \
	} \
	void Register(u32 addr, ReadHandlingMethod<u##Size>* read, \
	              WriteHandlingMethod<u##Size>* write) \
	{ \
		RegisterRead(addr, read); \
		RegisterWrite(addr, write); \
	}
	REGISTER_FUNCS(8) REGISTER_FUNCS(16) REGISTER_FUNCS(32)
#undef REGISTER_FUNCS

	// Direct read/write interface.
	//
	// These functions allow reading/writing an MMIO register at a given
	// address. They are used by the Memory:: access functions, which are
	// called in interpreter mode, from Dolphin's own code, or from JIT'd code
	// where the access address could not be predicted.
	//
	// Note that for reads we cannot simply return the read value because C++
	// allows overloading only with parameter types, not return types.
#define READ_FUNC(Size) \
	void Read(u32 addr, u##Size* val) \
	{ \
		u32 id = UniqueID(addr) / sizeof (u##Size); \
		*val = m_Read##Size##Handlers[id].Read(addr); \
	}
	READ_FUNC(8) READ_FUNC(16) READ_FUNC(32)
#undef READ_FUNC

#define WRITE_FUNC(Size) \
	void Write(u32 addr, u##Size val) \
	{ \
		u32 id = UniqueID(addr) / sizeof (u##Size); \
		m_Write##Size##Handlers[id].Write(addr, val); \
	}
	WRITE_FUNC(8) WRITE_FUNC(16) WRITE_FUNC(32)
#undef WRITE_FUNC

	// Handlers access interface.
	//
	// Use when you care more about how to access the MMIO register for an
	// address than the current value of that register. For example, this is
	// what could be used to implement fast MMIO accesses in Dolphin's JIT.
	//
	// Two variants of each GetHandler function are provided: one that returns
	// the handler directly and one that has a pointer parameter to return the
	// value. This second variant is needed because C++ doesn't do overloads
	// based on return type but only based on argument types.
#define GET_HANDLERS_FUNC(Type, Size) \
	Type##Handler<u##Size>& GetHandlerFor##Type##Size(u32 addr) \
	{ \
		return m_##Type##Size##Handlers[UniqueID(addr) / sizeof (u##Size)]; \
	} \
	void GetHandlerFor##Type(u32 addr, Type##Handler<u##Size>** h) \
	{ \
		*h = &GetHandlerFor##Type##Size(addr); \
	}
	GET_HANDLERS_FUNC(Read, 8) GET_HANDLERS_FUNC(Read, 16) GET_HANDLERS_FUNC(Read, 32)
	GET_HANDLERS_FUNC(Write, 8) GET_HANDLERS_FUNC(Write, 16) GET_HANDLERS_FUNC(Write, 32)
#undef GET_HANDLERS_FUNC

	// Dummy 64 bits variants of these functions. While 64 bits MMIO access is
	// not supported, we need these in order to make the code compile.
	void Read(u32 addr, u64* val) { _dbg_assert_(MEMMAP, 0); }
	void Write(u32 addr, u64 val) { _dbg_assert_(MEMMAP, 0); }

private:
	// These arrays contain the handlers for each MMIO access type: read/write
	// to 8/16/32 bits. They are indexed using the UniqueID(addr) function
	// defined earlier, which maps an MMIO address to a unique ID by using the
	// MMIO block ID.
	//
	// Each array contains NUM_MMIOS / sizeof (AccessType) because larger
	// access types mean less possible adresses (assuming aligned only
	// accesses).
#define HANDLERS(Size) \
	std::array<ReadHandler<u##Size>, NUM_MMIOS / sizeof (u##Size)> m_Read##Size##Handlers; \
	std::array<WriteHandler<u##Size>, NUM_MMIOS / sizeof (u##Size)> m_Write##Size##Handlers;
	HANDLERS(8) HANDLERS(16) HANDLERS(32)
#undef HANDLERS
};

}
