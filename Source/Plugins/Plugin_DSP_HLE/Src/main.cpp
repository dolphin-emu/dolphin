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

#include <iostream>

#include "Globals.h" // Local

#if defined(HAVE_WX) && HAVE_WX
#include "ConfigDlg.h"
DSPConfigDialogHLE* m_ConfigFrame = NULL;
#include "Debugger/File.h" // For file logging
#include "Debugger/Debugger.h"
DSPDebuggerHLE* m_DebuggerFrame = NULL;
#endif

#include "ChunkFile.h"
#include "HLEMixer.h"
#include "DSPHandler.h"
#include "Config.h"
#include "Setup.h"
#include "StringUtil.h"
#include "LogManager.h"


// Declarations and definitions
PLUGIN_GLOBALS* globals = NULL;
DSPInitialize g_dspInitialize;
u8* g_pMemory;
extern std::vector<std::string> sMailLog, sMailTime;

bool g_InitMixer = false;
SoundStream *soundStream = NULL;

// Mailbox utility
struct DSPState
{
	u32 CPUMailbox;
	u32 DSPMailbox;

	void Reset() {
		CPUMailbox = 0x00000000;
		DSPMailbox = 0x00000000;
	}

	DSPState()
	{
		Reset();
	}
};
DSPState g_dspState;

// Standard crap to make wxWidgets happy
#ifdef _WIN32
HINSTANCE g_hInstance;

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

BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
#if defined(HAVE_WX) && HAVE_WX
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if (!wxTheApp || !wxTheApp->CallOnInit())
				return FALSE;
#endif
		}
		break; 

	case DLL_PROCESS_DETACH:
#if defined(HAVE_WX) && HAVE_WX
		wxEntryCleanup();
#endif
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}
#endif

#if defined(HAVE_WX) && HAVE_WX
wxWindow* GetParentedWxWindow(HWND Parent)
{
#ifdef _WIN32
	wxSetInstance((HINSTANCE)g_hInstance);
#endif
	wxWindow *win = new wxWindow();
#ifdef _WIN32
	win->SetHWND((WXHWND)Parent);
	win->AdoptAttributesFromHWND();
#endif
	return win;
}
#endif


void DllDebugger(HWND _hParent, bool Show)
{
#if defined(HAVE_WX) && HAVE_WX
	if (Show)
	{
		if (!m_DebuggerFrame)
			m_DebuggerFrame = new DSPDebuggerHLE(NULL);	
			//m_DebuggerFrame = new DSPDebuggerHLE(GetParentedWxWindow(_hParent));
		m_DebuggerFrame->Show();
	}
	else
	{
		if (m_DebuggerFrame) m_DebuggerFrame->Close();
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


void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
       globals = _pPluginGlobals;
       LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllConfig(HWND _hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	// Load config settings
	g_Config.Load();
	g_Config.GameIniLoad(globals->game_ini);

	if (!m_ConfigFrame)
	{
		m_ConfigFrame = new DSPConfigDialogHLE(GetParentedWxWindow(_hParent));

		// add backends
		std::vector<std::string> backends = AudioCommon::GetSoundBackends();

		for (std::vector<std::string>::const_iterator iter = backends.begin(); 
			 iter != backends.end(); ++iter)
		{
			m_ConfigFrame->AddBackend((*iter).c_str());
		}

		// Only allow one open at a time
		m_ConfigFrame->ShowModal();

		delete m_ConfigFrame;
		m_ConfigFrame = 0;
	}
#endif
}


void Initialize(void *init)
{
	g_InitMixer = false;
	g_dspInitialize = *(DSPInitialize*)init;

	g_Config.Load();
	g_pMemory = g_dspInitialize.pGetMemoryPointer(0);

	g_dspState.Reset();

	CDSPHandler::CreateInstance();
}

void DSP_StopSoundStream()
{
}

void Shutdown()
{
	AudioCommon::ShutdownSoundStream();

	// Delete the UCodes
	CDSPHandler::Destroy();

#if defined(HAVE_WX) && HAVE_WX
	// Reset mails
	/*
	if (m_DebuggerFrame)
	{
		sMailLog.clear();
		sMailTime.clear();
		m_DebuggerFrame->sMail.clear();
		m_DebuggerFrame->sMailEnd.clear();
	}
	*/
#endif	
}

void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);
	p.Do(g_InitMixer);
	CDSPHandler::GetInstance().GetUCode()->DoState(p);
}

void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	DSP_ClearAudioBuffer((newState == PLUGIN_EMUSTATE_PLAY) ? false : true);
}

// Mailbox fuctions
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

void DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned short _Value)
{
	if (_CPUMailbox)
	{
		g_dspState.CPUMailbox = (g_dspState.CPUMailbox & 0xFFFF) | (_Value << 16);
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
		CDSPHandler::GetInstance().SendMailToDSP(g_dspState.CPUMailbox);
		// Mail sent so clear MSB to show that it is progressed
		g_dspState.CPUMailbox &= 0x7FFFFFFF; 
	}
	else
	{
		PanicAlert("CPU can't write %08x to DSP mailbox", _Value);
	}
}


// Other DSP fuctions
unsigned short DSP_WriteControlRegister(unsigned short _Value)
{
	UDSPControl Temp(_Value);
	if (!g_InitMixer)
	{
		if (!Temp.DSPHalt && Temp.DSPInit)
		{
			unsigned int AISampleRate, DACSampleRate;
			g_dspInitialize.pGetSampleRate(AISampleRate, DACSampleRate);
			soundStream = AudioCommon::InitSoundStream(new HLEMixer(AISampleRate, DACSampleRate)); 
			if(!soundStream) PanicAlert("Error starting up sound stream");
			// Mixer is initialized
			g_InitMixer = true;
		}
	}
	return CDSPHandler::GetInstance().WriteControlRegister(_Value);
}

unsigned short DSP_ReadControlRegister()
{
	return CDSPHandler::GetInstance().ReadControlRegister();
}

void DSP_Update(int cycles)
{
	// This is called OFTEN - better not do anything expensive!
	CDSPHandler::GetInstance().Update(cycles);
}

/* Other Audio will pass through here. The kind of audio that sometimes are
   used together with pre-drawn movies. This audio can be disabled further
   inside Mixer_PushSamples(), the reason that we don't disable this entire
   function when Other Audio is disabled is that then we can't turn it back on
   again once the game has started. */
void DSP_SendAIBuffer(unsigned int address, unsigned int num_samples)
{
	if (!soundStream)
		return;

	CMixer* pMixer = soundStream->GetMixer();

	if (pMixer && address)
	{
		short* samples = (short*)Memory_Get_Pointer(address);
		// Internal sample rate is always 32khz
		pMixer->PushSamples(samples, num_samples);

		// FIXME: Write the audio to a file
		//if (log_ai)
		//	g_wave_writer.AddStereoSamples(samples, 8);
	}

	soundStream->Update();
}

void DSP_ClearAudioBuffer(bool mute)
{
	if (soundStream)
		soundStream->Clear(mute);
}
