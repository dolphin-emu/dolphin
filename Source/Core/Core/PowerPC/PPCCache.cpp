// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonFuncs.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCCache.h"
#include "Core/PowerPC/JitCommon/JitBase.h"
#include "Core/PowerPC/JitCommon/JitCache.h"

namespace PowerPC
{

	const u32 plru_mask[8] = {11, 11, 19, 19, 37, 37, 69, 69};
	const u32 plru_value[8] = {11, 3, 17, 1, 36, 4, 64, 0};

	InstructionCache::InstructionCache()
	{
		for (u32 m = 0; m < 0xff; m++)
		{
			u32 w = 0;
			while (m & (1 << w))
				w++;
			way_from_valid[m] = w;
		}

		for (u32 m = 0; m < 128; m++)
		{
			u32 b[7];
			for (int i = 0; i < 7; i++)
				b[i] = m & (1 << i);
			u32 w;
			if (b[0])
				if (b[2])
					if (b[6])
						w = 7;
					else
						w = 6;
				else
					if (b[5])
						w = 5;
					else
						w = 4;
			else
				if (b[1])
					if (b[4])
						w = 3;
					else
						w = 2;
				else
					if (b[3])
						w = 1;
					else
						w = 0;
			way_from_plru[m] = w;
		}
	}

	void InstructionCache::Reset()
	{
		memset(valid, 0, sizeof(valid));
		memset(plru, 0, sizeof(plru));
		memset(lookup_table, 0xff, sizeof(lookup_table));
		memset(lookup_table_ex, 0xff, sizeof(lookup_table_ex));
		memset(lookup_table_vmem, 0xff, sizeof(lookup_table_vmem));
		JitInterface::ClearSafe();
	}

	void InstructionCache::Init()
	{
		memset(data, 0, sizeof(data));
		memset(tags, 0, sizeof(tags));
		memset(way_from_valid, 0, sizeof(way_from_valid));
		memset(way_from_plru, 0, sizeof(way_from_plru));

		Reset();
	}

	void InstructionCache::Invalidate(u32 addr)
	{
		if (!HID0.ICE)
			return;
		// invalidates the whole set
		u32 set = (addr >> 5) & 0x7f;
		for (int i = 0; i < 8; i++)
			if (valid[set] & (1 << i))
			{
				if (tags[set][i] & (ICACHE_VMEM_BIT >> 12))
					lookup_table_vmem[((tags[set][i] << 7) | set) & 0xfffff] = 0xff;
				else if (tags[set][i] & (ICACHE_EXRAM_BIT >> 12))
					lookup_table_ex[((tags[set][i] << 7) | set) & 0x1fffff] = 0xff;
				else
					lookup_table[((tags[set][i] << 7) | set) & 0xfffff] = 0xff;
			}
		valid[set] = 0;
		JitInterface::InvalidateICache(addr & ~0x1f, 32, false);
	}

	u32 InstructionCache::ReadInstruction(u32 addr)
	{
		if (!HID0.ICE) // instruction cache is disabled
			return Memory::ReadUnchecked_U32(addr);
		u32 set = (addr >> 5) & 0x7f;
		u32 tag = addr >> 12;

		u32 t;
		if (addr & ICACHE_VMEM_BIT)
		{
			t = lookup_table_vmem[(addr >> 5) & 0xfffff];
		}
		else if (addr & ICACHE_EXRAM_BIT)
		{
			t = lookup_table_ex[(addr >> 5) & 0x1fffff];
		}
		else
		{
			t = lookup_table[(addr >> 5) & 0xfffff];
		}

		if (t == 0xff) // load to the cache
		{
			if (HID0.ILOCK) // instruction cache is locked
				return Memory::ReadUnchecked_U32(addr);
			// select a way
			if (valid[set] != 0xff)
				t = way_from_valid[valid[set]];
			else
				t = way_from_plru[plru[set]];
			// load
			Memory::CopyFromEmu((u8*)data[set][t], (addr & ~0x1f), 32);
			if (valid[set] & (1 << t))
			{
				if (tags[set][t] & (ICACHE_VMEM_BIT >> 12))
					lookup_table_vmem[((tags[set][t] << 7) | set) & 0xfffff] = 0xff;
				else if (tags[set][t] & (ICACHE_EXRAM_BIT >> 12))
					lookup_table_ex[((tags[set][t] << 7) | set) & 0x1fffff] = 0xff;
				else
					lookup_table[((tags[set][t] << 7) | set) & 0xfffff] = 0xff;
			}

			if (addr & ICACHE_VMEM_BIT)
				lookup_table_vmem[(addr >> 5) & 0xfffff] = t;
			else if (addr & ICACHE_EXRAM_BIT)
				lookup_table_ex[(addr >> 5) & 0x1fffff] = t;
			else
				lookup_table[(addr>>5) & 0xfffff] = t;
			tags[set][t] = tag;
			valid[set] |= (1 << t);
		}
		// update plru
		plru[set] = (plru[set] & ~plru_mask[t]) | plru_value[t];
		u32 res = Common::swap32(data[set][t][(addr >> 2) & 7]);
		return res;
	}

}
