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

#include "Common.h"
#include "Globals.h"
#include "ChunkFile.h"
#include "resource.h"

#ifdef _WIN32
#include "PCHW/DSoundStream.h"
#include "AboutDlg.h"
#include "ConfigDlg.h"
#else
#include "PCHW/AOSoundStream.h"
#endif

#include "PCHW/Mixer.h"

#include "DSPHandler.h"
#include "Config.h"

DSPInitialize g_dspInitialize;
u8* g_pMemory;

struct DSPState
{
	u32 CPUMailbox;
	bool CPUMailbox_Written[2];

	u32 DSPMailbox;
	bool DSPMailbox_Read[2];

	DSPState()
	{
		CPUMailbox = 0x00000000;
		CPUMailbox_Written[0] = false;
		CPUMailbox_Written[1] = false;

		DSPMailbox = 0x00000000;
		DSPMailbox_Read[0] = true;
		DSPMailbox_Read[1] = true;
	}
};

DSPState g_dspState;


#ifdef _WIN32
HINSTANCE g_hInstance = NULL;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, // DLL module handle
		DWORD dwReason, // reason called
		LPVOID lpvReserved) // reserved
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
	sprintf(_PluginInfo->Name, "Dolphin DSP-HLE Plugin (DebugFast) ");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin DSP-HLE Plugin ");
#else
	sprintf(_PluginInfo->Name, "Dolphin DSP-HLE Plugin (Debug) ");
#endif
#endif
}

void DllAbout(HWND _hParent)
{
#ifdef _WIN32
	CAboutDlg aboutDlg;
	aboutDlg.DoModal(_hParent);
#endif
}

void DllConfig(HWND _hParent)
{
#ifdef _WIN32
	CConfigDlg configDlg;
	configDlg.DoModal(_hParent);
#endif
}

void DSP_Initialize(DSPInitialize _dspInitialize)
{
	g_Config.LoadDefaults();
	g_Config.Load();

	g_dspInitialize = _dspInitialize;

	g_pMemory = g_dspInitialize.pGetMemoryPointer(0);

	CDSPHandler::CreateInstance();

#ifdef _WIN32
	DSound::DSound_StartSound((HWND)g_dspInitialize.hWnd, 48000, Mixer);
#else
	AOSound::AOSound_StartSound(48000, Mixer);
#endif
}

void DSP_Shutdown()
{
	// delete the UCodes
#ifdef _WIN32
	DSound::DSound_StopSound();
#else
	AOSound::AOSound_StopSound();
#endif
	CDSPHandler::Destroy();
}

void DSP_DoState(unsigned char **ptr, int mode) {
	PointerWrap p(ptr, mode);
}

unsigned short DSP_ReadMailboxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return((g_dspState.CPUMailbox >> 16) & 0xFFFF);
	}
	else
	{
		return(CDSPHandler::GetInstance().AccessMailHandler().ReadDSPMailboxHigh());
	}
}

unsigned short DSP_ReadMailboxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return(g_dspState.CPUMailbox & 0xFFFF);
	}
	else
	{
		return(CDSPHandler::GetInstance().AccessMailHandler().ReadDSPMailboxLow());
	}
}

void Update_DSP_WriteRegister()
{
	// check if the whole message is complete and if we can send it
	if (g_dspState.CPUMailbox_Written[0] && g_dspState.CPUMailbox_Written[1])
	{
		CDSPHandler::GetInstance().SendMailToDSP(g_dspState.CPUMailbox);
		g_dspState.CPUMailbox_Written[0] = g_dspState.CPUMailbox_Written[1] = false;
		g_dspState.CPUMailbox = 0; // Mail sent so clear it to show that it is progressed
	}
}

void DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned short _Value)
{
	if (_CPUMailbox)
	{
		g_dspState.CPUMailbox = (g_dspState.CPUMailbox & 0xFFFF) | (_Value << 16);
		g_dspState.CPUMailbox_Written[0] = true;

		Update_DSP_WriteRegister();
	}
	else
	{
		PanicAlert("CPU can't write %08x to DSP mailbox", _Value);
	}
}

void DSP_WriteMailboxLow(bool _CPUMailbox, unsigned short _Value)
{
	if (_CPUMailbox)
	{
		g_dspState.CPUMailbox = (g_dspState.CPUMailbox & 0xFFFF0000) | _Value;
		g_dspState.CPUMailbox_Written[1] = true;

		Update_DSP_WriteRegister();
	}
	else
	{
		PanicAlert("CPU can't write %08x to DSP mailbox", _Value);
	}
}

unsigned short DSP_WriteControlRegister(unsigned short _Value)
{
	return CDSPHandler::GetInstance().WriteControlRegister(_Value);
}

unsigned short DSP_ReadControlRegister()
{
	return CDSPHandler::GetInstance().ReadControlRegister();
}

void DSP_Update(int cycles)
{
	CDSPHandler::GetInstance().Update();
}

void DSP_SendAIBuffer(unsigned int address, int sample_rate)
{
	short samples[16] = {0};  // interleaved stereo
	if (address) {
		for (int i = 0; i < 16; i++) {
			samples[i] = Memory_Read_U16(address + i * 2);
		}
	}
	Mixer_PushSamples(samples, 32 / 4, sample_rate);

	static int counter = 0;
	counter++;
#ifdef _WIN32
	if ((counter & 255) == 0)
		DSound::DSound_UpdateSound();
#endif

}
