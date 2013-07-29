// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include "Common.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include "../../Core.h"
#include "MemoryUtil.h"

#include "../../HW/Memmap.h"
#include "../JitInterface.h"
#include "../../CoreTiming.h"

#include "../PowerPC.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"

#include "JitCache.h"
#include "JitBase.h"

#include "disasm.h"

#if defined USE_OPROFILE && USE_OPROFILE
#include <opagent.h>

op_agent_t agent;
#endif

#if defined USE_VTUNE
#include <jitprofiling.h>
#pragma comment(lib, "libittnotify.lib")
#pragma comment(lib, "jitprofiling.lib")
#endif

using namespace Gen;

#define INVALID_EXIT 0xFFFFFFFF

bool JitBlock::ContainsAddress(u32 em_address)
{
	// WARNING - THIS DOES NOT WORK WITH INLINING ENABLED.
	return (em_address >= originalAddress && em_address < originalAddress + 4 * originalSize);
}

	bool JitBaseBlockCache::IsFull() const 
	{
		return GetNumBlocks() >= MAX_NUM_BLOCKS - 1;
	}

	void JitBaseBlockCache::Init()
	{
		MAX_NUM_BLOCKS = 65536*2;

#if defined USE_OPROFILE && USE_OPROFILE
		agent = op_open_agent();
#endif
		blocks = new JitBlock[MAX_NUM_BLOCKS];
		blockCodePointers = new const u8*[MAX_NUM_BLOCKS];
#ifdef JIT_UNLIMITED_ICACHE
		if (iCache == 0 && iCacheEx == 0 && iCacheVMEM == 0)
		{
			iCache = new u8[JIT_ICACHE_SIZE];
			iCacheEx = new u8[JIT_ICACHEEX_SIZE];
			iCacheVMEM = new u8[JIT_ICACHE_SIZE];
		}
		else
		{
			PanicAlert("JitBaseBlockCache::Init() - iCache is already initialized");
		}
		if (iCache == 0 || iCacheEx == 0 || iCacheVMEM == 0)
		{
			PanicAlert("JitBaseBlockCache::Init() - unable to allocate iCache");
		}
		memset(iCache, JIT_ICACHE_INVALID_BYTE, JIT_ICACHE_SIZE);
		memset(iCacheEx, JIT_ICACHE_INVALID_BYTE, JIT_ICACHEEX_SIZE);
		memset(iCacheVMEM, JIT_ICACHE_INVALID_BYTE, JIT_ICACHE_SIZE);
#endif
		Clear();
	}

	void JitBaseBlockCache::Shutdown()
	{
		delete[] blocks;
		delete[] blockCodePointers;
#ifdef JIT_UNLIMITED_ICACHE		
		if (iCache != 0)
			delete[] iCache;
		iCache = 0;
		if (iCacheEx != 0)
			delete[] iCacheEx;
		iCacheEx = 0;
		if (iCacheVMEM != 0)
			delete[] iCacheVMEM;
		iCacheVMEM = 0;
#endif
		blocks = 0;
		blockCodePointers = 0;
		num_blocks = 0;
#if defined USE_OPROFILE && USE_OPROFILE
		op_close_agent(agent);
#endif

#ifdef USE_VTUNE
		iJIT_NotifyEvent(iJVM_EVENT_TYPE_SHUTDOWN, NULL);
#endif
	}
	
	// This clears the JIT cache. It's called from JitCache.cpp when the JIT cache
	// is full and when saving and loading states.
	void JitBaseBlockCache::Clear()
	{
#if defined(_DEBUG) || defined(DEBUGFAST)
		if (IsFull())
			Core::DisplayMessage("Clearing block cache.", 3000);
		else
			Core::DisplayMessage("Clearing code cache.", 3000);
#endif

		for (int i = 0; i < num_blocks; i++)
		{
			DestroyBlock(i, false);
		}
		links_to.clear();
		block_map.clear();
		valid_block.reset();
		num_blocks = 0;
		memset(blockCodePointers, 0, sizeof(u8*)*MAX_NUM_BLOCKS);
	}

	void JitBaseBlockCache::ClearSafe()
	{
#ifdef JIT_UNLIMITED_ICACHE
		memset(iCache, JIT_ICACHE_INVALID_BYTE, JIT_ICACHE_SIZE);
		memset(iCacheEx, JIT_ICACHE_INVALID_BYTE, JIT_ICACHEEX_SIZE);
		memset(iCacheVMEM, JIT_ICACHE_INVALID_BYTE, JIT_ICACHE_SIZE);
#endif
	}

	/*void JitBaseBlockCache::DestroyBlocksWithFlag(BlockFlag death_flag)
	{
		for (int i = 0; i < num_blocks; i++)
		{
			if (blocks[i].flags & death_flag)
			{
				DestroyBlock(i, false);
			}
		}
	}*/

	void JitBaseBlockCache::Reset()
	{
		Shutdown();
		Init();
	}

	JitBlock *JitBaseBlockCache::GetBlock(int no)
	{
		return &blocks[no];
	}

	int JitBaseBlockCache::GetNumBlocks() const
	{
		return num_blocks;
	}

	bool JitBaseBlockCache::RangeIntersect(int s1, int e1, int s2, int e2) const
	{
		// check if any endpoint is inside the other range
		if ((s1 >= s2 && s1 <= e2) ||
			(e1 >= s2 && e1 <= e2) ||
			(s2 >= s1 && s2 <= e1) ||
			(e2 >= s1 && e2 <= e1)) 
			return true;
		else
			return false;
	}

	int JitBaseBlockCache::AllocateBlock(u32 em_address)
	{
		JitBlock &b = blocks[num_blocks];
		b.invalid = false;
		b.originalAddress = em_address;
		b.exitAddress[0] = INVALID_EXIT;
		b.exitAddress[1] = INVALID_EXIT;
		b.exitPtrs[0] = 0;
		b.exitPtrs[1] = 0;
		b.linkStatus[0] = false;
		b.linkStatus[1] = false;
		b.blockNum = num_blocks;
		num_blocks++; //commit the current block
		return num_blocks - 1;
	}

	void JitBaseBlockCache::FinalizeBlock(int block_num, bool block_link, const u8 *code_ptr)
	{
		blockCodePointers[block_num] = code_ptr;
		JitBlock &b = blocks[block_num];
		b.originalFirstOpcode = JitInterface::Read_Opcode_JIT(b.originalAddress);
		JitInterface::Write_Opcode_JIT(b.originalAddress, (JIT_OPCODE << 26) | block_num);

		// Convert the logical address to a physical address for the block map
		u32 pAddr = b.originalAddress & 0x1FFFFFFF;

		for (u32 i = 0; i < (b.originalSize + 7) / 8; ++i)
			valid_block[pAddr / 32 + i] = true;

		block_map[std::make_pair(pAddr + 4 * b.originalSize - 1, pAddr)] = block_num;
		if (block_link)
		{
			for (int i = 0; i < 2; i++)
			{
				if (b.exitAddress[i] != INVALID_EXIT) 
					links_to.insert(std::pair<u32, int>(b.exitAddress[i], block_num));
			}
			
			LinkBlock(block_num);
			LinkBlockExits(block_num);
		}

#if defined USE_OPROFILE && USE_OPROFILE
		char buf[100];
		sprintf(buf, "EmuCode%x", b.originalAddress);
		const u8* blockStart = blockCodePointers[block_num];
		op_write_native_code(agent, buf, (uint64_t)blockStart,
		                     blockStart, b.codeSize);
#endif

#ifdef USE_VTUNE
		sprintf(b.blockName, "EmuCode_0x%08x", b.originalAddress);

		iJIT_Method_Load jmethod = {0};
		jmethod.method_id = iJIT_GetNewMethodID();
		jmethod.class_file_name = "";
		jmethod.source_file_name = __FILE__;
		jmethod.method_load_address = (void*)blockCodePointers[block_num];
		jmethod.method_size = b.codeSize;
		jmethod.line_number_size = 0;
		jmethod.method_name = b.blockName;
		iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jmethod);
#endif
	}

	const u8 **JitBaseBlockCache::GetCodePointers()
	{
		return blockCodePointers;
	}

#ifdef JIT_UNLIMITED_ICACHE
	u8* JitBaseBlockCache::GetICache()
	{
		return iCache;
	}

	u8* JitBaseBlockCache::GetICacheEx()
	{
		return iCacheEx;
	}

	u8* JitBaseBlockCache::GetICacheVMEM()
	{
		return iCacheVMEM;
	}
#endif

	int JitBaseBlockCache::GetBlockNumberFromStartAddress(u32 addr)
	{
		if (!blocks)
			return -1;		
#ifdef JIT_UNLIMITED_ICACHE
		u32 inst;
		if (addr & JIT_ICACHE_VMEM_BIT)
		{
			inst = *(u32*)(iCacheVMEM + (addr & JIT_ICACHE_MASK));
		}
		else if (addr & JIT_ICACHE_EXRAM_BIT)
		{
			inst = *(u32*)(iCacheEx + (addr & JIT_ICACHEEX_MASK));
		}
		else
		{
			inst = *(u32*)(iCache + (addr & JIT_ICACHE_MASK));
		}
		inst = Common::swap32(inst);
#else
		u32 inst = Memory::ReadFast32(addr);
#endif
		if (inst & 0xfc000000) // definitely not a JIT block
			return -1;
		if ((int)inst >= num_blocks)
			return -1;
		if (blocks[inst].originalAddress != addr)
			return -1;		
		return inst;
	}

	void JitBaseBlockCache::GetBlockNumbersFromAddress(u32 em_address, std::vector<int> *block_numbers)
	{
		for (int i = 0; i < num_blocks; i++)
			if (blocks[i].ContainsAddress(em_address))
				block_numbers->push_back(i);
	}

	u32 JitBaseBlockCache::GetOriginalFirstOp(int block_num)
	{
		if (block_num >= num_blocks)
		{
			//PanicAlert("JitBaseBlockCache::GetOriginalFirstOp - block_num = %u is out of range", block_num);
			return block_num;
		}
		return blocks[block_num].originalFirstOpcode;
	}

	CompiledCode JitBaseBlockCache::GetCompiledCodeFromBlock(int block_num)
	{		
		return (CompiledCode)blockCodePointers[block_num];
	}

	//Block linker
	//Make sure to have as many blocks as possible compiled before calling this
	//It's O(N), so it's fast :)
	//Can be faster by doing a queue for blocks to link up, and only process those
	//Should probably be done

	void JitBaseBlockCache::LinkBlockExits(int i)
	{
		JitBlock &b = blocks[i];
		if (b.invalid)
		{
			// This block is dead. Don't relink it.
			return;
		}
		for (int e = 0; e < 2; e++)
		{
			if (b.exitAddress[e] != INVALID_EXIT && !b.linkStatus[e])
			{
				int destinationBlock = GetBlockNumberFromStartAddress(b.exitAddress[e]);
				if (destinationBlock != -1)
				{
					WriteLinkBlock(b.exitPtrs[e], blocks[destinationBlock].checkedEntry);
					b.linkStatus[e] = true;
				}
			}
		}
	}

	using namespace std;

	void JitBaseBlockCache::LinkBlock(int i)
	{
		LinkBlockExits(i);
		JitBlock &b = blocks[i];
		pair<multimap<u32, int>::iterator, multimap<u32, int>::iterator> ppp;
		// equal_range(b) returns pair<iterator,iterator> representing the range
		// of element with key b
		ppp = links_to.equal_range(b.originalAddress);
		if (ppp.first == ppp.second)
			return;
		for (multimap<u32, int>::iterator iter = ppp.first; iter != ppp.second; ++iter) {
			// PanicAlert("Linking block %i to block %i", iter->second, i);
			LinkBlockExits(iter->second);
		}
	}

	void JitBaseBlockCache::UnlinkBlock(int i)
	{
		JitBlock &b = blocks[i];
		pair<multimap<u32, int>::iterator, multimap<u32, int>::iterator> ppp;
		ppp = links_to.equal_range(b.originalAddress);
		if (ppp.first == ppp.second)
			return;
		for (multimap<u32, int>::iterator iter = ppp.first; iter != ppp.second; ++iter) {
			JitBlock &sourceBlock = blocks[iter->second];
			for (int e = 0; e < 2; e++)
			{
				if (sourceBlock.exitAddress[e] == b.originalAddress)
					sourceBlock.linkStatus[e] = false;
			}
		}
	}

	void JitBaseBlockCache::DestroyBlock(int block_num, bool invalidate)
	{
		if (block_num < 0 || block_num >= num_blocks)
		{
			PanicAlert("DestroyBlock: Invalid block number %d", block_num);
			return;
		}
		JitBlock &b = blocks[block_num];
		if (b.invalid)
		{
			if (invalidate)
				PanicAlert("Invalidating invalid block %d", block_num);
			return;
		}
		b.invalid = true;
#ifdef JIT_UNLIMITED_ICACHE
		JitInterface::Write_Opcode_JIT(b.originalAddress, b.originalFirstOpcode?b.originalFirstOpcode:JIT_ICACHE_INVALID_WORD);
#else
		if (Memory::ReadFast32(b.originalAddress) == block_num)
			Memory::WriteUnchecked_U32(b.originalFirstOpcode, b.originalAddress);
#endif

		UnlinkBlock(block_num);

		// Send anyone who tries to run this block back to the dispatcher.
		// Not entirely ideal, but .. pretty good.
		// Spurious entrances from previously linked blocks can only come through checkedEntry
		WriteDestroyBlock(b.checkedEntry, b.originalAddress);
	}

	void JitBaseBlockCache::InvalidateICache(u32 address, const u32 length)
	{
		// Convert the logical address to a physical address for the block map
		u32 pAddr = address & 0x1FFFFFFF;

		// Optimize the common case of length == 32 which is used by Interpreter::dcb*
		bool destroy_block = true;
		if (length == 32)
	{
			if (!valid_block[pAddr / 32])
				destroy_block = false;
			else
				valid_block[pAddr / 32] = false;
		}

		// destroy JIT blocks
		// !! this works correctly under assumption that any two overlapping blocks end at the same address
		if (destroy_block)
		{
			std::map<pair<u32,u32>, u32>::iterator it1 = block_map.lower_bound(std::make_pair(pAddr, 0)), it2 = it1;
			while (it2 != block_map.end() && it2->first.second < pAddr + length)
		{
#ifdef JIT_UNLIMITED_ICACHE
				JitBlock &b = blocks[it2->second];
				if (b.originalAddress & JIT_ICACHE_VMEM_BIT)
				{
					u32 cacheaddr = b.originalAddress & JIT_ICACHE_MASK;
					memset(iCacheVMEM + cacheaddr, JIT_ICACHE_INVALID_BYTE, 4);
				}
				else if (b.originalAddress & JIT_ICACHE_EXRAM_BIT)
				{
					u32 cacheaddr = b.originalAddress & JIT_ICACHEEX_MASK;
					memset(iCacheEx + cacheaddr, JIT_ICACHE_INVALID_BYTE, 4);
				}
				else
				{
					u32 cacheaddr = b.originalAddress & JIT_ICACHE_MASK;
					memset(iCache + cacheaddr, JIT_ICACHE_INVALID_BYTE, 4);
				}
#endif
				DestroyBlock(it2->second, true);
				it2++;
			}
			if (it1 != it2)
			{
				block_map.erase(it1, it2);
			}
		}

#ifdef JIT_UNLIMITED_ICACHE
		// invalidate iCache.
		// icbi can be called with any address, so we should check
		if ((address & ~JIT_ICACHE_MASK) != 0x80000000 && (address & ~JIT_ICACHE_MASK) != 0x00000000 &&
			(address & ~JIT_ICACHE_MASK) != 0x7e000000 && // TLB area
			(address & ~JIT_ICACHEEX_MASK) != 0x90000000 && (address & ~JIT_ICACHEEX_MASK) != 0x10000000)
		{
			return;
		}
		if (address & JIT_ICACHE_VMEM_BIT)
		{
			u32 cacheaddr = address & JIT_ICACHE_MASK;
			memset(iCacheVMEM + cacheaddr, JIT_ICACHE_INVALID_BYTE, length);
		}
		else if (address & JIT_ICACHE_EXRAM_BIT)
		{
			u32 cacheaddr = address & JIT_ICACHEEX_MASK;
			memset(iCacheEx + cacheaddr, JIT_ICACHE_INVALID_BYTE, length);
		}
		else
		{
			u32 cacheaddr = address & JIT_ICACHE_MASK;
			memset(iCache + cacheaddr, JIT_ICACHE_INVALID_BYTE, length);
		}
#endif
	}
	void JitBlockCache::WriteLinkBlock(u8* location, const u8* address)
	{
		XEmitter emit(location);
		emit.JMP(address, true);
	}
	void JitBlockCache::WriteDestroyBlock(const u8* location, u32 address)
	{
		XEmitter emit((u8 *)location);
		emit.MOV(32, M(&PC), Imm32(address));
		emit.JMP(jit->GetAsmRoutines()->dispatcher, true);
	}
