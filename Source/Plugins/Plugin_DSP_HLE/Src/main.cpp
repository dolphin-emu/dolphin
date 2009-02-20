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

#include <iostream>

#include "Globals.h" // Local

#if defined(HAVE_WX) && HAVE_WX
#include "ConfigDlg.h"
#include "Debugger/File.h" // For file logging
#include "Debugger/Debugger.h" // For the CDebugger class
CDebugger* m_frame = NULL;
#endif

#include "ConsoleWindow.h" // Common: For the Windows console
#include "ChunkFile.h"
#include "WaveFile.h"
#include "PCHW/Mixer.h"
#include "DSPHandler.h"
#include "Config.h"
#include "Setup.h"
#include "StringUtil.h"

#include "PCHW/AOSoundStream.h"
#include "PCHW/DSoundStream.h"
#include "PCHW/NullSoundStream.h"

// Declarations and definitions
DSPInitialize g_dspInitialize;
u8* g_pMemory;
extern std::vector<std::string> sMailLog, sMailTime;
std::string gpName;

SoundStream *soundStream = NULL;

// Set this if you want to log audio. search for log_ai in this file to see the filename.
bool log_ai = false;
WaveFileWriter g_wave_writer;


// Mailbox utility
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


// Open and close console
void OpenConsole()
{
#if defined (_WIN32)
	Console::Open(155, 100, "Sound Debugging"); // give room for 100 rows
	Console::Print("OpenConsole > Console opened\n");
	MoveWindow(Console::GetHwnd(), 0,400, 1280,550, true); // move window, TODO: make this
	// adjustable from the debugging window
#endif
}

void CloseConsole()
{
#if defined (_WIN32)
	FreeConsole();
#endif
}


// Exported fuctions

// Create debugging window - We could use use wxWindow win; new CDebugger(win) like nJoy but I don't
// know why it would be better. - There's a lockup problem with ShowModal(), but Show() doesn't work
// because then DLL_PROCESS_DETACH is called immediately after DLL_PROCESS_ATTACH.
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
	sprintf(_PluginInfo	->Name, "Dolphin DSP-HLE Plugin (Debug) ");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals) {
}

void DllConfig(HWND _hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	// (shuffle2) TODO: reparent dlg with DolphinApp
	ConfigDialog dlg(NULL);

	// Add avaliable output options
	if (DSound::isValid())
		dlg.AddBackend("DSound");
	if (AOSound::isValid())
		dlg.AddBackend("AOSound");
	dlg.AddBackend("NullSound");

	// Show the window
	dlg.ShowModal();
#endif
}

void Initialize(void *init)
{
	//Console::Open(80, 5000);

	g_dspInitialize = *(DSPInitialize*)init;

	g_Config.Load();
	g_pMemory = g_dspInitialize.pGetMemoryPointer(0);

#if defined(_DEBUG) || defined(DEBUGFAST)
	gpName = g_dspInitialize.pName(); // save the game name globally
	for (u32 i = 0; i < gpName.length(); ++i) // and fix it
	{
		fprintf(stderr,"%c", gpName[i]);
		std::cout << gpName[i];
		if (gpName[i] == ':') gpName[i] = ' ';
	}
	fprintf(stderr, "\n");
#endif

	CDSPHandler::CreateInstance();

	if (g_Config.sBackend == "DSound")
	{
		if (DSound::isValid())
			soundStream = new DSound(48000, Mixer, g_dspInitialize.hWnd);
	}
	else if (g_Config.sBackend == "AOSound")
	{
		if (AOSound::isValid())
			soundStream = new AOSound(48000, Mixer);
	}
	else if (g_Config.sBackend == "NullSound")
	{
		soundStream = new NullSound(48000, Mixer_MixUCode);
	}
	else
	{
		PanicAlert("Cannot recognize backend %s", g_Config.sBackend);
		return;
	}

#if defined(WIN32) && defined(_DEBUG)
	int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
	_CrtSetDbgFlag(tmpflag);
#endif

	if (soundStream)
	{
		if (!soundStream->Start())
		{
			PanicAlert("Could not initialize backend %s, falling back to NULL", 
				g_Config.sBackend);
			delete soundStream;
			soundStream = new NullSound(48000, Mixer);
			soundStream->Start();
		}
	}
	else
	{
		PanicAlert("Sound backend %s is not valid, falling back to NULL", 
			g_Config.sBackend);
		delete soundStream;
		soundStream = new NullSound(48000, Mixer);
		soundStream->Start();
	}

	// Start the sound recording
	if (log_ai)
	{
		g_wave_writer.Start("ai_log.wav");
		g_wave_writer.SetSkipSilence(false);
	}
}

void DSP_StopSoundStream()
{
	if (!soundStream)
		PanicAlert("Can't stop non running SoundStream!");
	soundStream->Stop();
	delete soundStream;
	soundStream = NULL;
}

void Shutdown()
{
	// Check that soundstream already is stopped.
	if (soundStream)
		PanicAlert("SoundStream alive in DSP::Shutdown!");

	// Stop the sound recording
	if (log_ai)
		g_wave_writer.Stop();

	// Delete the UCodes
	CDSPHandler::Destroy();

#if defined(HAVE_WX) && HAVE_WX
	// Reset mails
	if (m_frame)
	{
		sMailLog.clear();
		sMailTime.clear();
		m_frame->sMail.clear();
		m_frame->sMailEnd.clear();
	}
#endif
}

void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);
}


////////////////////////////////////////////////////////////////////////////////////////
// Mailbox fuctions
// ¯¯¯¯¯¯¯¯¯¯¯¯
unsigned short DSP_ReadMailboxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return (g_dspState.CPUMailbox >> 16) & 0xFFFF;
	}
	else
	{
		return CDSPHandler::GetInstance().AccessMailHandler().ReadDSPMailboxHigh();
	}
}

unsigned short DSP_ReadMailboxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return g_dspState.CPUMailbox & 0xFFFF;
	}
	else
	{
		return CDSPHandler::GetInstance().AccessMailHandler().ReadDSPMailboxLow();
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
/////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////
// Other DSP fuctions
// ¯¯¯¯¯¯¯¯¯¯¯¯
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

/* Other Audio will pass through here. The kind of audio that sometimes are used together with pre-drawn
   movies. This audio can be disabled further inside Mixer_PushSamples(), the reason that we don't disable
   this entire function when Other Audio is disabled is that then we can't turn it back on again once the
   game has started. */
void DSP_SendAIBuffer(unsigned int address, int sample_rate)
{
	// TODO: This is not yet fully threadsafe.
	if (!soundStream) {
		return;
	}

	if (soundStream->usesMixer())
	{
		short samples[16] = {0};  // interleaved stereo
		if (address)
		{
			for (int i = 0; i < 16; i++)
			{
				samples[i] = Memory_Read_U16(address + i * 2);
			}

			// Write the audio to a file
			if (log_ai)
				g_wave_writer.AddStereoSamples(samples, 8);
		}
		Mixer_PushSamples(samples, 32 / 4, sample_rate);
	}

	// SoundStream is updated only when necessary (there is no 70 ms limit
	// so each sample now triggers the sound stream)
	
	// TODO: think about this.
	static int counter = 0;
	counter++;
	if ((counter & 31) == 0 && soundStream)
		soundStream->Update();
}
/////////////////////////////////////
