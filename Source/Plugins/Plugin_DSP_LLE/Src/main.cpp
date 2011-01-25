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
#include "CommonPaths.h"
#include "Atomic.h"
#include "CommonTypes.h"
#include "LogManager.h"
#include "Thread.h"
#include "ChunkFile.h"
#include "IniFile.h"

#include "Globals.h" // Local
#include "DSPInterpreter.h"
#include "DSPHWInterface.h"
#include "disassemble.h"
#include "DSPSymbols.h"
#include "Config.h"

#include "AudioCommon.h"
#include "Mixer.h"

#include "DSPTables.h"
#include "DSPCore.h"

#if defined(HAVE_WX) && HAVE_WX
#include "DSPConfigDlgLLE.h"
DSPConfigDialogLLE* m_ConfigFrame = NULL;
#include "Debugger/DSPDebugWindow.h"
#endif

PLUGIN_GLOBALS* globals = NULL;
DSPInitialize g_dspInitialize;
Common::Thread *g_hDSPThread = NULL;
SoundStream *soundStream = NULL;
bool g_InitMixer = false;

bool bIsRunning = false;
volatile u32 cycle_count = 0;

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

void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_DSP;

#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE Plugin (DebugFast)");
#elif defined _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin DSP-LLE Plugin (Debug)");
#else
	sprintf(_PluginInfo->Name, _trans("Dolphin DSP-LLE Plugin"));
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	m_ConfigFrame = new DSPConfigDialogLLE((wxWindow *)_hParent);

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

void DoState(unsigned char **ptr, int mode)
{
	PointerWrap p(ptr, mode);
	p.Do(g_InitMixer);

	p.Do(g_dsp.r);
	p.Do(g_dsp.pc);
#if PROFILE
	p.Do(g_dsp.err_pc);
#endif
	p.Do(g_dsp.cr);
	p.Do(g_dsp.reg_stack_ptr);
	p.Do(g_dsp.exceptions);
	p.Do(g_dsp.external_interrupt_waiting);
	for (int i = 0; i < 4; i++) {
		p.Do(g_dsp.reg_stack[i]);
	}
	p.Do(g_dsp.iram_crc);
	p.Do(g_dsp.step_counter);
	p.Do(g_dsp.ifx_regs);
	p.Do(g_dsp.mbox[0]);
	p.Do(g_dsp.mbox[1]);
	UnWriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	p.DoArray(g_dsp.iram, DSP_IRAM_SIZE);
	WriteProtectMemory(g_dsp.iram, DSP_IRAM_BYTE_SIZE, false);
	p.DoArray(g_dsp.dram, DSP_DRAM_SIZE);
	p.Do(cyclesLeft);
	p.Do(cycle_count);
}

void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	DSP_ClearAudioBuffer((newState == PLUGIN_EMUSTATE_PLAY) ? false : true);
}

void *DllDebugger(void *_hParent, bool Show)
{
#if defined(HAVE_WX) && HAVE_WX
	m_DebuggerFrame = new DSPDebuggerLLE((wxWindow *)_hParent);
	return (void *)m_DebuggerFrame;
#else
	return NULL;
#endif
}


// Regular thread
THREAD_RETURN dsp_thread(void* lpParameter)
{
	while (bIsRunning)
	{
		int cycles = (int)cycle_count;
		if (cycles > 0) {
			if (jit) 
				DSPCore_RunCycles(cycles);
			else
				DSPInterpreter::RunCycles(cycles);

			Common::AtomicAdd(cycle_count, -cycles);
		}
		// yield?
	}
	return 0;
}

void DSP_DebugBreak()
{
#if defined(HAVE_WX) && HAVE_WX
	// if (m_DebuggerFrame)
	//  	m_DebuggerFrame->DebugBreak();
#endif
}

void Initialize(void *init)
{
	g_InitMixer = false;
	bool bCanWork = true;
	char irom_file[MAX_PATH];
	char coef_file[MAX_PATH];
	g_dspInitialize = *(DSPInitialize*)init;

	g_Config.Load();

	snprintf(irom_file, MAX_PATH, "%s%s",
		File::GetSysDirectory().c_str(), GC_SYS_DIR DIR_SEP DSP_IROM);
	if (!File::Exists(irom_file))
		snprintf(irom_file, MAX_PATH, "%s%s",
			File::GetUserPath(D_GCUSER_IDX), DIR_SEP DSP_IROM);
	snprintf(coef_file, MAX_PATH, "%s%s",
		File::GetSysDirectory().c_str(), GC_SYS_DIR DIR_SEP DSP_COEF);
	if (!File::Exists(coef_file))
		snprintf(coef_file, MAX_PATH, "%s%s",
			File::GetUserPath(D_GCUSER_IDX), DIR_SEP DSP_COEF);
	bCanWork = DSPCore_Init(irom_file, coef_file, AudioCommon::UseJIT());

	g_dsp.cpu_ram = g_dspInitialize.pGetMemoryPointer(0);
	DSPCore_Reset();

	if (!bCanWork)
	{
		DSPCore_Shutdown();
		// No way to shutdown Core from here? Hardcore shutdown!
		exit(EXIT_FAILURE);
		return;
	}

	bIsRunning = true;

	InitInstructionTable();

	if (g_dspInitialize.bOnThread)
	{
		g_hDSPThread = new Common::Thread(dsp_thread, NULL);
	}

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
	UDSPControl Temp(_uFlag);
	if (!g_InitMixer)
	{
		if (!Temp.DSPHalt && Temp.DSPInit)
		{
			unsigned int AISampleRate, DACSampleRate;
			g_dspInitialize.pGetSampleRate(AISampleRate, DACSampleRate);
			soundStream = AudioCommon::InitSoundStream(new CMixer(AISampleRate, DACSampleRate)); 
			if(!soundStream) PanicAlert("Error starting up sound stream");
			// Mixer is initialized
			g_InitMixer = true;
		}
	}
	DSPInterpreter::WriteCR(_uFlag);

	// Check if the CPU has set an external interrupt (CR_EXTERNAL_INT)
	// and immediately process it, if it has.
	if (_uFlag & 2)
	{
		if (!g_dspInitialize.bOnThread)
		{
			DSPCore_CheckExternalInterrupt();
			DSPCore_CheckExceptions();
		}
		else
		{
			DSPCore_SetExternalInterrupt(true);
		}

	}

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
	unsigned int dsp_cycles = cycles / 6;  //(jit?20:6);

// Sound stream update job has been handled by AudioDMA routine, which is more efficient
/*
	// This gets called VERY OFTEN. The soundstream update might be expensive so only do it 200 times per second or something.
	int cycles_between_ss_update;

	if (g_dspInitialize.bWii)
		cycles_between_ss_update = 121500000 / 200;
	else
		cycles_between_ss_update = 81000000 / 200;
	
	cycle_count += cycles;
	if (cycle_count > cycles_between_ss_update)
	{
		while (cycle_count > cycles_between_ss_update)
			cycle_count -= cycles_between_ss_update;
		soundStream->Update();
	}
*/
	// If we're not on a thread, run cycles here.
	if (!g_dspInitialize.bOnThread)
	{
		// ~1/6th as many cycles as the period PPC-side.
		DSPCore_RunCycles(dsp_cycles);
	}
	else
	{
		// Wait for dsp thread to catch up reasonably. Note: this logic should be thought through.
		while (cycle_count > dsp_cycles)
			;
		Common::AtomicAdd(cycle_count, dsp_cycles);
	}
}

void DSP_SendAIBuffer(unsigned int address, unsigned int num_samples)
{
	if (!soundStream)
		return;

	CMixer *pMixer = soundStream->GetMixer();

	if (pMixer != 0 && address != 0)
	{
		short *samples = (short *)Memory_Get_Pointer(address);
		pMixer->PushSamples(samples, num_samples);
	}

	soundStream->Update();
}

void DSP_ClearAudioBuffer(bool mute)
{
	if (soundStream)
		soundStream->Clear(mute);
}

