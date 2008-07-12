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
#include <map>

#include "Common.h"
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

using namespace Gen;

namespace Jit64
{
	u8 *codeCache;
	u8 *genFunctions;
	u8 *trampolineCache;
	u8 *trampolineCodePtr;
#define INVALID_EXIT 0xFFFFFFFF

	enum 
	{
		CODE_SIZE = 1024*1024*8,
		GEN_SIZE = 4096,
		TRAMPOLINE_SIZE = 1024*1024,
		MAX_NUM_BLOCKS = 65536,
	};

	u8 **blockCodePointers; // cut these in half and force below 2GB?

	// todo - replace with something faster
	std::map<u32, int> unlinked;

	JitBlock *blocks;
	int numBlocks;

	//stats
	int numFlushes;

	void PrintStats()
	{
		LOG(DYNA_REC, "JIT Statistics =======================");
		LOG(DYNA_REC, "Number of full cache flushes: %i", numFlushes);
		LOG(DYNA_REC, "Number of blocks currently: %i", numBlocks);
		LOG(DYNA_REC, "Code cache size: %i b", GetCodePtr() - codeCache);
		LOG(DYNA_REC, "======================================");
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

	void LinkBlocksCallback(u64 userdata, int cyclesLate)
	{
		LinkBlocks();
	}

	u8 *Jit(u32 emaddress)
	{
		if (GetCodePtr() >= codeCache + CODE_SIZE - 0x10000 || numBlocks>=MAX_NUM_BLOCKS-1)
		{
			//PanicAlert("Cleared cache");
			LOG(DYNA_REC, "JIT code cache full - clearing cache and restoring memory")
			ClearCache();
		}
		JitBlock &b = blocks[numBlocks];
		b.originalAddress = emaddress;
		b.exitAddress[0] = INVALID_EXIT;
		b.exitAddress[1] = INVALID_EXIT;
		b.exitPtrs[0] = 0;
		b.exitPtrs[1] = 0;
		b.linkStatus[0] = false;
		b.linkStatus[1] = false;
		b.originalFirstOpcode = Memory::ReadFast32(emaddress);
		
		blockCodePointers[numBlocks] = (u8*)DoJit(emaddress, b); //cast away const, evil
		Memory::WriteUnchecked_U32((JIT_OPCODE << 26) | numBlocks, emaddress);

		//Flatten should also compute exits
		//Success!

		if (jo.enableBlocklink) {
			for (int i = 0; i < 2; i++) {
				if (b.exitAddress[0] != INVALID_EXIT)
				{
					unlinked[b.exitAddress[0]] = numBlocks;
				}
			}
			// Link max once per half billion cycles
			//LinkBlocks();
			/*if (!CoreTiming::IsScheduled(&LinkBlocksCallback))
			{
				CoreTiming::ScheduleEvent(50000000, &LinkBlocksCallback, "Link JIT blocks", 0);
			}*/
			//if ((numBlocks & 1) == 0)
				LinkBlocks();
		}
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

	void InitCache()
	{
		jo.optimizeStack = true;
		jo.enableBlocklink = false;  // Large speed boost but currently causes slowdowns too, due to stupid O(n^2) algo :P
		jo.enableFastMem = true;
		jo.noAssumeFPLoadFromMem = false;
		jo.fpAccurateFlags = true;

		codeCache    = (u8*)AllocateExecutableMemory(CODE_SIZE);
		genFunctions = (u8*)AllocateExecutableMemory(GEN_SIZE);
		trampolineCache = (u8*)AllocateExecutableMemory(TRAMPOLINE_SIZE);
		trampolineCodePtr = trampolineCache;
		numFlushes = 0;

		blocks = new JitBlock[MAX_NUM_BLOCKS];
		blockCodePointers = new u8*[MAX_NUM_BLOCKS];
		ClearCache();
		SetCodePtr(genFunctions);
		Asm::Generate();
		// Protect the generated functions
		WriteProtectMemory(genFunctions, 4096, true);
		SetCodePtr(codeCache);
	}

	bool IsInJitCode(u8 *codePtr) {
		return codePtr >= codeCache && codePtr <= GetCodePtr();
	}


	void EnterFastRun()
	{
		CompiledCode pExecAddr = (CompiledCode)Asm::enterCode;
		pExecAddr();
		//Will return when PowerPC::state changes
	}

	void ShutdownCache()
	{
		UnWriteProtectMemory(genFunctions, 4096, true);
		FreeMemoryPages(codeCache, CODE_SIZE);
		FreeMemoryPages(genFunctions, GEN_SIZE);
		FreeMemoryPages(trampolineCache, TRAMPOLINE_SIZE);
		delete [] blocks;
		delete [] blockCodePointers;
		blocks = 0;
		blockCodePointers = 0;
		numBlocks = 0;
	}

	void ResetCache()
	{
		ShutdownCache();
		InitCache();
	}

	int GetBlockNumberFromAddress(u32 addr)
	{
		if (!blocks)
			return -1;
		u32 code = Memory::ReadFast32(addr);
		if ((code>>26) == JIT_OPCODE)
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

	//Block linker
	//Make sure to have as many blocks as possible compiled before calling this
	//It's O(N), so it's fast :)
	//Can be faster by doing a queue for blocks to link up, and only process those
	//Should probably be done

	void LinkBlockExits(int i)
	{
		JitBlock &b = blocks[i];
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

	void LinkBlock(int i)
	{
		LinkBlockExits(i);
		JitBlock &b = blocks[i];
		std::map<u32, int>::iterator iter;
		iter = unlinked.find(b.originalAddress);
		if (iter != unlinked.end())
		{
			LinkBlockExits(iter->second);
			// todo - remove stuff from unlinked
		}
	}

	void LinkBlocks()
	{
		u8 *oldCodePtr = GetWritableCodePtr();
		//for (int i = 0; i < numBlocks; i++)
	//		LinkBlockExits(i);
		
		for (std::map<u32, int>::iterator iter = unlinked.begin();
			iter != unlinked.end(); iter++)
		{
			LinkBlockExits(iter->second);
		}
		// for (int i = 0; i < 2000; i++)
		LinkBlock(numBlocks);		
		SetCodePtr(oldCodePtr);
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
		u8 *prev_code = GetWritableCodePtr();
		// Spurious entrances from previously linked blocks can only come through checkedEntry
		SetCodePtr((u8*)b.checkedEntry);
		MOV(32, M(&PC), Imm32(b.originalAddress));
		JMP(Asm::dispatcher, true);
		SetCodePtr(blockCodePointers[blocknum]);
		MOV(32, M(&PC), Imm32(b.originalAddress));
		JMP(Asm::dispatcher, true);
		SetCodePtr(prev_code);
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
				               address, address+length))
			{
				DestroyBlock(i, true);
			}
		}
	}

	void ClearCache()
	{

		// Is destroying the blocks really necessary?
		for (int i = 0; i < numBlocks; i++)
		{
			DestroyBlock(i, false);
		}
		unlinked.clear();
		trampolineCodePtr = trampolineCache;
		numBlocks = 0;
		numFlushes++;
		memset(blockCodePointers, 0, sizeof(u8*)*MAX_NUM_BLOCKS);
		memset(codeCache, 0xCC, CODE_SIZE);
		SetCodePtr(codeCache);
	}

	extern u8 *trampolineCodePtr;
	void BackPatch(u8 *codePtr, int accessType)
	{
		if (codePtr < codeCache || codePtr > codeCache + CODE_SIZE)
			return;  // this will become a regular crash real soon after this

		static int counter = 0;
		++counter;
		if (counter == 30)
		{
			counter ++;
			counter --;
		}
		
		// TODO: also mark and remember the instruction address as known HW memory access, for use in later compiles.
		// But to do that we need to be able to reconstruct what instruction wrote this code, and we can't do that yet.
		u8 *oldCodePtr = GetWritableCodePtr();
		InstructionInfo info;
		if (!DisassembleMov(codePtr, info, accessType))
			PanicAlert("BackPatch - failed to disassemble MOV instruction");
		
		X64Reg addrReg = (X64Reg)info.scaledReg;
		X64Reg dataReg = (X64Reg)info.regOperandReg;
		if (info.otherReg != RBX)
			PanicAlert("BackPatch : Base reg not RBX.");
		if (accessType == OP_ACCESS_WRITE)
			PanicAlert("BackPatch : Currently only supporting reads.");
		//if (info.instructionSize < 5) 
		//	PanicAlert("Instruction at %08x Too Small : %i", (u32)codePtr, info.instructionSize);
		// OK, let's write a trampoline, and a jump to it.
		// Later, let's share trampolines.

		// In the first iteration, we assume that all accesses are 32-bit. We also only deal with reads.
		u8 *trampoline = trampolineCodePtr;
		SetCodePtr(trampolineCodePtr);
		// * Save all volatile regs
		PUSH(RCX);
		PUSH(RDX);
		PUSH(RSI); 
		PUSH(RDI); 
		PUSH(R8);
		PUSH(R9);
		PUSH(R10);
		PUSH(R11);
		//TODO: Also preserve XMM0-3?
		SUB(64, R(RSP), Imm8(0x20));
		// * Set up stack frame.
		// * Call ReadMemory32
		//LEA(32, ECX, MDisp((X64Reg)addrReg, info.displacement));
		MOV(32, R(ECX), R((X64Reg)addrReg));
		if (info.displacement) {
			ADD(32, R(ECX), Imm32(info.displacement));
		}
		switch (info.operandSize) {
		//case 1: 
		//	CALL((void *)&Memory::Read_U8);
		//	break;
		case 4: 
			CALL((void *)&Memory::Read_U32);
			break;
		default:
			PanicAlert("We don't handle this size %i yet in backpatch", info.operandSize);
			break;
		}
		// * Tear down stack frame.
		ADD(64, R(RSP), Imm8(0x20));
		POP(R11);
		POP(R10);
		POP(R9);
		POP(R8);
		POP(RDI); 
		POP(RSI); 
		POP(RDX);
		POP(RCX);
		MOV(32, R(dataReg), R(EAX));
		RET();
		trampolineCodePtr = GetWritableCodePtr();

		SetCodePtr(codePtr);
		int bswapNopCount;
		// Check the following BSWAP for REX byte
		if ((GetCodePtr()[info.instructionSize] & 0xF0) == 0x40)
			bswapNopCount = 3;
		else
			bswapNopCount = 2;
		CALL(trampoline);
		NOP((int)info.instructionSize + bswapNopCount - 5);
		// There is also a BSWAP to kill.
		SetCodePtr(oldCodePtr);
	}

}

