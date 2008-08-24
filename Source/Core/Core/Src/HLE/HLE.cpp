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

#include "HLE.h"

#include "../PowerPC/PowerPC.h"
#include "../PowerPC/SymbolDB.h"
#include "../HW/Memmap.h"
#include "../Debugger/Debugger_SymbolMap.h"
#include "../Debugger/Debugger_BreakPoints.h"

#include "HLE_OS.h"
#include "HLE_Misc.h"

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
	int returnType;
};

static const SPatch OSPatches[] = 
{	
	{ "FAKE_TO_SKIP_0",		        HLE_Misc::UnimplementedFunction },

    // speedup
    { "OSProtectRange",	            HLE_Misc::UnimplementedFunctionFalse },
//	{ "THPPlayerGetState",			HLE_Misc:THPPlayerGetState },


    // debug out is very nice ;)
    { "OSReport",					HLE_OS::HLE_OSReport    },
    { "OSPanic",					HLE_OS::HLE_OSPanic     },
    { "vprintf",					HLE_OS::HLE_vprintf     },		
    { "printf",						HLE_OS::HLE_printf      },

	// wii only
	{ "SCCheckStatus",				HLE_Misc::UnimplementedFunctionFalse },	
	{ "__OSInitAudioSystem",        HLE_Misc::UnimplementedFunction },			
	
	// Super Monkey Ball
	{ ".evil_vec_cosine",           HLE_Misc::SMB_EvilVecCosine },
	{ ".evil_normalize",            HLE_Misc::SMB_EvilNormalize },
	{ ".evil_vec_setlength",        HLE_Misc::SMB_evil_vec_setlength },
	{ "PanicAlert",			        HLE_Misc::PanicAlert },
	{ ".sqrt_internal_needs_cr1",   HLE_Misc::SMB_sqrt_internal },
	{ ".rsqrt_internal_needs_cr1",  HLE_Misc::SMB_rsqrt_internal },
	{ ".atan2",						HLE_Misc::SMB_atan2},

    // special
//	{ "GXPeekZ",					HLE_Misc::GXPeekZ},
//	{ "GXPeekARGB",					HLE_Misc::GXPeekARGB},  
};

static const SPatch OSBreakPoints[] =
{
	{ "FAKE_TO_SKIP_0",									HLE_Misc::UnimplementedFunction },
};

void Patch(u32 address, const char *hle_func_name)
{
	for (u32 i = 0; i < sizeof(OSPatches) / sizeof(SPatch); i++)
	{
		if (!strcmp(OSPatches[i].m_szPatchName, hle_func_name)) {
			u32 HLEPatchValue = (1 & 0x3f) << 26;
			Memory::Write_U32(HLEPatchValue | i, address);
			return;
		}
	}
}

void PatchFunctions()
{	
	for (u32 i = 0; i < sizeof(OSPatches) / sizeof(SPatch); i++)
	{
		Symbol *symbol = g_symbolDB.GetSymbolFromName(OSPatches[i].m_szPatchName);
		if (symbol > 0)
		{
			u32 HLEPatchValue = (1 & 0x3f) << 26;
			for (size_t addr = symbol->address; addr < symbol->address + symbol->size; addr += 4)
				Memory::Write_U32(HLEPatchValue | i, addr);
            LOG(HLE,"Patching %s %08x", OSPatches[i].m_szPatchName, symbol->address);
		}
	}

	for (size_t i = 1; i < sizeof(OSBreakPoints) / sizeof(SPatch); i++)
	{
		Symbol *symbol = g_symbolDB.GetSymbolFromName(OSPatches[i].m_szPatchName);
		if (symbol > 0)
		{
		    CBreakPoints::AddBreakPoint(symbol->address, false);
            LOG(HLE,"Adding BP to %s %08x", OSBreakPoints[i].m_szPatchName, symbol->address);
		}
	}

//    CBreakPoints::AddBreakPoint(0x8000D3D0, false);
}

void Execute(u32 _CurrentPC, u32 _Instruction)
{
	int FunctionIndex = _Instruction & 0xFFFFF;
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

}
