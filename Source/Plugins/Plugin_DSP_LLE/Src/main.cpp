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

#include "Globals.h"

#include "gdsp_interpreter.h"
#include "gdsp_interface.h"
#include "disassemble.h"

#ifdef _WIN32
#include "DisAsmDlg.h"
#include "DSoundStream.h"

HINSTANCE g_hInstance = NULL;
HANDLE g_hDSPThread = NULL;
CRITICAL_SECTION g_CriticalSection;
CDisAsmDlg g_Dialog;
#endif

DSPInitialize g_dspInitialize;

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1


uint32 g_LastDMAAddress = 0;
uint32 g_LastDMASize = 0;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, // DLL module handle
		DWORD dwReason,             // reason called
		LPVOID lpvReserved)         // reserved
{
	switch (dwReason)
	{
	    case DLL_PROCESS_ATTACH:
		    break;

	    case DLL_PROCESS_DETACH:
		    break;

	    default:
		    break;
	}

	g_hInstance = hinstDLL;
	return(TRUE);
}


void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_DSP;

#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE Plugin (DebugFast)");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE Plugin");
#else
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE Plugin (Debug)");
#endif
#endif
}


void DllAbout(HWND _hParent)
{}


void DllConfig(HWND _hParent)
{}


void DllDebugger(HWND _hParent)
{
#if defined (_DEBUG) && defined (_WIN32)
	g_Dialog.Create(NULL); //_hParent);
	g_Dialog.ShowWindow(SW_SHOW);
#endif

}


DWORD WINAPI dsp_thread(LPVOID lpParameter)
{
	while (1)
	{
		if (!gdsp_run())
		{
			ErrorLog("*** DSP: CRITICAL ERROR ***\n");
			//return 0;
			exit(0);
		}
	}
}


DWORD WINAPI dsp_thread_debug(LPVOID lpParameter)
{
	while (1)
	{
		if (g_Dialog.CanDoStep())
		{
			gdsp_runx(1);
		}
		else
		{
			Sleep(100);
		}
	}
}


void DSP_DebugBreak()
{
#ifdef _WIN32
#ifdef _DEBUG
	g_Dialog.DebugBreak();
#endif
#endif
}


void dspi_req_dsp_irq()
{
	g_dspInitialize.pGenerateDSPInterrupt();
}


void Mixer(short* buffer, int numSamples, int bits, int rate, int channels)
{}


void DSP_Initialize(DSPInitialize _dspInitialize)
{
	{
		g_dspInitialize = _dspInitialize;

		gdsp_init();
		g_dsp.step_counter = 0;
		g_dsp.cpu_ram = g_dspInitialize.pGetMemoryPointer(0);
		g_dsp.irq_request = dspi_req_dsp_irq;
		gdsp_reset();

		if (!gdsp_load_rom("data\\dsp_rom.bin"))
		{
			ErrorLog("Cannot load DSP ROM\n");
		}

		if (!gdsp_load_coef("data\\dsp_coef.bin"))
		{
			ErrorLog("Cannot load DSP COEF\n");
		}


/*		Dump UCode to file...
   	FILE* t = fopen("e:\\hmm.txt", "wb");
   	gd_globals_t gdg;
   	gd_dis_file(&gdg, "D:\\DSP_UCode.bin", t);
   	fclose(t);  */
	}


#if _DEBUG
	g_hDSPThread = CreateThread(NULL, 0, dsp_thread_debug, 0, 0, NULL);

#else
	g_hDSPThread = CreateThread(NULL, 0, dsp_thread, 0, 0, NULL);
#endif



	InitializeCriticalSection(&g_CriticalSection);

	DSound::DSound_StartSound((HWND)g_dspInitialize.hWnd, 32000, Mixer);
}


void DSP_Shutdown(void)
{
	if (g_hDSPThread != NULL)
	{
		TerminateThread(g_hDSPThread, 0);
	}
}


unsigned __int16 DSP_WriteControlRegister(unsigned __int16 _uFlag)
{
	gdsp_write_cr(_uFlag);
	return(gdsp_read_cr());
}


unsigned __int16 DSP_ReadControlRegister()
{
	return(gdsp_read_cr());
}


unsigned __int16 DSP_ReadMailboxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return(gdsp_mbox_read_h(GDSP_MBOX_CPU));
	}
	else
	{
		return(gdsp_mbox_read_h(GDSP_MBOX_DSP));
	}
}


unsigned __int16 DSP_ReadMailboxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return(gdsp_mbox_read_l(GDSP_MBOX_CPU));
	}
	else
	{
		return(gdsp_mbox_read_l(GDSP_MBOX_DSP));
	}
}


void DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned __int16 _uHighMail)
{
	if (_CPUMailbox)
	{
		if (gdsp_mbox_peek(GDSP_MBOX_CPU) & 0x80000000)
		{
			ErrorLog("Mailbox isnt empty ... strange");
		}

		gdsp_mbox_write_h(GDSP_MBOX_CPU, _uHighMail);
	}
	else
	{
		ErrorLog("CPU cant write to DSP mailbox");
	}
}


void DSP_WriteMailboxLow(bool _CPUMailbox, unsigned __int16 _uLowMail)
{
	if (_CPUMailbox)
	{
		gdsp_mbox_write_l(GDSP_MBOX_CPU, _uLowMail);

		DebugLog("Write CPU Mail: 0x%08x (pc=0x%04x)\n", gdsp_mbox_peek(GDSP_MBOX_CPU), g_dsp.err_pc);
	}
	else
	{
		ErrorLog("CPU cant write to DSP mailbox");
	}
}


void DSP_Update()
{
	if (g_hDSPThread)
	{
		return;
	}

	if (g_Dialog.CanDoStep())
	{
		gdsp_runx(100);
	}
}


void DSP_SendAIBuffer(unsigned __int32 _Address, unsigned __int32 _Size)
{
	uint32 Size = _Size * 16 * 2; // 16bit per sample, two channels

	g_LastDMAAddress = _Address;
	g_LastDMASize = Size;
}
