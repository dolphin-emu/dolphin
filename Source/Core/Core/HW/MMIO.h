// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/HW/MMIOHandlers.h"

namespace MMIO
{

// There are three main MMIO blocks on the Wii (only one on the GameCube):
//  - 0x0C00xxxx: GameCube MMIOs (CP, PE, VI, PI, MI, DSP, DVD, SI, EI, AI, GP)
//  - 0x0D00xxxx: Wii MMIOs and GC mirrors (IPC, DVD, SI, EI, AI)
//  - 0x0D80xxxx: Mirror of 0x0D00xxxx.
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
	if (address == 0x0C008000)
		return false; // WG Pipe
	if ((address & 0xFFFF0000) == 0x0C000000)
		return true; // GameCube MMIOs

	if(SConfig::GetInstance().bWii)
	{
		return ((address & 0xFFFF0000) == 0x0D000000) || // Wii MMIOs
		       ((address & 0xFFFF0000) == 0x0D800000);   // Mirror of Wii MMIOs
	}

	return false;
}

// Compute the internal unique ID for a given MMIO address. This ID is computed
// from a very simple formula: (block_id << 16) | lower_16_bits(address).
//
// The block ID can easily be computed by simply checking bit 24 (CC vs. CD).
inline u32 UniqueID(u32 address)
{
	_dbg_assert_msg_(MEMMAP, ((address & 0xFFFF0000) == 0x0C000000) ||
	                         ((address & 0xFFFF0000) == 0x0D000000) ||
	                         ((address & 0xFFFF0000) == 0x0D800000),
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
	template<typename Unit>
	void RegisterRead(u32 addr, ReadHandlingMethod<Unit>* read)
	{
		GetHandlerForRead<Unit>(addr).ResetMethod(read);
	}

	template<typename Unit>
	void RegisterWrite(u32 addr, WriteHandlingMethod<Unit>* write)
	{
		GetHandlerForWrite<Unit>(addr).ResetMethod(write);
	}

	template<typename Unit>
	void Register(u32 addr, ReadHandlingMethod<Unit>* read, WriteHandlingMethod<Unit>* write)
	{
		RegisterRead(addr, read);
		RegisterWrite(addr, write);
	}

	// Direct read/write interface.
	//
	// These functions allow reading/writing an MMIO register at a given
	// address. They are used by the Memory:: access functions, which are
	// called in interpreter mode, from Dolphin's own code, or from JIT'd code
	// where the access address could not be predicted.
	template<typename Unit>
	Unit Read(u32 addr)
	{
		return GetHandlerForRead<Unit>(addr).Read(addr);
	}

	template<typename Unit>
	void Write(u32 addr, Unit val)
	{
		GetHandlerForWrite<Unit>(addr).Write(addr, val);
	}

	// Handlers access interface.
	//
	// Use when you care more about how to access the MMIO register for an
	// address than the current value of that register. For example, this is
	// what could be used to implement fast MMIO accesses in Dolphin's JIT.
	template<typename Unit>
	ReadHandler<Unit>& GetHandlerForRead(u32 addr)
	{
		return GetReadHandler<Unit>(UniqueID(addr) / sizeof(Unit));
	}

	template<typename Unit>
	WriteHandler<Unit>& GetHandlerForWrite(u32 addr)
	{
		return GetWriteHandler<Unit>(UniqueID(addr) / sizeof(Unit));
	}

private:
	// These arrays contain the handlers for each MMIO access type: read/write
	// to 8/16/32 bits. They are indexed using the UniqueID(addr) function
	// defined earlier, which maps an MMIO address to a unique ID by using the
	// MMIO block ID.
	//
	// Each array contains NUM_MMIOS / sizeof (AccessType) because larger
	// access types mean less possible adresses (assuming aligned only
	// accesses).
	template<typename Unit>
	struct HandlerArray
	{
		using Read = std::array<ReadHandler<Unit>, NUM_MMIOS / sizeof(Unit)>;
		using Write = std::array<WriteHandler<Unit>, NUM_MMIOS / sizeof(Unit)>;
	};

	HandlerArray<u8>::Read m_read_handlers8;
	HandlerArray<u16>::Read m_read_handlers16;
	HandlerArray<u32>::Read m_read_handlers32;

	HandlerArray<u8>::Write m_write_handlers8;
	HandlerArray<u16>::Write m_write_handlers16;
	HandlerArray<u32>::Write m_write_handlers32;

	// Getter functions for the handler arrays.
	//
	// TODO:
	// It would be desirable to clean these methods up using tuples, i.e. doing something like
	//
	//     auto handlers = std::tie(m_read_handlers8, m_read_handlers16, m_read_handlers32);
	//     return std::get<Unit>(handlers)[index];
	//
	// However, we cannot use this currently because of a compiler bug in clang, presumably related
	// to http://llvm.org/bugs/show_bug.cgi?id=18345, due to which the above code makes the compiler
	// exceed the template recursion depth.
	// As a workaround, we cast all handlers to the requested one's type. This cast will
	// compile to a NOP for the returned member variable, but it's necessary to get this
	// code to compile at all.
	template<typename Unit>
	ReadHandler<Unit>& GetReadHandler(size_t index)
	{
		static_assert(std::is_same<Unit, u8>::value || std::is_same<Unit, u16>::value || std::is_same<Unit, u32>::value, "Invalid unit used");
		using ArrayType = typename HandlerArray<Unit>::Read;
		ArrayType& handler = *(std::is_same<Unit,u8>::value ? (ArrayType*)&m_read_handlers8
		                                                    : std::is_same<Unit,u16>::value ? (ArrayType*)&m_read_handlers16
		                                                                                    : (ArrayType*)&m_read_handlers32);
		return handler[index];
	}

	template<typename Unit>
	WriteHandler<Unit>& GetWriteHandler(size_t index)
	{
		static_assert(std::is_same<Unit, u8>::value || std::is_same<Unit, u16>::value || std::is_same<Unit, u32>::value, "Invalid unit used");
		using ArrayType = typename HandlerArray<Unit>::Write;
		ArrayType& handler = *(std::is_same<Unit,u8>::value ? (ArrayType*)&m_write_handlers8
		                                                    : std::is_same<Unit,u16>::value ? (ArrayType*)&m_write_handlers16
		                                                                                    : (ArrayType*)&m_write_handlers32);
		return handler[index];
	}
};

// Dummy 64 bits variants of these functions. While 64 bits MMIO access is
// not supported, we need these in order to make the code compile.
template<>
inline u64 Mapping::Read<u64>(u32 addr)
{
    _dbg_assert_(MEMMAP, 0);
    return 0;
}

template<>
inline void Mapping::Write(u32 addr, u64 val)
{
    _dbg_assert_(MEMMAP, 0);
}

}
