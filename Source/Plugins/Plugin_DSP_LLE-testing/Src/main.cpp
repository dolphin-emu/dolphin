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
#include "gdsp_interpreter.h"
#include "gdsp_interface.h"
#include "disassemble.h"
#include "Config.h"

#include "AudioCommon.h"
#include "Logging/Logging.h" // For Logging

#include "Thread.h"
#include "ChunkFile.h"

#if defined(HAVE_WX) && HAVE_WX
#include "DSPConfigDlgLLE.h"
#include "Debugger/Debugger.h" // For the CDebugger class
CDebugger* m_frame = NULL;
#endif

PLUGIN_GLOBALS* globals = NULL;
DSPInitialize g_dspInitialize;
Common::Thread *g_hDSPThread = NULL;

SoundStream *soundStream = NULL;

#define GDSP_MBOX_CPU   0
#define GDSP_MBOX_DSP   1

u32 g_LastDMAAddress = 0;
u32 g_LastDMASize = 0;

extern u32 m_addressPBs;
bool AXTask(u32& _uMail);

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
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE-Testing Plugin (DebugFast)");
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE-Testing Plugin");
#else
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE-Testing Plugin (Debug)");
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
	if (m_frame && Show) // if we have created it, let us show it again
	{
		m_frame->DoShow();
	}
	else if (!m_frame && Show)
	{
		m_frame = new CDebugger(NULL);
		m_frame->Show();
	}
	else if (m_frame && !Show)
	{
		m_frame->DoHide();
	}
#endif
}


// Regular thread
THREAD_RETURN dsp_thread(void* lpParameter)
{
	while (bIsRunning)
	{
		if (!gdsp_run())
		{
			ERROR_LOG(AUDIO, "DSP Halt");
			return 0;
		}
	}
	return 0;
}

// Debug thread
THREAD_RETURN dsp_thread_debug(void* lpParameter)
{
	return NULL;
}

void DSP_DebugBreak()
{
}


void dspi_req_dsp_irq()
{
	g_dspInitialize.pGenerateDSPInterrupt();
}

void Initialize(void *init)
{
    bCanWork = true;
    g_dspInitialize = *(DSPInitialize*)init;

	g_Config.Load();
	gdsp_init();
	g_dsp.step_counter = 0;
	g_dsp.cpu_ram = g_dspInitialize.pGetMemoryPointer(0);
	g_dsp.irq_request = dspi_req_dsp_irq;
	gdsp_reset();

	if (!gdsp_load_rom((char *)DSP_ROM_FILE)) {
		bCanWork = false;
		PanicAlert("Cannot load DSP ROM");
	}
	
	if (!gdsp_load_coef((char *)DSP_COEF_FILE)) {
		bCanWork = false;
		PanicAlert("Cannot load DSP COEF");
	}
	
	if(!bCanWork)
		return; // TODO: Don't let it work

	bIsRunning = true;
	
	g_hDSPThread = new Common::Thread(dsp_thread, NULL);
	
	soundStream = AudioCommon::InitSoundStream();
}

void DSP_StopSoundStream()
{
	delete g_hDSPThread;
	g_hDSPThread = NULL;
}

void Shutdown(void)
{
	bIsRunning = false;
	gdsp_stop();
	AudioCommon::ShutdownSoundStream();
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

		// I couldn't find any better way to detect the AX mails so this had to
		// do. Please feel free to change it.
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
		soundStream->Update();
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

			// Write the audio to a file
			//if (log_ai)
			//				g_wave_writer.AddStereoSamples(samples, 8);
		}
		soundStream->GetMixer()->PushSamples(samples, 32 / 4, sample_rate);
	}

	// SoundStream is updated only when necessary (there is no 70 ms limit
	// so each sample now triggers the sound stream)
	
	// TODO: think about this.
	static int counter = 0;
	counter++;
	if ((counter & 31) == 0 && soundStream)
		soundStream->Update();
}
