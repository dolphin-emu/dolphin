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

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include "Common.h"
#include "../../Core.h"
#include "MemoryUtil.h"

#include "../../HW/Memmap.h"
#include "../../CoreTiming.h"

#include "../PowerPC.h"
#include "../PPCTables.h"
#include "../PPCAnalyst.h"

#include "x64Emitter.h"
#include "x64Analyzer.h"

#include "Jit.h"
#include "JitCache.h"
#include "JitAsm.h"

#include "disasm.h"

#if defined USE_OPROFILE && USE_OPROFILE
#include <opagent.h>
#endif

#if defined USE_OPROFILE && USE_OPROFILE
	op_agent_t agent;
#endif

using namespace Gen;

#define INVALID_EXIT 0xFFFFFFFF


bool JitBlock::ContainsAddress(u32 em_address)
{
	// WARNING - THIS DOES NOT WORK WITH INLINING ENABLED.
	return (em_address >= originalAddress && em_address < originalAddress + originalSize);
}

	bool JitBlockCache::IsFull() const 
	{
		return GetNumBlocks() >= MAX_NUM_BLOCKS - 1;
	}

	void JitBlockCache::Init()
	{
		MAX_NUM_BLOCKS = 65536*2;
		if (Core::g_CoreStartupParameter.bJITUnlimitedCache)
		{
			MAX_NUM_BLOCKS = 65536*8;
		}

#if defined USE_OPROFILE && USE_OPROFILE
		agent = op_open_agent();
#endif
		blocks = new JitBlock[MAX_NUM_BLOCKS];
		blockCodePointers = new const u8*[MAX_NUM_BLOCKS];

		Clear();
	}

	void JitBlockCache::Shutdown()
	{
		delete [] blocks;
		delete [] blockCodePointers;
		blocks = 0;
		blockCodePointers = 0;
		num_blocks = 0;
#if defined USE_OPROFILE && USE_OPROFILE
		op_close_agent(agent);
#endif
	}
	
	// This clears the JIT cache. It's called from JitCache.cpp when the JIT cache
	// is full and when saving and loading states.
	void JitBlockCache::Clear()
	{
		Core::DisplayMessage("Cleared code cache.", 3000);
		// Is destroying the blocks really necessary?
		for (int i = 0; i < num_blocks; i++)
		{
			DestroyBlock(i, false);
		}
		links_to.clear();
		num_blocks = 0;
		memset(blockCodePointers, 0, sizeof(u8*)*MAX_NUM_BLOCKS);
	}

	void JitBlockCache::DestroyBlocksWithFlag(BlockFlag death_flag)
	{
		for (int i = 0; i < num_blocks; i++)
		{
			if (blocks[i].flags & death_flag)
			{
				DestroyBlock(i, false);
			}
		}
	}

	void JitBlockCache::Reset()
	{
		Shutdown();
		Init();
	}

	JitBlock *JitBlockCache::GetBlock(int no)
	{
		return &blocks[no];
	}

	int JitBlockCache::GetNumBlocks() const
	{
		return num_blocks;
	}

	bool JitBlockCache::RangeIntersect(int s1, int e1, int s2, int e2) const
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

	int JitBlockCache::AllocateBlock(u32 em_address)
	{
		JitBlock &b = blocks[num_blocks];
		b.invalid = false;
		b.originalAddress = em_address;
		b.originalFirstOpcode = Memory::ReadFast32(em_address);
		b.exitAddress[0] = INVALID_EXIT;
		b.exitAddress[1] = INVALID_EXIT;
		b.exitPtrs[0] = 0;
		b.exitPtrs[1] = 0;
		b.linkStatus[0] = false;
		b.linkStatus[1] = false;
		num_blocks++; //commit the current block
		return num_blocks - 1;
	}

	void JitBlockCache::FinalizeBlock(int block_num, bool block_link, const u8 *code_ptr)
	{
		blockCodePointers[block_num] = code_ptr;
		JitBlock &b = blocks[block_num];
		Memory::WriteUnchecked_U32((JIT_OPCODE << 26) | block_num, blocks[block_num].originalAddress);
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
	}

	const u8 **JitBlockCache::GetCodePointers()
	{
		return blockCodePointers;
	}

	int JitBlockCache::GetBlockNumberFromStartAddress(u32 addr)
	{
		if (!blocks)
			return -1;
		u32 code = Memory::ReadFast32(addr);
		if ((code >> 26) == JIT_OPCODE)
		{
			// Jitted code.
			unsigned int block = code & 0x03FFFFFF;
			if (block >= (unsigned int)num_blocks) {
				return -1;
			}

			if (blocks[block].originalAddress != addr)
			{
				//_assert_msg_(DYNA_REC, 0, "GetBlockFromAddress %08x - No match - This is BAD", addr);
				return -1;
			}
			return block;
		}
		else
		{
			return -1;
		}
	}

void JitBlockCache::GetBlockNumbersFromAddress(u32 em_address, std::vector<int> *block_numbers)
{
	for (int i = 0; i < num_blocks; i++)
		if (blocks[i].ContainsAddress(em_address))
			block_numbers->push_back(i);
}

	u32 JitBlockCache::GetOriginalCode(u32 address)
	{
		int num = GetBlockNumberFromStartAddress(address);
		if (num == -1)
			return Memory::ReadUnchecked_U32(address);
		else
			return blocks[num].originalFirstOpcode;
	} 

	CompiledCode JitBlockCache::GetCompiledCodeFromBlock(int blockNumber)
	{
		return (CompiledCode)blockCodePointers[blockNumber];
	}

	//Block linker
	//Make sure to have as many blocks as possible compiled before calling this
	//It's O(N), so it's fast :)
	//Can be faster by doing a queue for blocks to link up, and only process those
	//Should probably be done

	void JitBlockCache::LinkBlockExits(int i)
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
					XEmitter emit(b.exitPtrs[e]);
					emit.JMP(blocks[destinationBlock].checkedEntry, true);
					b.linkStatus[e] = true;
				}
			}
		}
	}

	using namespace std;

	void JitBlockCache::LinkBlock(int i)
	{
		LinkBlockExits(i);
		JitBlock &b = blocks[i];
		std::map<u32, int>::iterator iter;
		pair<multimap<u32, int>::iterator, multimap<u32, int>::iterator> ppp;
		// equal_range(b) returns pair<iterator,iterator> representing the range
		// of element with key b
		ppp = links_to.equal_range(b.originalAddress);
		if (ppp.first == ppp.second)
			return;
		for (multimap<u32, int>::iterator iter2 = ppp.first; iter2 != ppp.second; ++iter2) {
			// PanicAlert("Linking block %i to block %i", iter2->second, i);
			LinkBlockExits(iter2->second);
		}
	}

	void JitBlockCache::DestroyBlock(int blocknum, bool invalidate)
	{
		u32 codebytes = (JIT_OPCODE << 26) | blocknum; //generate from i
		JitBlock &b = blocks[blocknum];
		b.invalid = 1;
		if (codebytes == Memory::ReadFast32(b.originalAddress))
		{
			//nobody has changed it, good
			Memory::WriteUnchecked_U32(b.originalFirstOpcode, b.originalAddress);
		}
		else if (!invalidate)
		{
			//PanicAlert("Detected code overwrite");
			//else, we may be in trouble, since we apparently know of this block but it's been
			//overwritten. We should have thrown it out before, on instruction cache invalidate or something.
			//Not ne cessarily bad though , if a game has simply thrown away a lot of code and is now using the space
			//for something else, then it's fine.
			LOG(MASTER_LOG, "WARNING - ClearCache detected code overwrite @ %08x", blocks[blocknum].originalAddress);
		}

		// We don't unlink blocks, we just send anyone who tries to run them back to the dispatcher.
		// Not entirely ideal, but .. pretty good.

		// TODO - make sure that the below stuff really is safe.

		// Spurious entrances from previously linked blocks can only come through checkedEntry
		XEmitter emit((u8 *)b.checkedEntry);
		emit.MOV(32, M(&PC), Imm32(b.originalAddress));
		emit.JMP(asm_routines.dispatcher, true);

		emit.SetCodePtr((u8 *)blockCodePointers[blocknum]);
		emit.MOV(32, M(&PC), Imm32(b.originalAddress));
		emit.JMP(asm_routines.dispatcher, true);
	}


	void JitBlockCache::InvalidateCodeRange(u32 address, u32 length)
	{
		if (!jit.jo.enableBlocklink)
			return;
		return;
		//This is slow but should be safe (zelda needs it for block linking)
		for (int i = 0; i < num_blocks; i++)
		{
			if (RangeIntersect(blocks[i].originalAddress, blocks[i].originalAddress + blocks[i].originalSize,
				               address, address + length))
			{
				DestroyBlock(i, true);
			}
		}
	}
