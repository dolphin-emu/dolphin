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
#endif

#include "ChunkFile.h"
#include "HLEMixer.h"
#include "DSPHandler.h"
#include "Config.h"
#include "Setup.h"
#include "StringUtil.h"
#include "LogManager.h"
#include "IniFile.h"


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

wxLocale *InitLanguageSupport()
{
	wxLocale *m_locale;
	unsigned int language = 0;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	ini.Get("Interface", "Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if(wxLocale::IsAvailable(language))
	{
		m_locale = new wxLocale(language);

		m_locale->AddCatalogLookupPathPrefix(wxT("Languages"));

		m_locale->AddCatalog(wxT("dolphin-emu"));

		if(!m_locale->IsOk())
		{
			PanicAlertT("Error loading selected language. Falling back to system default.");
			delete m_locale;
			m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
		}
	}
	else
	{
		PanicAlertT("The selected language is not supported by your system. Falling back to system default.");
		m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
	return m_locale;
}

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	static wxLocale *m_locale;
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			wxSetInstance((HINSTANCE)hinstDLL);
			wxInitialize();
			m_locale = InitLanguageSupport();
		}
		break;

	case DLL_PROCESS_DETACH:
		wxUninitialize();
		delete m_locale;
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}
#endif

void *DllDebugger(void *_hParent, bool Show)
{
	return NULL;
}


void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_DSP;
#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin DSP-HLE Plugin (DebugFast)");
#elif defined _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin DSP-HLE Plugin (Debug)");
#else
	sprintf(_PluginInfo->Name, _trans("Dolphin DSP-HLE Plugin"));
#endif
}


void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager*)globals->logManager);
}

void DllConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	// Load config settings
	g_Config.Load();

	m_ConfigFrame = new DSPConfigDialogHLE((wxWindow *)_hParent);

	// add backends
	std::vector<std::string> backends = AudioCommon::GetSoundBackends();
	
	for (std::vector<std::string>::const_iterator iter = backends.begin(); 
		 iter != backends.end(); ++iter)
	{
		m_ConfigFrame->AddBackend((*iter).c_str());
	}

	m_ConfigFrame->ShowModal();
	m_ConfigFrame->Destroy();
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
			unsigned int AISampleRate, DACSampleRate, BackendSampleRate;
			g_dspInitialize.pGetSampleRate(AISampleRate, DACSampleRate);
			std::string frequency = ac_Config.sFrequency;
			if (frequency == "48,000 Hz")
				BackendSampleRate = 48000;
			else
				BackendSampleRate = 32000;

			soundStream = AudioCommon::InitSoundStream(new HLEMixer(AISampleRate, DACSampleRate, BackendSampleRate));
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
	// ~1/6th as many cycles as the period PPC-side.
	CDSPHandler::GetInstance().Update(cycles / 6);
}

// The reason that we don't disable this entire
// function when Other Audio is disabled is that then we can't turn it back on
// again once the game has started.
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
