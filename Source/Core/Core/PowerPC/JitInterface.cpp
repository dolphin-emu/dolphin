// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#if _M_X86
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_Tables.h"
#include "Core/PowerPC/Jit64IL/JitIL.h"
#include "Core/PowerPC/Jit64IL/JitIL_Tables.h"
#endif

#if _M_ARM_32
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_Tables.h"
#endif

#if _M_ARM_64
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_Tables.h"
#endif

static bool bFakeVMEM = false;
bool bMMU = false;

namespace JitInterface
{
	void DoState(PointerWrap &p)
	{
		if (jit && p.GetMode() == PointerWrap::MODE_READ)
			jit->GetBlockCache()->Clear();
	}
	CPUCoreBase *InitJitCore(int core)
	{
		bFakeVMEM = SConfig::GetInstance().m_LocalCoreStartupParameter.bTLBHack == true;
		bMMU = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;

		CPUCoreBase *ptr = nullptr;
		switch (core)
		{
			#if _M_X86
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
			#if _M_ARM_32
			case 3:
			{
				ptr = new JitArm();
				break;
			}
			#endif
			#if _M_ARM_64
			case 4:
			{
				ptr = new JitArm64();
				break;
			}
			#endif
			default:
			{
				PanicAlert("Unrecognizable cpu_core: %d", core);
				jit = nullptr;
				return nullptr;
			}
		}
		jit = static_cast<JitBase*>(ptr);
		jit->Init();
		return ptr;
	}
	void InitTables(int core)
	{
		switch (core)
		{
			#if _M_X86
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
			#if _M_ARM_32
			case 3:
			{
				JitArmTables::InitTables();
				break;
			}
			#endif
			#if _M_ARM_64
			case 4:
			{
				JitArm64Tables::InitTables();
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

	void WriteProfileResults(const std::string& filename)
	{
		// Can't really do this with no jit core available
		#if _M_X86

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
			PanicAlert("Failed to open %s", filename.c_str());
			return;
		}
		fprintf(f.GetHandle(), "origAddr\tblkName\tcost\ttimeCost\tpercent\ttimePercent\tOvAllinBlkTime(ms)\tblkCodeSize\n");
		for (auto& stat : stats)
		{
			const JitBlock *block = jit->GetBlockCache()->GetBlock(stat.blockNum);
			if (block)
			{
				std::string name = g_symbolDB.GetDescription(block->originalAddress);
				double percent = 100.0 * (double)stat.cost / (double)cost_sum;
	#ifdef _WIN32
				double timePercent = 100.0 * (double)block->ticCounter / (double)timecost_sum;
				fprintf(f.GetHandle(), "%08x\t%s\t%" PRIu64 "\t%" PRIu64 "\t%.2lf\t%llf\t%lf\t%i\n",
						block->originalAddress, name.c_str(), stat.cost,
						block->ticCounter, percent, timePercent,
						(double)block->ticCounter*1000.0/(double)countsPerSec, block->codeSize);
	#else
				fprintf(f.GetHandle(), "%08x\t%s\t%" PRIu64 "\t???\t%.2lf\t???\t???\t%i\n",
						block->originalAddress, name.c_str(), stat.cost,  percent, block->codeSize);
	#endif
			}
		}
		#endif
	}
	bool HandleFault(uintptr_t access_address, SContext* ctx)
	{
		return jit->HandleFault(access_address, ctx);
	}

	void ClearCache()
	{
		if (jit)
			jit->ClearCache();
	}
	void ClearSafe()
	{
		// This clear is "safe" in the sense that it's okay to run from
		// inside a JIT'ed block: it clears the instruction cache, but not
		// the JIT'ed code.
		// TODO: There's probably a better way to handle this situation.
		if (jit)
			jit->GetBlockCache()->Clear();
	}

	void InvalidateICache(u32 address, u32 size)
	{
		if (jit)
			jit->GetBlockCache()->InvalidateICache(address, size);
	}

	u32 ReadOpcodeJIT(u32 _Address)
	{
		if (bMMU && !bFakeVMEM && (_Address & Memory::ADDR_MASK_MEM1))
		{
			_Address = Memory::TranslateAddress(_Address, Memory::FLAG_OPCODE);
			if (_Address == 0)
			{
				return 0;
			}
		}

		u32 inst;
		// Bypass the icache for the external interrupt exception handler
		// -- this is stupid, should respect HID0
		if ( (_Address & 0x0FFFFF00) == 0x00000500 )
			inst = Memory::ReadUnchecked_U32(_Address);
		else
			inst = PowerPC::ppcState.iCache.ReadInstruction(_Address);
		return inst;
	}

	void CompileExceptionCheck(int type)
	{
		if (!jit)
			return;

		std::unordered_set<u32> *exception_addresses;

		switch (type)
		{
		case EXCEPTIONS_FIFO_WRITE:
		{
			exception_addresses = &jit->js.fifoWriteAddresses;
			break;
		}
		default:
			ERROR_LOG(POWERPC, "Unknown exception check type");
		}

		if (PC != 0 && (exception_addresses->find(PC)) == (exception_addresses->end()))
		{
			int optype = GetOpInfo(Memory::ReadUnchecked_U32(PC))->type;
			if (optype == OPTYPE_STORE || optype == OPTYPE_STOREFP || (optype == OPTYPE_STOREPS))
			{
				exception_addresses->insert(PC);

				// Invalidate the JIT block so that it gets recompiled with the external exception check included.
				jit->GetBlockCache()->InvalidateICache(PC, 4);
			}
		}
	}

	void Shutdown()
	{
		if (jit)
		{
			jit->Shutdown();
			delete jit;
			jit = nullptr;
		}
	}
}
