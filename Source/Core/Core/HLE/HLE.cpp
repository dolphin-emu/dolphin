// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/HLE_Misc.h"
#include "Core/HLE/HLE_OS.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_es.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCSymbolDB.h"


namespace HLE
{

using namespace PowerPC;

typedef void (*TPatchFunction)();

enum
{
	HLE_RETURNTYPE_BLR = 0,
	HLE_RETURNTYPE_RFI = 1,
};

struct SPatch
{
	char m_szPatchName[128];
	TPatchFunction PatchFunction;
	int type;
	int flags;
};

static const SPatch OSPatches[] =
{
	{ "FAKE_TO_SKIP_0",       HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	{ "PanicAlert",           HLE_Misc::HLEPanicAlert,         HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },

	// Name doesn't matter, installed in CBoot::BootUp()
	{ "HBReload",             HLE_Misc::HBReload,              HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// Debug/OS Support
	{ "OSPanic",              HLE_OS::HLE_OSPanic,             HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },

	{ "OSReport",             HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "DEBUGPrint",           HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "WUD_DEBUGPrint",       HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "vprintf",              HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "printf",               HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "puts",                 HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // gcc-optimized printf?
	{ "___blank(char *,...)", HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // used for early init things (normally)
	{ "___blank",             HLE_OS::HLE_GeneralDebugPrint,   HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "__write_console",      HLE_OS::HLE_write_console,       HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // used by sysmenu (+more?)
	{ "GeckoCodehandler",     HLE_Misc::HLEGeckoCodehandler,   HLE_HOOK_START,   HLE_TYPE_GENERIC },
};

static const SPatch OSBreakPoints[] =
{
	{ "FAKE_TO_SKIP_0", HLE_Misc::UnimplementedFunction },
};

void Patch(u32 addr, const char *hle_func_name)
{
	for (u32 i = 0; i < sizeof(OSPatches) / sizeof(SPatch); i++)
	{
		if (!strcmp(OSPatches[i].m_szPatchName, hle_func_name))
		{
			orig_instruction[addr] = i;
			return;
		}
	}
}

void PatchFunctions()
{
	orig_instruction.clear();
	for (u32 i = 0; i < sizeof(OSPatches) / sizeof(SPatch); i++)
	{
		Symbol *symbol = g_symbolDB.GetSymbolFromName(OSPatches[i].m_szPatchName);
		if (symbol)
		{
			for (u32 addr = symbol->address; addr < symbol->address + symbol->size; addr += 4)
			{
				orig_instruction[addr] = i;
			}
			INFO_LOG(OSHLE, "Patching %s %08x", OSPatches[i].m_szPatchName, symbol->address);
		}
	}

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
	{
		for (size_t i = 1; i < sizeof(OSBreakPoints) / sizeof(SPatch); i++)
		{
			Symbol *symbol = g_symbolDB.GetSymbolFromName(OSPatches[i].m_szPatchName);
			if (symbol)
			{
				PowerPC::breakpoints.Add(symbol->address, false);
				INFO_LOG(OSHLE, "Adding BP to %s %08x", OSBreakPoints[i].m_szPatchName, symbol->address);
			}
		}
	}

	// CBreakPoints::AddBreakPoint(0x8000D3D0, false);
}

void Execute(u32 _CurrentPC, u32 _Instruction)
{
	unsigned int FunctionIndex = _Instruction & 0xFFFFF;
	if ((FunctionIndex > 0) && (FunctionIndex < (sizeof(OSPatches) / sizeof(SPatch))))
	{
		OSPatches[FunctionIndex].PatchFunction();
	}
	else
	{
		PanicAlert("HLE system tried to call an undefined HLE function %i.", FunctionIndex);
	}

	// _dbg_assert_msg_(HLE,NPC == LR, "Broken HLE function (doesn't set NPC)", OSPatches[pos].m_szPatchName);
}

u32 GetFunctionIndex(u32 addr)
{
	std::map<u32, u32>::const_iterator iter = orig_instruction.find(addr);
	return (iter != orig_instruction.end()) ?  iter->second : 0;
}

int GetFunctionTypeByIndex(u32 index)
{
	return OSPatches[index].type;
}

int GetFunctionFlagsByIndex(u32 index)
{
	return OSPatches[index].flags;
}

bool IsEnabled(int flags)
{
	if (flags == HLE::HLE_TYPE_DEBUG && !SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging && PowerPC::GetMode() != MODE_INTERPRETER)
		return false;

	return true;
}

u32 UnPatch(const std::string& patchName)
{
	Symbol* symbol = g_symbolDB.GetSymbolFromName(patchName);

	if (symbol)
	{
		for (u32 addr = symbol->address; addr < symbol->address + symbol->size; addr += 4)
		{
			orig_instruction[addr] = 0;
			PowerPC::ppcState.iCache.Invalidate(addr);
		}
		return symbol->address;
	}

	return 0;
}

}  // end of namespace HLE
