// Copyright (C) 2003-2009 Dolphin Project.

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


#include "Common.h" // Common
#include "CommonTypes.h"
#include "Mixer.h"

#include "Globals.h" // Local
#include "DSPInterpreter.h"
#include "DSPHWInterface.h"
#include "disassemble.h"
#include "DSPSymbols.h"
#include "Config.h"

#include "AudioCommon.h"
#include "Logging/Logging.h" // For Logging

#include "Thread.h"
#include "ChunkFile.h"

#include "DSPTables.h"
#include "DSPCore.h"

#if defined(HAVE_WX) && HAVE_WX
#include "DSPConfigDlgLLE.h"
#include "Debugger/Debugger.h" // For the DSPDebuggerLLE class
DSPDebuggerLLE* m_DebuggerFrame = NULL;
#endif

PLUGIN_GLOBALS* globals = NULL;
DSPInitialize g_dspInitialize;
Common::Thread *g_hDSPThread = NULL;

SoundStream *soundStream = NULL;

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

bool bCanWork = false;
bool bIsRunning = false;

//////////////////////////////////////////////////////////////////////////
// UGLY wxw stuff, TODO fix up
// wxWidgets: Create the wxApp
#if defined(HAVE_WX) && HAVE_WX
class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};

IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

// DllMain
#ifdef _WIN32
HINSTANCE g_hInstance = NULL;

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, // DLL module handle
					  DWORD dwReason, // reason called
					  LPVOID lpvReserved) // reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{

			// more stuff wx needs
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);

			// This is for ?
			if ( !wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break;

	case DLL_PROCESS_DETACH:
		wxEntryCleanup(); // use this or get a crash
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
{

}

void DllConfig(HWND _hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	// (shuffle2) TODO: reparent dlg with DolphinApp
	DSPConfigDialogLLE dlg(NULL);

	// add backends
	std::vector<std::string> backends = AudioCommon::GetSoundBackends();

	for (std::vector<std::string>::const_iterator iter = backends.begin(); 
		 iter != backends.end(); ++iter) {
		dlg.AddBackend((*iter).c_str());
	}

	// Show the window
	dlg.ShowModal();
#endif
}


void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);
}

void DllDebugger(HWND _hParent, bool Show)
{
#if defined(HAVE_WX) && HAVE_WX
	if (!m_DebuggerFrame)
		m_DebuggerFrame = new DSPDebuggerLLE(NULL);

	if (Show)
		m_DebuggerFrame->Show();
	else
		m_DebuggerFrame->Hide();
#endif
}


// Regular thread
THREAD_RETURN dsp_thread(void* lpParameter)
{
	while (bIsRunning)
	{
		DSPInterpreter::Run();
	}
	return 0;
}

void DSP_DebugBreak()
{
#if defined(HAVE_WX) && HAVE_WX
	if(m_DebuggerFrame)
		m_DebuggerFrame->DebugBreak();
#endif
}

void Initialize(void *init)
{
    bCanWork = true;
    g_dspInitialize = *(DSPInitialize*)init;

	g_Config.Load();

	std::string irom_filename = File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + DSP_IROM;
	std::string coef_filename = File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + DSP_COEF;
	bCanWork = DSPCore_Init(irom_filename.c_str(), coef_filename.c_str());
	g_dsp.cpu_ram = g_dspInitialize.pGetMemoryPointer(0);
	DSPCore_Reset();

	if (!bCanWork)
	{
		DSPCore_Shutdown();
		return;
	}

	bIsRunning = true;

	InitInstructionTable();

	if (g_dspInitialize.bOnThread)
	{
		g_hDSPThread = new Common::Thread(dsp_thread, NULL);
	}
	soundStream = AudioCommon::InitSoundStream();

#if defined(HAVE_WX) && HAVE_WX
	if (m_DebuggerFrame)
		m_DebuggerFrame->Refresh();
#endif
}

void DSP_StopSoundStream()
{
	DSPInterpreter::Stop();
	bIsRunning = false;
	if (g_dspInitialize.bOnThread)
	{
		delete g_hDSPThread;
		g_hDSPThread = NULL;
	}
}

void Shutdown()
{
	AudioCommon::ShutdownSoundStream();
	DSPCore_Shutdown();
}

u16 DSP_WriteControlRegister(u16 _uFlag)
{
	DSPInterpreter::WriteCR(_uFlag);
	return DSPInterpreter::ReadCR();
}

u16 DSP_ReadControlRegister()
{
	return DSPInterpreter::ReadCR();
}

u16 DSP_ReadMailboxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
		return gdsp_mbox_read_h(GDSP_MBOX_CPU);
	else
		return gdsp_mbox_read_h(GDSP_MBOX_DSP);
}

u16 DSP_ReadMailboxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
		return gdsp_mbox_read_l(GDSP_MBOX_CPU);
	else
		return gdsp_mbox_read_l(GDSP_MBOX_DSP);
}

void DSP_WriteMailboxHigh(bool _CPUMailbox, u16 _uHighMail)
{
	if (_CPUMailbox)
	{
		if (gdsp_mbox_peek(GDSP_MBOX_CPU) & 0x80000000)
		{
			ERROR_LOG(DSPLLE, "Mailbox isnt empty ... strange");
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
		ERROR_LOG(DSPLLE, "CPU cant write to DSP mailbox");
	}
}

void DSP_WriteMailboxLow(bool _CPUMailbox, u16 _uLowMail)
{
	if (_CPUMailbox)
	{
		gdsp_mbox_write_l(GDSP_MBOX_CPU, _uLowMail);
	}
	else
	{
		ERROR_LOG(DSPLLE, "CPU cant write to DSP mailbox");
	}
}

void DSP_Update(int cycles)
{
	// This gets called VERY OFTEN. The soundstream update might be expensive so only do it 200 times per second or something.
	const int cycles_between_ss_update = 80000000 / 200;
	static int cycle_count = 0;
	cycle_count += cycles;
	if (cycle_count > cycles_between_ss_update)
	{
		while (cycle_count > cycles_between_ss_update)
			cycle_count -= cycles_between_ss_update;
		soundStream->Update();
	}

	// If we're not on a thread, run cycles here.
	if (!g_dspInitialize.bOnThread)
	{
		DSPInterpreter::RunCycles(cycles);
	}
}

void DSP_SendAIBuffer(unsigned int address, int sample_rate)
{
	if (soundStream->GetMixer())
	{
		short samples[16] = {0};  // interleaved stereo
		if (address)
		{
			for (int i = 0; i < 16; i++)
			{
				samples[i] = Memory_Read_U16(address + i * 2);
			}
		}
		soundStream->GetMixer()->PushSamples(samples, 32 / 4, sample_rate);
	}

	// SoundStream is updated only when necessary (there is no 70 ms limit
	// so each sample now triggers the sound stream)
	
	// TODO: think about this.
	//static int counter = 0;
	//counter++;
	if (/*(counter & 31) == 0 &&*/ soundStream)
		soundStream->Update();
}
