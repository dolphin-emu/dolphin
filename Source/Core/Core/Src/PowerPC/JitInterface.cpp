// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#include "JitInterface.h"
#include "JitCommon/JitBase.h"

#ifndef _M_GENERIC
#include "Jit64IL/JitIL.h"
#include "Jit64/Jit.h"
#include "Jit64/Jit64_Tables.h"
#include "Jit64IL/JitIL_Tables.h"
#endif

#ifdef _M_ARM
#include "JitArm32/Jit.h"
#include "JitArm32/JitArm_Tables.h"
#endif

#include "Profiler.h"
#include "PPCSymbolDB.h"
#include "HW/Memmap.h"
#include "ConfigManager.h"

bool bFakeVMEM = false;
bool bMMU = false;

namespace JitInterface
{
	void DoState(PointerWrap &p)
	{
		if (jit && p.GetMode() == PointerWrap::MODE_READ)
			jit->GetBlockCache()->ClearSafe();
	}
	CPUCoreBase *InitJitCore(int core)
	{
		bFakeVMEM = SConfig::GetInstance().m_LocalCoreStartupParameter.iTLBHack == 1;
		bMMU = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;
	
		CPUCoreBase *ptr = NULL;
		switch(core)
		{
			#ifndef _M_GENERIC
			case 1:
			{
				ptr = new Jit64();
				break;
			}
			case 2:
			{
				ptr = new JitIL();
				break;
			}
			#endif
			#ifdef _M_ARM
			case 3:
			{
				ptr = new JitArm();
				break;
			}
			#endif
			default:
			{
				PanicAlert("Unrecognizable cpu_core: %d", core);
				jit = NULL;
				return NULL;
				break;
			}
		}
		jit = static_cast<JitBase*>(ptr);
		jit->Init();
		return ptr;
	}
	void InitTables(int core)
	{
		switch(core)
		{
			#ifndef _M_GENERIC
			case 1:
			{
				Jit64Tables::InitTables();
				break;
			}
			case 2:
			{
				JitILTables::InitTables();
				break;
			}
			#endif
			#ifdef _M_ARM
			case 3:
			{
				JitArmTables::InitTables();
				break;
			}
			#endif
			default:
			{
				PanicAlert("Unrecognizable cpu_core: %d", core);
				break;
			}
		}
	}
	CPUCoreBase *GetCore()
	{
		return jit;
	}

	void WriteProfileResults(const char *filename)
	{
		// Can't really do this with no jit core available
		#ifndef _M_GENERIC
		
		std::vector<BlockStat> stats;
		stats.reserve(jit->GetBlockCache()->GetNumBlocks());
		u64 cost_sum = 0;
	#ifdef _WIN32
		u64 timecost_sum = 0;
		u64 countsPerSec;
		QueryPerformanceFrequency((LARGE_INTEGER *)&countsPerSec);
	#endif
		for (int i = 0; i < jit->GetBlockCache()->GetNumBlocks(); i++)
		{
			const JitBlock *block = jit->GetBlockCache()->GetBlock(i);
			// Rough heuristic.  Mem instructions should cost more.
			u64 cost = block->originalSize * (block->runCount / 4);
	#ifdef _WIN32
			u64 timecost = block->ticCounter;
	#endif
			// Todo: tweak.
			if (block->runCount >= 1)
				stats.push_back(BlockStat(i, cost));
			cost_sum += cost;
	#ifdef _WIN32
			timecost_sum += timecost;
	#endif
		}

		sort(stats.begin(), stats.end());
		File::IOFile f(filename, "w");
		if (!f)
		{
			PanicAlert("Failed to open %s", filename);
			return;
		}
		fprintf(f.GetHandle(), "origAddr\tblkName\tcost\ttimeCost\tpercent\ttimePercent\tOvAllinBlkTime(ms)\tblkCodeSize\n");
		for (unsigned int i = 0; i < stats.size(); i++)
		{
			const JitBlock *block = jit->GetBlockCache()->GetBlock(stats[i].blockNum);
			if (block)
			{
				std::string name = g_symbolDB.GetDescription(block->originalAddress);
				double percent = 100.0 * (double)stats[i].cost / (double)cost_sum;
	#ifdef _WIN32 
				double timePercent = 100.0 * (double)block->ticCounter / (double)timecost_sum;
				fprintf(f.GetHandle(), "%08x\t%s\t%llu\t%llu\t%.2lf\t%llf\t%lf\t%i\n", 
						block->originalAddress, name.c_str(), stats[i].cost,
						block->ticCounter, percent, timePercent,
						(double)block->ticCounter*1000.0/(double)countsPerSec, block->codeSize);
	#else
				fprintf(f.GetHandle(), "%08x\t%s\t%llu\t???\t%.2lf\t???\t???\t%i\n", 
						block->originalAddress, name.c_str(), stats[i].cost,  percent, block->codeSize);
	#endif
			}
		}
		#endif
	}
	bool IsInCodeSpace(u8 *ptr)
	{
		return jit->IsInCodeSpace(ptr);
	}
	const u8 *BackPatch(u8 *codePtr, int accessType, u32 em_address, void *ctx)
	{
		return jit->BackPatch(codePtr, accessType, em_address, ctx);
	}

	void ClearCache()
	{
		if (jit)
			jit->ClearCache();
	}
	void ClearSafe()
	{
		if (jit)
			jit->GetBlockCache()->ClearSafe();
	}

	void InvalidateICache(u32 address, u32 size)
	{
		if (jit)
			jit->GetBlockCache()->InvalidateICache(address, size);
	}


	// Memory functions
	u32 Read_Opcode_JIT_Uncached(const u32 _Address)
	{
		u8* iCache;
		u32 addr;
		if (_Address & JIT_ICACHE_VMEM_BIT)
		{
			iCache = jit->GetBlockCache()->GetICacheVMEM();
			addr = _Address & JIT_ICACHE_MASK;
		}
		else if (_Address & JIT_ICACHE_EXRAM_BIT)
		{
			iCache = jit->GetBlockCache()->GetICacheEx();
			addr = _Address & JIT_ICACHEEX_MASK;
		}
		else
		{
			iCache = jit->GetBlockCache()->GetICache();
			addr = _Address & JIT_ICACHE_MASK;
		}
		u32 inst = *(u32*)(iCache + addr);
		if (inst == JIT_ICACHE_INVALID_WORD)
		{
			u32 cache_block_start = addr & ~0x1f;
			u32 mem_block_start = _Address & ~0x1f;
			u8 *pMem = Memory::GetPointer(mem_block_start);
			memcpy(iCache + cache_block_start, pMem, 32);
			inst = *(u32*)(iCache + addr);
		}
		inst = Common::swap32(inst);

		if ((inst & 0xfc000000) == 0)
		{
			inst = jit->GetBlockCache()->GetOriginalFirstOp(inst);
		}

		return inst;
	}

	u32 Read_Opcode_JIT(u32 _Address)
	{
	#ifdef FAST_ICACHE	
		if (bMMU && !bFakeVMEM && (_Address & Memory::ADDR_MASK_MEM1))
		{
			_Address = Memory::TranslateAddress(_Address, Memory::FLAG_OPCODE);
			if (_Address == 0)
			{
				return 0;
			}
		}
		u32 inst = 0;

		// Bypass the icache for the external interrupt exception handler
		if ( (_Address & 0x0FFFFF00) == 0x00000500 )
			inst = Read_Opcode_JIT_Uncached(_Address);
		else
			inst = PowerPC::ppcState.iCache.ReadInstruction(_Address);
	#else
		u32 inst = Memory::ReadUnchecked_U32(_Address);
	#endif
		return inst;
	}

	// The following function is deprecated in favour of FAST_ICACHE
	u32 Read_Opcode_JIT_LC(const u32 _Address)
	{
	#ifdef JIT_UNLIMITED_ICACHE	
		if ((_Address & ~JIT_ICACHE_MASK) != 0x80000000 && (_Address & ~JIT_ICACHE_MASK) != 0x00000000 &&
			(_Address & ~JIT_ICACHE_MASK) != 0x7e000000 && // TLB area
			(_Address & ~JIT_ICACHEEX_MASK) != 0x90000000 && (_Address & ~JIT_ICACHEEX_MASK) != 0x10000000)
		{
			PanicAlertT("iCacheJIT: Reading Opcode from %x. Please report.", _Address);
			ERROR_LOG(MEMMAP, "iCacheJIT: Reading Opcode from %x. Please report.", _Address);
			return 0;
		}
		u8* iCache;
		u32 addr;
		if (_Address & JIT_ICACHE_VMEM_BIT)
		{		
			iCache = jit->GetBlockCache()->GetICacheVMEM();
			addr = _Address & JIT_ICACHE_MASK;
		}
		else if (_Address & JIT_ICACHE_EXRAM_BIT)
		{		
			iCache = jit->GetBlockCache()->GetICacheEx();
			addr = _Address & JIT_ICACHEEX_MASK;
		}
		else
		{
			iCache = jit->GetBlockCache()->GetICache();
			addr = _Address & JIT_ICACHE_MASK;
		}
		u32 inst = *(u32*)(iCache + addr);
		if (inst == JIT_ICACHE_INVALID_WORD)
			inst = Memory::ReadUnchecked_U32(_Address);
		else
			inst = Common::swap32(inst);
	#else
		u32 inst = Memory::ReadUnchecked_U32(_Address);
	#endif
		if ((inst & 0xfc000000) == 0)
		{
			inst = jit->GetBlockCache()->GetOriginalFirstOp(inst);
		}	
		return inst;
	}

	// WARNING! No checks!
	// We assume that _Address is cached
	void Write_Opcode_JIT(const u32 _Address, const u32 _Value)
	{
	#ifdef JIT_UNLIMITED_ICACHE
		if (_Address & JIT_ICACHE_VMEM_BIT)
		{
			*(u32*)(jit->GetBlockCache()->GetICacheVMEM() + (_Address & JIT_ICACHE_MASK)) = Common::swap32(_Value);		
		}
		else if (_Address & JIT_ICACHE_EXRAM_BIT)
		{
			*(u32*)(jit->GetBlockCache()->GetICacheEx() + (_Address & JIT_ICACHEEX_MASK)) = Common::swap32(_Value);		
		}
		else
			*(u32*)(jit->GetBlockCache()->GetICache() + (_Address & JIT_ICACHE_MASK)) = Common::swap32(_Value);
	#else
		Memory::WriteUnchecked_U32(_Value, _Address);
	#endif	
	}

	
	void Shutdown()
	{
		if (jit)
		{
			jit->Shutdown();
			delete jit;
			jit = NULL;
		}
	}
}
