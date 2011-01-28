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

#include "DSPHLEGlobals.h" // Local

#include "ChunkFile.h"
#include "IniFile.h"
#include "HLEMixer.h"
#include "DSPHandler.h"
#include "Config.h"
#include "Setup.h"
#include "StringUtil.h"
#include "LogManager.h"
#include "IniFile.h"
#include "DSPHLE.h"
#include "../AudioInterface.h"

DSPHLE::DSPHLE() {
	g_InitMixer = false;
	soundStream = NULL;
}

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

void DSPHLE::Initialize(void *hWnd, bool bWii, bool bDSPThread)
{
	this->hWnd = hWnd;
	this->bWii = bWii;

	g_InitMixer = false;
	g_dspState.Reset();

	CDSPHandler::CreateInstance(bWii);
}

void DSPHLE::DSP_StopSoundStream()
{
}

void DSPHLE::Shutdown()
{
	AudioCommon::ShutdownSoundStream();

	// Delete the UCodes
	CDSPHandler::Destroy();
}

void DSPHLE::DoState(PointerWrap &p)
{
	p.Do(g_InitMixer);
	CDSPHandler::GetInstance().GetUCode()->DoState(p);
}

void DSPHLE::EmuStateChange(PLUGIN_EMUSTATE newState)
{
	DSP_ClearAudioBuffer((newState == PLUGIN_EMUSTATE_PLAY) ? false : true);
}

// Mailbox fuctions
unsigned short DSPHLE::DSP_ReadMailBoxHigh(bool _CPUMailbox)
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

unsigned short DSPHLE::DSP_ReadMailBoxLow(bool _CPUMailbox)
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

void DSPHLE::DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short _Value)
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

void DSPHLE::DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short _Value)
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
unsigned short DSPHLE::DSP_WriteControlRegister(unsigned short _Value)
{
	UDSPControl Temp(_Value);
	if (!g_InitMixer)
	{
		if (!Temp.DSPHalt && Temp.DSPInit)
		{
			unsigned int AISampleRate, DACSampleRate, BackendSampleRate;
			AudioInterface::Callback_GetSampleRate(AISampleRate, DACSampleRate);
			std::string frequency = ac_Config.sFrequency;
			if (frequency == "48,000 Hz")
				BackendSampleRate = 48000;
			else
				BackendSampleRate = 32000;

			soundStream = AudioCommon::InitSoundStream(
				new HLEMixer(AISampleRate, DACSampleRate, BackendSampleRate), hWnd);
			if(!soundStream) PanicAlert("Error starting up sound stream");
			// Mixer is initialized
			g_InitMixer = true;
		}
	}
	return CDSPHandler::GetInstance().WriteControlRegister(_Value);
}

unsigned short DSPHLE::DSP_ReadControlRegister()
{
	return CDSPHandler::GetInstance().ReadControlRegister();
}

void DSPHLE::DSP_Update(int cycles)
{
	// This is called OFTEN - better not do anything expensive!
	// ~1/6th as many cycles as the period PPC-side.
	CDSPHandler::GetInstance().Update(cycles / 6);
}

// The reason that we don't disable this entire
// function when Other Audio is disabled is that then we can't turn it back on
// again once the game has started.
void DSPHLE::DSP_SendAIBuffer(unsigned int address, unsigned int num_samples)
{
	if (!soundStream)
		return;

	CMixer* pMixer = soundStream->GetMixer();

	if (pMixer && address)
	{
		short* samples = (short*)HLEMemory_Get_Pointer(address);
		// Internal sample rate is always 32khz
		pMixer->PushSamples(samples, num_samples);

		// FIXME: Write the audio to a file
		//if (log_ai)
		//	g_wave_writer.AddStereoSamples(samples, 8);
	}

	soundStream->Update();
}

void DSPHLE::DSP_ClearAudioBuffer(bool mute)
{
	if (soundStream)
		soundStream->Clear(mute);
}


#define HLE_CONFIG_FILE "DSP.ini"

void DSPHLE_LoadConfig()
{
	// first load defaults
	IniFile file;
	file.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + HLE_CONFIG_FILE).c_str());
	ac_Config.Load(file);
}

void DSPHLE_SaveConfig()
{
	IniFile file;
	file.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + HLE_CONFIG_FILE).c_str());
	ac_Config.Set(file);
	file.Save((std::string(File::GetUserPath(D_CONFIG_IDX)) + HLE_CONFIG_FILE).c_str());
}
