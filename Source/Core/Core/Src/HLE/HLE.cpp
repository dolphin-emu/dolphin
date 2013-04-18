// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"

#include "HLE.h"

#include "../PowerPC/PowerPC.h"
#include "../PowerPC/PPCSymbolDB.h"
#include "../HW/Memmap.h"
#include "../Debugger/Debugger_SymbolMap.h"

#include "HLE_OS.h"
#include "HLE_Misc.h"
#include "IPC_HLE/WII_IPC_HLE_Device_es.h"
#include "ConfigManager.h"
#include "Core.h"

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
	{ "FAKE_TO_SKIP_0",		HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// speedup
	//{ "OSProtectRange", HLE_Misc::UnimplementedFunctionFalse, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ "THPPlayerGetState", HLE_Misc:THPPlayerGetState, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ "memcpy",				HLE_Misc::gc_memcpy, HLE_HOOK_REPLACE, HLE_TYPE_MEMORY },
	//{ "memcmp",				HLE_Misc::gc_memcmp, HLE_HOOK_REPLACE, HLE_TYPE_MEMORY },
	//{ "memset",				HLE_Misc::gc_memset, HLE_HOOK_REPLACE, HLE_TYPE_MEMORY },
	//{ "memmove",			HLE_Misc::gc_memmove, HLE_HOOK_REPLACE, HLE_TYPE_MEMORY },

	//{ "__div2i",			HLE_Misc::div2i, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC }, // Slower?
	//{ "__div2u",			HLE_Misc::div2u, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC }, // Slower?

	//{ "DCFlushRange",		HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ "DCInvalidateRange",	HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ "DCZeroRange",		HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// debug out is very nice ;)
	{ "OSReport",			HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "DEBUGPrint",			HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "WUD_DEBUGPrint",		HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "OSPanic",			HLE_OS::HLE_OSPanic, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "vprintf",			HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "printf",				HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "puts",				HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // gcc-optimized printf?
	{ "___blank(char *,...)", HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // used for early init things (normally)
	{ "___blank",			HLE_OS::HLE_GeneralDebugPrint, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	{ "__write_console",	HLE_OS::HLE_write_console, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG }, // used by sysmenu (+more?)

	// wii only
	//{ "__OSInitAudioSystem", HLE_Misc::UnimplementedFunction, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// Super Monkey Ball - no longer needed.
	//{ ".evil_vec_cosine", HLE_Misc::SMB_EvilVecCosine, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".evil_normalize", HLE_Misc::SMB_EvilNormalize, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".evil_vec_setlength", HLE_Misc::SMB_evil_vec_setlength, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".evil_vec_something", HLE_Misc::FZero_evil_vec_normalize, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	{ "PanicAlert",			HLE_Misc::HLEPanicAlert, HLE_HOOK_REPLACE, HLE_TYPE_DEBUG },
	//{ ".sqrt_internal_needs_cr1", HLE_Misc::SMB_sqrt_internal, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".rsqrt_internal_needs_cr1", HLE_Misc::SMB_rsqrt_internal, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".atan2", HLE_Misc::SMB_atan2HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".sqrt_fz", HLE_Misc::FZ_sqrtHLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// F-zero still isn't working correctly, but these aren't really helping.

	//{ ".sqrt_internal_fz", HLE_Misc::FZ_sqrt_internal, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	//{ ".rsqrt_internal_fz", HLE_Misc::FZ_rsqrt_internal, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	//{ ".kill_infinites", HLE_Misc::FZero_kill_infinites, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	// special
	// { "GXPeekZ", HLE_Misc::GXPeekZHLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	// { "GXPeekARGB", HLE_Misc::GXPeekARGBHLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// Name doesn't matter, installed in CBoot::BootUp()
	{ "HBReload",			HLE_Misc::HBReload, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

	// ES_LAUNCH
	{ "__OSBootDol",		HLE_Misc::OSBootDol, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },
	{ "OSGetResetCode",		HLE_Misc::OSGetResetCode, HLE_HOOK_REPLACE, HLE_TYPE_GENERIC },

};

static const SPatch OSBreakPoints[] =
{
	{ "FAKE_TO_SKIP_0",									HLE_Misc::UnimplementedFunction },
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
		if (symbol > 0)
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
			if (symbol > 0)
			{
				PowerPC::breakpoints.Add(symbol->address, false);
				INFO_LOG(OSHLE, "Adding BP to %s %08x", OSBreakPoints[i].m_szPatchName, symbol->address);
			}
		}
	}

	//    CBreakPoints::AddBreakPoint(0x8000D3D0, false);
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

	//	_dbg_assert_msg_(HLE,NPC == LR, "Broken HLE function (doesn't set NPC)", OSPatches[pos].m_szPatchName);
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
	if (flags == HLE::HLE_TYPE_MEMORY && Core::g_CoreStartupParameter.bMMU)
		return false;

	if (flags == HLE::HLE_TYPE_DEBUG && !Core::g_CoreStartupParameter.bEnableDebugging && PowerPC::GetMode() != MODE_INTERPRETER)
		return false;

	return true;
}

u32 UnPatch(std::string patchName)
{
	Symbol *symbol = g_symbolDB.GetSymbolFromName(patchName.c_str());
	if (symbol > 0)
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
