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


// =======================================================================================
// Includes
// --------------
#include "Common.h" // Common
#include "WaveFile.h"
#include "CommonTypes.h"
#include "Mixer.h"

#include "Globals.h" // Local
#include "gdsp_interpreter.h"
#include "gdsp_interface.h"
#include "disassemble.h"

#ifdef _WIN32
	#include "DisAsmDlg.h"
	#include "DSoundStream.h"
	#include "Logging/Logging.h" // For Logging

	HINSTANCE g_hInstance = NULL;
	HANDLE g_hDSPThread = NULL;
	CRITICAL_SECTION g_CriticalSection;
	CDisAsmDlg g_Dialog;
#else
	#define WINAPI
	#define LPVOID void*
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <pthread.h>
	#include "AOSoundStream.h"
	pthread_t g_hDSPThread = NULL;
#endif

#include "ChunkFile.h"
// ==============


// =======================================================================================
// Global declarations and definitions
// --------------
PLUGIN_GLOBALS* globals = NULL;
DSPInitialize g_dspInitialize;

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

uint32 g_LastDMAAddress = 0;
uint32 g_LastDMASize = 0;

extern u32 m_addressPBs;
bool AXTask(u32& _uMail);

bool bCanWork = false;

// Set this if you want to log audio. search for log_ai in this file to see the filename.
static bool log_ai = false;
WaveFileWriter g_wave_writer;
// ==============


#ifdef _WIN32
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
#endif

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

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllAbout(HWND _hParent)
{}


void DllConfig(HWND _hParent)
{}


void DoState(unsigned char **ptr, int mode) {
	PointerWrap p(ptr, mode);
}

void DllDebugger(HWND _hParent, bool Show)
{
#ifdef _WIN32
	#if defined (_DEBUG) || defined (DEBUGFAST)
		g_Dialog.Create(NULL); //_hParent);
		g_Dialog.ShowWindow(SW_SHOW);
		//		MoveWindow(g_Dialog.m_hWnd, 450,0, 780,530, true);
		
		// Open the console window
		//		Console::Open(155, 100, "Sound Debugging"); // give room for 100 rows
		//		Console::Print("DllDebugger > Console opened\n");
		// Todo: Make this adjustable from the Debugging window
		//		MoveWindow(Console::GetHwnd(), 0,400, 1280,500, true);
	#else
		MessageBox(0, "Can't open debugging window in the Release build of this plugin.", "DSP LLE", 0);
	#endif
#endif
}


// =======================================================================================
// Regular thread
// --------------
#ifdef _WIN32
DWORD WINAPI dsp_thread(LPVOID lpParameter)
#else
void* dsp_thread(void* lpParameter)
#endif
{
	while (1)
	{
		if (!gdsp_run())
		{
			PanicAlert("ERROR: DSP Halt");
			return 0;
		}
	}
}
// ==============


// =======================================================================================
// Debug thread
// --------------
#ifdef _WIN32
DWORD WINAPI dsp_thread_debug(LPVOID lpParameter)
#else
void* dsp_thread_debug(void* lpParameter)
#endif
{
    
	//if (g_hDSPThread)
	//{
	//	return NULL; // enable this to disable the plugin
	//}
#ifdef _WIN32

	while (1)
	{
		Logging(); // logging

		if (g_Dialog.CanDoStep())
		{
			gdsp_runx(1);
		}
		else
		{
			Sleep(100);
		}
	}
#endif
        return NULL;
}
// ==============


void DSP_DebugBreak()
{
#ifdef _WIN32
#if defined(_DEBUG) || defined(DEBUGFAST)
	g_Dialog.DebugBreak();
#endif
#endif
}


void dspi_req_dsp_irq()
{
	g_dspInitialize.pGenerateDSPInterrupt();
}

void Initialize(void *init)
{
    bCanWork = true;
    g_dspInitialize = *(DSPInitialize*)init;

	gdsp_init();
	g_dsp.step_counter = 0;
	g_dsp.cpu_ram = g_dspInitialize.pGetMemoryPointer(0);
	g_dsp.irq_request = dspi_req_dsp_irq;
	gdsp_reset();

	if (!gdsp_load_rom((char *)DSP_ROM_FILE))
	{
                bCanWork = false;
                PanicAlert("No DSP ROM");
		ERROR_LOG(DSPHLE, "Cannot load DSP ROM\n");
	}

	if (!gdsp_load_coef((char *)DSP_COEF_FILE))
	{
                bCanWork = false;
                PanicAlert("No DSP COEF");
		ERROR_LOG(DSPHLE, "Cannot load DSP COEF\n");
	}

        if(!bCanWork)
            return; // TODO: Don't let it work

// ---------------------------------------------------------------------------------------
// First create DSP_UCode.bin by setting "#define DUMP_DSP_IMEM   1" in Globals.h. Then
// make the disassembled file here.
// --------------
//		Dump UCode to file...

   	FILE* t = fopen("C:\\_\\DSP_UC_09CD143F.txt", "wb");
	if (t != NULL)
	{
   		gd_globals_t gdg;
   		gd_dis_file(&gdg, (char *)"C:\\_\\DSP_UC_09CD143F.bin", t);
   		fclose(t);   
	}
// --------------

#ifdef _WIN32
#if defined(_DEBUG) || defined(DEBUGFAST)
	g_hDSPThread = CreateThread(NULL, 0, dsp_thread_debug, 0, 0, NULL);
#else
	g_hDSPThread = CreateThread(NULL, 0, dsp_thread, 0, 0, NULL);
#endif // DEBUG
#else
#if _DEBUG
        pthread_create(&g_hDSPThread, NULL, dsp_thread_debug, (void *)NULL);
#else
	pthread_create(&g_hDSPThread, NULL, dsp_thread, (void *)NULL);
#endif // DEBUG
#endif // WIN32
        
	if (log_ai) {
		g_wave_writer.Start("C:\\_\\ai_log.wav");
		g_wave_writer.SetSkipSilence(false);
	}

#ifdef _WIN32
	InitializeCriticalSection(&g_CriticalSection);
	DSound::DSound_StartSound((HWND)g_dspInitialize.hWnd, 48000, Mixer);
#else
	AOSound::AOSound_StartSound(48000, Mixer);
#endif
}

void DSP_StopSoundStream()
{
#ifdef _WIN32
	if (g_hDSPThread != NULL)
	{
        TerminateThread(g_hDSPThread, 0);
    }
#else
	// Isn't pthread_cancel kind of evil?
    pthread_cancel(g_hDSPThread);
#endif
}

void Shutdown(void)
{
	if (log_ai)
		g_wave_writer.Stop();
}

u16 DSP_WriteControlRegister(u16 _uFlag)
{
	gdsp_write_cr(_uFlag);
	return(gdsp_read_cr());
}

u16 DSP_ReadControlRegister()
{
	return(gdsp_read_cr());
}

u16 DSP_ReadMailboxHigh(bool _CPUMailbox)
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

u16 DSP_ReadMailboxLow(bool _CPUMailbox)
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

void DSP_WriteMailboxHigh(bool _CPUMailbox, u16 _uHighMail)
{
	if (_CPUMailbox)
	{
		if (gdsp_mbox_peek(GDSP_MBOX_CPU) & 0x80000000)
		{
			ERROR_LOG(DSPHLE, "Mailbox isnt empty ... strange");
		}

#if PROFILE
		if ((_uHighMail) == 0xBABE)
		{
			ProfilerStart();
		}
#endif

		gdsp_mbox_write_h(GDSP_MBOX_CPU, _uHighMail);
	}
	else
	{
		ERROR_LOG(DSPHLE, "CPU cant write to DSP mailbox");
	}
}

void DSP_WriteMailboxLow(bool _CPUMailbox, u16 _uLowMail)
{
	if (_CPUMailbox)
	{
		gdsp_mbox_write_l(GDSP_MBOX_CPU, _uLowMail);

		u32 uAddress = gdsp_mbox_peek(GDSP_MBOX_CPU);
		u16 errpc = g_dsp.err_pc;

		DEBUG_LOG(DSPHLE, "Write CPU Mail: 0x%08x (pc=0x%04x)\n", uAddress, errpc);

		// ---------------------------------------------------------------------------------------
		// I couldn't find any better way to detect the AX mails so this had to do. Please feel free
		// to change it.
		// --------------
		if ((errpc == 0x0054 || errpc == 0x0055) && m_addressPBs == 0)
		{
			DEBUG_LOG(DSPHLE, "AXTask ======== 0x%08x (pc=0x%04x)", uAddress, errpc);
			AXTask(uAddress);
		}
	}
	else
	{
		ERROR_LOG(DSPHLE, "CPU cant write to DSP mailbox");
	}
}


void DSP_Update(int cycles)
{
	if (g_hDSPThread)
	{
		return;
	}
	#ifdef _WIN32
	if (g_Dialog.CanDoStep())
	{
		gdsp_runx(100); // cycles
	}
	#endif
}


void DSP_SendAIBuffer(unsigned int address, int sample_rate)
{
	short samples[16] = {0};  // interleaved stereo
	if (address) {
		for (int i = 0; i < 16; i++) {
			samples[i] = Memory_Read_U16(address + i * 2);
		}
		if (log_ai)
			g_wave_writer.AddStereoSamples(samples, 8);
	}
	Mixer_PushSamples(samples, 32 / 4, sample_rate);

	static int counter = 0;
	counter++;
#ifdef _WIN32
	if ((counter & 255) == 0)
		DSound::DSound_UpdateSound();
#endif
}



