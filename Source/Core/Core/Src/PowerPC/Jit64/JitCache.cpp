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
//#define OPROFILE_REPORT

#include <map>

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

#ifdef OPROFILE_REPORT
#include <opagent.h>
#endif

using namespace Gen;

namespace Jit64
{
#ifdef OPROFILE_REPORT
	op_agent_t agent;
#endif
	static u8 *codeCache;
	static u8 *genFunctions;
	static u8 *trampolineCache;
	u8 *trampolineCodePtr;
#define INVALID_EXIT 0xFFFFFFFF
	void LinkBlockExits(int i);
	void LinkBlock(int i);

	enum 
	{
		//CODE_SIZE = 1024*1024*8,
		GEN_SIZE = 4096,
		TRAMPOLINE_SIZE = 1024*1024,
		//MAX_NUM_BLOCKS = 65536,
	};
	int CODE_SIZE = 1024*1024*8; // nonconstant to be able to have an option for it
	int MAX_NUM_BLOCKS = 65536;

	static u8 **blockCodePointers; // cut these in half and force below 2GB?

	static std::multimap<u32, int> links_to;

	static JitBlock *blocks;
	static int numBlocks;

	void DestroyBlock(int blocknum, bool invalidate);

	void PrintStats()
	{
		LOG(DYNA_REC, "JIT Statistics =======================");
		LOG(DYNA_REC, "Number of blocks currently: %i", numBlocks);
		LOG(DYNA_REC, "Code cache size: %i b", GetCodePtr() - codeCache);
		LOG(DYNA_REC, "======================================");
	}

	void InitCache()
	{
		if(Core::g_CoreStartupParameter.bJITUnlimitedCache)
		{
			CODE_SIZE = 1024*1024*8*8;
			MAX_NUM_BLOCKS = 65536*8;
		}

		codeCache    = (u8*)AllocateExecutableMemory(CODE_SIZE);
		genFunctions = (u8*)AllocateExecutableMemory(GEN_SIZE);
		trampolineCache = (u8*)AllocateExecutableMemory(TRAMPOLINE_SIZE);
		trampolineCodePtr = trampolineCache;

#ifdef OPROFILE_REPORT
		agent = op_open_agent();
#endif
		blocks = new JitBlock[MAX_NUM_BLOCKS];
		blockCodePointers = new u8*[MAX_NUM_BLOCKS];
		ClearCache();
		SetCodePtr(genFunctions);
		Asm::Generate();
		// Protect the generated functions
		WriteProtectMemory(genFunctions, GEN_SIZE, true);
		SetCodePtr(codeCache);
	}

	void ShutdownCache()
	{
		UnWriteProtectMemory(genFunctions, GEN_SIZE, true);
		FreeMemoryPages(codeCache, CODE_SIZE);
		FreeMemoryPages(genFunctions, GEN_SIZE);
		FreeMemoryPages(trampolineCache, TRAMPOLINE_SIZE);
		delete [] blocks;
		delete [] blockCodePointers;
		blocks = 0;
		blockCodePointers = 0;
		numBlocks = 0;
#ifdef OPROFILE_REPORT
		op_close_agent(agent);
#endif
	}
	
	/* This clears the JIT cache. It's called from JitCache.cpp when the JIT cache
	   is full and when saving and loading states */
	void ClearCache()
	{
		Core::DisplayMessage("Cleared code cache.", 3000);
		// Is destroying the blocks really necessary?
		for (int i = 0; i < numBlocks; i++) {
			DestroyBlock(i, false);
		}
		links_to.clear();
		trampolineCodePtr = trampolineCache;
		numBlocks = 0;
		memset(blockCodePointers, 0, sizeof(u8*)*MAX_NUM_BLOCKS);
		memset(codeCache, 0xCC, CODE_SIZE);
		SetCodePtr(codeCache);
	}

	void DestroyBlocksWithFlag(BlockFlag death_flag)
	{
		for (int i = 0; i < numBlocks; i++) {
			if (blocks[i].flags & death_flag) {
				DestroyBlock(i, false);
			}
		}
	}

	void ResetCache()
	{
		ShutdownCache();
		InitCache();
	}

	JitBlock *CurBlock()
	{
		return &blocks[numBlocks];
	}

	JitBlock *GetBlock(int no)
	{
		return &blocks[no];
	}

	int GetNumBlocks()
	{
		return numBlocks;
	}

	bool RangeIntersect(int s1, int e1, int s2, int e2)
	{
		// check if any endpoint is inside the other range
		if ( (s1 >= s2 && s1 <= e2) ||
			 (e1 >= s2 && e1 <= e2) ||
			 (s2 >= s1 && s2 <= e1) ||
			 (e2 >= s1 && e2 <= e1)) 
			return true;
		else
			return false;
	}

	u8 *Jit(u32 emAddress)
	{
		if (GetCodePtr() >= codeCache + CODE_SIZE - 0x10000 || numBlocks >= MAX_NUM_BLOCKS - 1)
		{
			LOG(DYNA_REC, "JIT cache full - clearing.")
			if(Core::g_CoreStartupParameter.bJITUnlimitedCache)
			{
				PanicAlert("What? JIT cache still full - clearing.");
			}
			ClearCache();
		}

		JitBlock &b = blocks[numBlocks];
		b.invalid = false;
		b.originalAddress = emAddress;
		b.originalFirstOpcode = Memory::ReadFast32(emAddress);
		b.exitAddress[0] = INVALID_EXIT;
		b.exitAddress[1] = INVALID_EXIT;
		b.exitPtrs[0] = 0;
		b.exitPtrs[1] = 0;
		b.linkStatus[0] = false;
		b.linkStatus[1] = false;
		
		blockCodePointers[numBlocks] = (u8*)DoJit(emAddress, b);  //cast away const
		Memory::WriteUnchecked_U32((JIT_OPCODE << 26) | numBlocks, emAddress);

		if (jo.enableBlocklink) {
			for (int i = 0; i < 2; i++) {
				if (b.exitAddress[i] != INVALID_EXIT) {
					links_to.insert(std::pair<u32, int>(b.exitAddress[i], numBlocks));
				}
			}
			
			u8 *oldCodePtr = GetWritableCodePtr();
			LinkBlock(numBlocks);
			LinkBlockExits(numBlocks);
			SetCodePtr(oldCodePtr);
		}

#ifdef OPROFILE_REPORT
		char buf[100];
		sprintf(buf, "EmuCode%x", emAddress);
		u8* blockStart = blockCodePointers[numBlocks], *blockEnd = GetWritableCodePtr();
		op_write_native_code(agent, buf, (uint64_t)blockStart,
		                     blockStart, blockEnd - blockStart);
#endif

		numBlocks++; //commit the current block
		return 0;
	}

	void unknown_instruction(UGeckoInstruction _inst)
	{
		//	CCPU::Break();
		PanicAlert("unknown_instruction Jit64 - Fix me ;)");
		_dbg_assert_(DYNA_REC, 0);
	}
	
	u8 **GetCodePointers()
	{
		return blockCodePointers;
	}

	bool IsInJitCode(const u8 *codePtr) {
		return codePtr >= codeCache && codePtr <= GetCodePtr();
	}

	void EnterFastRun()
	{
		CompiledCode pExecAddr = (CompiledCode)Asm::enterCode;
		pExecAddr();
		//Will return when PowerPC::state changes
	}

	int GetBlockNumberFromAddress(u32 addr)
	{
		if (!blocks)
			return -1;
		u32 code = Memory::ReadFast32(addr);
		if ((code >> 26) == JIT_OPCODE)
		{
			//jitted code
			unsigned int blockNum = code & 0x03FFFFFF;
			if (blockNum >= (unsigned int)numBlocks) {
				return -1;
			}

			if (blocks[blockNum].originalAddress != addr)
			{
				//_assert_msg_(DYNA_REC, 0, "GetBlockFromAddress %08x - No match - This is BAD", addr);
				return -1;
			}
			return blockNum;
		}
		else
		{
			return -1;
		}
	}

	u32 GetOriginalCode(u32 address)
	{
		int num = GetBlockNumberFromAddress(address);
		if (num == -1)
			return Memory::ReadUnchecked_U32(address);
		else
			return blocks[num].originalFirstOpcode;
	} 

	CompiledCode GetCompiledCode(u32 address)
	{
		int num = GetBlockNumberFromAddress(address);
		if (num == -1)
			return 0;
		else
			return (CompiledCode)blockCodePointers[num];
	}

	CompiledCode GetCompiledCodeFromBlock(int blockNumber)
	{
		return (CompiledCode)blockCodePointers[blockNumber];
	}

	int GetCodeSize() {
		return (int)(GetCodePtr() - codeCache);
	}

	//Block linker
	//Make sure to have as many blocks as possible compiled before calling this
	//It's O(N), so it's fast :)
	//Can be faster by doing a queue for blocks to link up, and only process those
	//Should probably be done

	void LinkBlockExits(int i)
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
				int destinationBlock = GetBlockNumberFromAddress(b.exitAddress[e]);
				if (destinationBlock != -1)
				{
					SetCodePtr(b.exitPtrs[e]);
					JMP(blocks[destinationBlock].checkedEntry, true);
					b.linkStatus[e] = true;
				}
			}
		}
	}

	/*
					if ((b.exitAddress[0] == INVALID_EXIT || b.linkStatus[0]) &&
					(b.exitAddress[1] == INVALID_EXIT || b.linkStatus[1])) {
					unlinked.erase(iter);
					if (unlinked.size() > 4000) PanicAlert("Removed from unlinked. Size = %i", unlinked.size());
				}
*/
	using namespace std;
	void LinkBlock(int i)
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

	void DestroyBlock(int blocknum, bool invalidate)
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
		u8 *prev_code = GetWritableCodePtr();
		// Spurious entrances from previously linked blocks can only come through checkedEntry
		SetCodePtr((u8*)b.checkedEntry);
		MOV(32, M(&PC), Imm32(b.originalAddress));
		JMP(Asm::dispatcher, true);
		SetCodePtr(blockCodePointers[blocknum]);
		MOV(32, M(&PC), Imm32(b.originalAddress));
		JMP(Asm::dispatcher, true);
		SetCodePtr(prev_code);  // reset code pointer
	}

#define BLR_OP 0x4e800020

	void InvalidateCodeRange(u32 address, u32 length)
	{
		if (!jo.enableBlocklink)
			return;
		return;
		//This is slow but should be safe (zelda needs it for block linking)
		for (int i = 0; i < numBlocks; i++)
		{
			if (RangeIntersect(blocks[i].originalAddress, blocks[i].originalAddress+blocks[i].originalSize,
				               address, address + length))
			{
				DestroyBlock(i, true);
			}
		}
	}

}  // namespace
