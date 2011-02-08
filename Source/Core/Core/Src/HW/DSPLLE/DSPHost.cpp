// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"
#include "Hash.h"
#include "DSP/DSPHost.h"
#include "DSPSymbols.h"
#include "DSPLLETools.h"
#include "../DSP.h"
#include "../../ConfigManager.h"
#include "../../PowerPC/PowerPC.h"
#include "Host.h"

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

u8 DSPHost_ReadHostMemory(u32 addr)
{
	return DSP::ReadARAM(addr);
}

void DSPHost_WriteHostMemory(u8 value, u32 addr)
{
	DSP::WriteARAM(value, addr);
}

bool DSPHost_OnThread()
{
	const SCoreStartupParameter& _CoreParameter = SConfig::GetInstance().m_LocalCoreStartupParameter;
	return  _CoreParameter.bDSPThread;
}

void DSPHost_InterruptRequest()
{
	// Fire an interrupt on the PPC ASAP.
	DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
}

u32 DSPHost_CodeLoaded(const u8 *ptr, int size)
{
	u32 ector_crc = HashEctor(ptr, size);

#if defined(_DEBUG) || defined(DEBUGFAST)
	DumpDSPCode(ptr, size, ector_crc);
#endif

	DSPSymbols::Clear();

	// Auto load text file - if none just disassemble.
	
	NOTICE_LOG(DSPLLE, "ector_crc: %08x", ector_crc);

	DSPSymbols::Clear();
	bool success = false;
	switch (ector_crc)
	{
		case 0x86840740: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_Zelda.txt"); break;
		case 0x42f64ac4: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_Luigi.txt"); break;
		case 0x07f88145: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AX_07F88145.txt"); break;
		case 0x3ad3b7ac: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AX_3AD3B7AC.txt"); break;
		case 0x3daf59b9: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AX_3DAF59B9.txt"); break;
		case 0x4e8a8b21: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AX_4E8A8B21.txt"); break;
		case 0xe2136399: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AX_E2136399.txt"); break;
		case 0xdd7e72d5: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_GBA.txt"); break;
		case 0x347112BA: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_AXWii.txt"); break;
		case 0xD643001F: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_SuperMarioGalaxy.txt"); break;
		default: success = false; break;
	}

	if (!success) {
		DSPSymbols::AutoDisassembly(0x0, 0x1000);
	}

	// Always add the ROM.
	DSPSymbols::AutoDisassembly(0x8000, 0x9000);

	DSPHost_UpdateDebugger();

	return ector_crc;
}

void DSPHost_UpdateDebugger()
{
	Host_RefreshDSPDebuggerWindow();
}
