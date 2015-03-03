// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Enable define below to enable oprofile integration. For this to work,
// it requires at least oprofile version 0.9.4, and changing the build
// system to link the Dolphin executable against libopagent.  Since the
// dependency is a little inconvenient and this is possibly a slight
// performance hit, it's not enabled by default, but it's useful for
// locating performance issues.

#include "disasm.h"

#include "Common/CommonTypes.h"
#include "Common/JitRegister.h"
#include "Common/MemoryUtil.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace Gen;

	bool JitBaseBlockCache::IsFull() const
	{
		return GetNumBlocks() >= MAX_NUM_BLOCKS - 1;
	}

	void JitBaseBlockCache::Init()
	{
		JitRegister::Init();

		iCache.fill(0);
		Clear();
	}

	void JitBaseBlockCache::Shutdown()
	{
		num_blocks = 0;

		JitRegister::Shutdown();
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
		jit->js.fifoWriteAddresses.clear();
		jit->js.pairedQuantizeAddresses.clear();
		for (int i = 0; i < num_blocks; i++)
		{
			DestroyBlock(i, false);
		}
		links_to.clear();
		block_map.clear();

		valid_block.ClearAll();

		num_blocks = 0;
		blocks[0].msrBits = 0xFFFFFFFF;
		blocks[0].invalid = true;
	}

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

	int JitBaseBlockCache::AllocateBlock(u32 em_address)
	{
		JitBlock &b = blocks[num_blocks];
		b.invalid = false;
		b.effectiveAddress = em_address;
		b.physicalAddress = PowerPC::JitCache_TranslateAddress(em_address).address;
		b.msrBits = MSR & 0x30;
		b.linkData.clear();
		num_blocks++; //commit the current block
		return num_blocks - 1;
	}

	void JitBaseBlockCache::FinalizeBlock(int block_num, bool block_link, const u8 *code_ptr)
	{
		JitBlock &b = blocks[block_num];
		if (start_block_map.count(b.physicalAddress))
		{
			// We already have a block at this address; invalidate the old block.
			// This should be very rare.
			WARN_LOG(DYNA_REC, "Invalidating compiled block at same address %08x", b.physicalAddress);
			int old_block_num = start_block_map[b.physicalAddress];
			JitBlock &old_b = blocks[old_block_num];
			block_map.erase(std::make_pair(old_b.physicalAddress + 4 * old_b.originalSize - 1, old_b.physicalAddress));
			DestroyBlock(old_block_num, true);
		}
		start_block_map[b.physicalAddress] = block_num;
		iCache[(b.effectiveAddress >> 2) & iCache_Mask] = block_num;

		u32 pAddr = b.physicalAddress;

		for (u32 block = pAddr / 32; block <= (pAddr + (b.originalSize - 1) * 4) / 32; ++block)
			valid_block.Set(block);

		block_map[std::make_pair(pAddr + 4 * b.originalSize - 1, pAddr)] = block_num;

		if (block_link)
		{
			for (const auto& e : b.linkData)
			{
				links_to.insert(std::pair<u32, int>(e.exitAddress, block_num));
			}

			LinkBlock(block_num);
			LinkBlockExits(block_num);
		}

		JitRegister::Register(b.normalEntry, b.codeSize,
			"JIT_PPC_%08x", b.physicalAddress);
	}

	int JitBaseBlockCache::GetBlockNumberFromStartAddress(u32 addr)
	{
		u32 translated_addr = addr;
		if (UReg_MSR(MSR).IR)
		{
			auto translated = PowerPC::JitCache_TranslateAddress(addr);
			if (!translated.valid)
			{
				return -1;
			}
			translated_addr = translated.address;
		}

		auto map_result = start_block_map.find(translated_addr);
		if (map_result == start_block_map.end())
			return -1;
		int block_num = map_result->second;
		if (blocks[block_num].invalid)
			return -1;
		if (blocks[block_num].effectiveAddress != addr)
			return -1;
		return block_num;
	}

	void JitBaseBlockCache::MoveBlockIntoFastCache(u32 addr)
	{
		int block_num = GetBlockNumberFromStartAddress(addr);
		if (block_num < 0 || blocks[block_num].msrBits != (MSR & 0x30))
		{
			Jit(addr);
		}
		else
		{
			iCache[(addr >> 2) & iCache_Mask] = block_num;
			JitBlock &b = blocks[block_num];
			WriteUndestroyBlock(b.checkedEntry, b.effectiveAddress);
		}
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
		for (auto& e : b.linkData)
		{
			if (!e.linkStatus)
			{
				int destinationBlock = GetBlockNumberFromStartAddress(e.exitAddress);
				if (destinationBlock != -1 && blocks[destinationBlock].msrBits == b.msrBits)
				{
					WriteLinkBlock(e.exitPtrs, blocks[destinationBlock].checkedEntry);
					e.linkStatus = true;
				}
			}
		}
	}

	void JitBaseBlockCache::LinkBlock(int i)
	{
		LinkBlockExits(i);
		JitBlock &b = blocks[i];
		auto ppp = links_to.equal_range(b.effectiveAddress);

		for (auto iter = ppp.first; iter != ppp.second; ++iter)
		{
			LinkBlockExits(iter->second);
		}
	}

	void JitBaseBlockCache::UnlinkBlock(int i)
	{
		JitBlock &b = blocks[i];
		auto ppp = links_to.equal_range(b.effectiveAddress);

		for (auto iter = ppp.first; iter != ppp.second; ++iter)
		{
			JitBlock &sourceBlock = blocks[iter->second];
			for (auto& e : sourceBlock.linkData)
			{
				if (e.exitAddress == b.effectiveAddress)
					e.linkStatus = false;
			}
		}
		links_to.erase(b.effectiveAddress);
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
		start_block_map.erase(b.physicalAddress);
		iCache[(b.effectiveAddress >> 2) & iCache_Mask] = 0;

		UnlinkBlock(block_num);

		// Send anyone who tries to run this block back to the dispatcher.
		// Not entirely ideal, but .. pretty good.
		// Spurious entrances from previously linked blocks can only come through checkedEntry
		WriteDestroyBlock(b.checkedEntry, b.effectiveAddress);
	}

	void JitBaseBlockCache::InvalidateICache(u32 address, const u32 length, bool forced)
	{
		auto translated = PowerPC::JitCache_TranslateAddress(address);
		if (!translated.valid)
			return;
		u32 pAddr = translated.address;

		// Optimize the common case of length == 32 which is used by Interpreter::dcb*
		bool destroy_block = true;
		if (length == 32)
		{
			if (!valid_block.Test(pAddr / 32))
				destroy_block = false;
			else
				valid_block.Clear(pAddr / 32);
		}

		// destroy JIT blocks
		// !! this works correctly under assumption that any two overlapping blocks end at the same address
		if (destroy_block)
		{
			std::map<std::pair<u32,u32>, u32>::iterator it1 = block_map.lower_bound(std::make_pair(pAddr, 0)), it2 = it1;
			while (it2 != block_map.end() && it2->first.second < pAddr + length)
			{
				DestroyBlock(it2->second, true);
				++it2;
			}
			if (it1 != it2)
			{
				block_map.erase(it1, it2);
			}

			// If the code was actually modified, we need to clear the relevant entries from the
			// FIFO write address cache, so we don't end up with FIFO checks in places they shouldn't
			// be (this can clobber flags, and thus break any optimization that relies on flags
			// being in the right place between instructions).
			if (!forced)
			{
				for (u32 i = address; i < address + length; i += 4)
				{
					jit->js.fifoWriteAddresses.erase(i);
					jit->js.pairedQuantizeAddresses.erase(i);
				}
			}
		}
	}

	void JitBaseBlockCache::EvictTLBEntry(u32 address)
	{
		auto iter = start_block_map.lower_bound(address);
		while (iter != start_block_map.end() && (iter->first & ~0xFFF) == address)
		{
			// Remove the block from the fast lookup map, so we re-verify the address
			// before using it again.
			iCache[(iter->first >> 2) & iCache_Mask] = 0;

			// Unlink the block
			JitBlock &b = blocks[iter->second];
			WriteDestroyBlock(b.checkedEntry, b.effectiveAddress);

			++iter;
		}
	}


	void JitBlockCache::WriteLinkBlock(u8* location, const u8* address)
	{
		XEmitter emit(location);
		if (*location == 0xE8)
			emit.CALL(address);
		else
			emit.JMP(address, true);
	}

	void JitBlockCache::WriteDestroyBlock(const u8* location, u32 address)
	{
		XEmitter emit((u8 *)location);
		emit.MOV(32, PPCSTATE(pc), Imm32(address));
		emit.JMP(jit->GetAsmRoutines()->dispatcher, true);
	}

	void JitBlockCache::WriteUndestroyBlock(const u8* location, u32 address)
	{
		XEmitter emit((u8 *)location);
		FixupBranch skip = emit.J_CC(CC_NBE);
		emit.MOV(32, PPCSTATE(pc), Imm32(address));
		emit.JMP(jit->GetAsmRoutines()->doTiming, true);  // downcount hit zero - go doTiming.
		emit.SetJumpTarget(skip);
	}
