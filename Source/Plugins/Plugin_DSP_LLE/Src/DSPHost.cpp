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
#include "DSPHost.h"
#include "DSPSymbols.h"
#include "Tools.h"
#include "pluginspecs_dsp.h"

extern DSPInitialize g_dspInitialize;

#if defined(HAVE_WX) && HAVE_WX

#include "DSPConfigDlgLLE.h"
#include "Debugger/DSPDebugWindow.h" // For the DSPDebuggerLLE class
extern DSPDebuggerLLE* m_DebuggerFrame;

#endif

// The user of the DSPCore library must supply a few functions so that the
// emulation core can access the environment it runs in. If the emulation
// core isn't used, for example in an asm/disasm tool, then most of these
// can be stubbed out.

u8 DSPHost_ReadHostMemory(u32 addr)
{
	return g_dspInitialize.pARAM_Read_U8(addr);
}

void DSPHost_WriteHostMemory(u8 value, u32 addr)
{
	g_dspInitialize.pARAM_Write_U8(value, addr);
}

bool DSPHost_OnThread()
{
	return g_dspInitialize.bOnThread;
}

bool DSPHost_Running()
{
	return !(*g_dspInitialize.pEmulatorState);
}

void DSPHost_InterruptRequest()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	NOTICE_LOG(DSPLLE, "Firing an interrupt on the PPC ASAP");
#endif
	// Fire an interrupt on the PPC ASAP.
	g_dspInitialize.pGenerateDSPInterrupt();
}

u32 DSPHost_CodeLoaded(const u8 *ptr, int size)
{
	u32 crc = GenerateCRC(ptr, size);
	DumpDSPCode(ptr, size, crc);

	// HLE plugin uses this crc method
	u32 ector_crc = HashEctor(ptr, size);

	DSPSymbols::Clear();

	// Auto load text file - if none just disassemble.
	
	// TODO: Don't hardcode for Zelda.
	NOTICE_LOG(DSPLLE, "ector_crc: %08x", ector_crc);

	DSPSymbols::Clear();
	bool success = false;
	switch (ector_crc)
	{
		case 0x86840740: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_Zelda.txt"); break;
		case 0x42f64ac4: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_Luigi.txt"); break;
		case 0x07f88145: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_4CB8233B.txt"); break;
		case 0x3ad3b7ac: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_8A7A05E2.txt"); break;
		case 0x3daf59b9: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_D9D066EA.txt"); break;
		case 0x4e8a8b21: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_6A696CE7.txt"); break;
		case 0xe2136399: success = DSPSymbols::ReadAnnotatedAssembly("../../docs/DSP/DSP_UC_EB79C705.txt"); break;
		default: success = false; break;
	}

	if (!success) {
		DSPSymbols::AutoDisassembly(0x0, 0x1000);
	}

	// Always add the ROM.
	DSPSymbols::AutoDisassembly(0x8000, 0x9000);
#if defined(HAVE_WX) && HAVE_WX
	if (m_DebuggerFrame)
		m_DebuggerFrame->Refresh();
#endif
	return crc;
}

void DSPHost_UpdateDebugger()
{
#if defined(HAVE_WX) && HAVE_WX
	if (m_DebuggerFrame)
		m_DebuggerFrame->Refresh();
#endif
}
