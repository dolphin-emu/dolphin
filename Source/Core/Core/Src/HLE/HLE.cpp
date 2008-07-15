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

SPatch OSPatches[] = 
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

    // special
//	{ "GXPeekZ",					HLE_Misc::GXPeekZ},
//	{ "GXPeekARGB",					HLE_Misc::GXPeekARGB},  
};

SPatch OSBreakPoints[] =
{
	{ "FAKE_TO_SKIP_0",									HLE_Misc::UnimplementedFunction },
};

void PatchFunctions(const char* _gameID)
{	
	for (u32 i=0; i < sizeof(OSPatches) / sizeof(SPatch); i++)
	{
		int SymbolIndex = Debugger::FindSymbol(OSPatches[i].m_szPatchName);
		if (SymbolIndex > 0)
		{
            const Debugger::CSymbol& rSymbol = Debugger::GetSymbol(SymbolIndex);
			u32 HLEPatchValue = (1 & 0x3f) << 26;
			Memory::Write_U32(HLEPatchValue | i, rSymbol.vaddress);

            LOG(HLE,"Patching %s %08x", OSPatches[i].m_szPatchName, rSymbol.vaddress);
		}
	}

	for (size_t i=1; i < sizeof(OSBreakPoints) / sizeof(SPatch); i++)
	{
		int SymbolIndex = Debugger::FindSymbol(OSBreakPoints[i].m_szPatchName);
		if (SymbolIndex != -1)
		{
		    const Debugger::CSymbol& rSymbol = Debugger::GetSymbol(SymbolIndex);			
		    CBreakPoints::AddBreakPoint(rSymbol.vaddress, false);

            LOG(HLE,"Adding BP to %s %08x", OSBreakPoints[i].m_szPatchName, rSymbol.vaddress);
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
		_dbg_assert_(HLE, 0);
	}			

//	_dbg_assert_msg_(HLE,NPC == LR, "Broken HLE function (doesn't set NPC)", OSPatches[pos].m_szPatchName);
}

}
