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

#include "ChunkFile.h"
#include "IniFile.h"
#include "HLEMixer.h"
#include "StringUtil.h"
#include "LogManager.h"
#include "IniFile.h"
#include "DSPHLE.h"
#include "UCodes/UCodes.h"
#include "../AudioInterface.h"

DSPHLE::DSPHLE() {
	m_InitMixer = false;
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

bool DSPHLE::Initialize(void *hWnd, bool bWii, bool bDSPThread)
{
	m_hWnd = hWnd;
	m_bWii = bWii;
	m_pUCode = NULL;
	m_lastUCode = NULL;
	m_bHalt = false;
	m_bAssertInt = false;

	SetUCode(UCODE_ROM);
	m_DSPControl.DSPHalt = 1;
	m_DSPControl.DSPInit = 1;

	m_InitMixer = false;
	m_dspState.Reset();

	return true;
}

void DSPHLE::DSP_StopSoundStream()
{
}

void DSPHLE::Shutdown()
{
	AudioCommon::ShutdownSoundStream();
}

void DSPHLE::DSP_Update(int cycles)
{
	// This is called OFTEN - better not do anything expensive!
	// ~1/6th as many cycles as the period PPC-side.
	if (m_pUCode != NULL)
		m_pUCode->Update(cycles / 6);
}

void DSPHLE::SendMailToDSP(u32 _uMail)
{
	if (m_pUCode != NULL) {
		DEBUG_LOG(DSP_MAIL, "CPU writes 0x%08x", _uMail);
		m_pUCode->HandleMail(_uMail);
	}
}

IUCode* DSPHLE::GetUCode()
{
	return m_pUCode;
}

void DSPHLE::SetUCode(u32 _crc)
{
	delete m_pUCode;

	m_pUCode = NULL;
	m_MailHandler.Clear();
	m_pUCode = UCodeFactory(_crc, this, m_bWii);
}

// TODO do it better?
// Assumes that every odd call to this func is by the persistent ucode.
// Even callers are deleted.
void DSPHLE::SwapUCode(u32 _crc)
{
	m_MailHandler.Clear();

	if (m_lastUCode == NULL)
	{
		m_lastUCode = m_pUCode;
		m_pUCode = UCodeFactory(_crc, this, m_bWii);
	}
	else
	{
		delete m_pUCode;
		m_pUCode = m_lastUCode;
		m_lastUCode = NULL;
	}
}

void DSPHLE::DoState(PointerWrap &p)
{
	bool prevInitMixer = m_InitMixer;
	p.Do(m_InitMixer);
	if (prevInitMixer != m_InitMixer && p.GetMode() == PointerWrap::MODE_READ)
	{
		if (m_InitMixer)
		{
			InitMixer();
			AudioCommon::PauseAndLock(true);
		}
		else
		{
			AudioCommon::PauseAndLock(false);
			soundStream->Stop();
			delete soundStream;
			soundStream = NULL;
		}
	}

	p.Do(m_DSPControl);
	p.Do(m_dspState);

	int ucode_crc = IUCode::GetCRC(m_pUCode);
	int ucode_crc_beforeLoad = ucode_crc;
	int lastucode_crc = IUCode::GetCRC(m_lastUCode);
	int lastucode_crc_beforeLoad = lastucode_crc;

	p.Do(ucode_crc);
	p.Do(lastucode_crc);

	// if a different type of ucode was being used when the savestate was created,
	// we have to reconstruct the old type of ucode so that we have a valid thing to call DoState on.
	IUCode*     ucode =     (ucode_crc ==     ucode_crc_beforeLoad) ?    m_pUCode : UCodeFactory(    ucode_crc, this, m_bWii);
	IUCode* lastucode = (lastucode_crc != lastucode_crc_beforeLoad) ? m_lastUCode : UCodeFactory(lastucode_crc, this, m_bWii);

	if (ucode)
		ucode->DoState(p);
	if (lastucode)
		lastucode->DoState(p);

	// if a different type of ucode was being used when the savestate was created,
	// discard it if we're not loading, otherwise discard the old one and keep the new one.
	if (ucode != m_pUCode)
	{
		if (p.GetMode() != PointerWrap::MODE_READ)
			delete ucode;
		else
		{
			delete m_pUCode;
			m_pUCode = ucode;
		}
	}
	if (lastucode != m_lastUCode)
	{
		if (p.GetMode() != PointerWrap::MODE_READ)
			delete lastucode;
		else
		{
			delete m_lastUCode;
			m_lastUCode = lastucode;
		}
	}

	m_MailHandler.DoState(p);
}

// Mailbox fuctions
unsigned short DSPHLE::DSP_ReadMailBoxHigh(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return (m_dspState.CPUMailbox >> 16) & 0xFFFF;
	}
	else
	{
		return AccessMailHandler().ReadDSPMailboxHigh();
	}
}

unsigned short DSPHLE::DSP_ReadMailBoxLow(bool _CPUMailbox)
{
	if (_CPUMailbox)
	{
		return m_dspState.CPUMailbox & 0xFFFF;
	}
	else
	{
		return AccessMailHandler().ReadDSPMailboxLow();
	}
}

void DSPHLE::DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short _Value)
{
	if (_CPUMailbox)
	{
		m_dspState.CPUMailbox = (m_dspState.CPUMailbox & 0xFFFF) | (_Value << 16);
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
		m_dspState.CPUMailbox = (m_dspState.CPUMailbox & 0xFFFF0000) | _Value;
		SendMailToDSP(m_dspState.CPUMailbox);
		// Mail sent so clear MSB to show that it is progressed
		m_dspState.CPUMailbox &= 0x7FFFFFFF; 
	}
	else
	{
		PanicAlert("CPU can't write %08x to DSP mailbox", _Value);
	}
}

void DSPHLE::InitMixer()
{
	unsigned int AISampleRate, DACSampleRate;
	AudioInterface::Callback_GetSampleRate(AISampleRate, DACSampleRate);
	delete soundStream;
	soundStream = AudioCommon::InitSoundStream(new HLEMixer(this, AISampleRate, DACSampleRate, ac_Config.iFrequency), m_hWnd);
	if(!soundStream) PanicAlert("Error starting up sound stream");
	// Mixer is initialized
	m_InitMixer = true;
}

// Other DSP fuctions
u16 DSPHLE::DSP_WriteControlRegister(unsigned short _Value)
{
	UDSPControl Temp(_Value);
	if (!m_InitMixer)
	{
		if (!Temp.DSPHalt && Temp.DSPInit)
		{
			InitMixer();
		}
	}

	if (Temp.DSPReset)
	{
		SetUCode(UCODE_ROM);
		Temp.DSPReset = 0;
	}
	if (Temp.DSPInit == 0)
	{
		// copy 128 byte from ARAM 0x000000 to IMEM
		SetUCode(UCODE_INIT_AUDIO_SYSTEM);
		Temp.DSPInitCode = 0;
	}

	m_DSPControl.Hex = Temp.Hex;
	return m_DSPControl.Hex;
}

u16 DSPHLE::DSP_ReadControlRegister()
{
	return m_DSPControl.Hex;
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
		pMixer->PushSamples(samples, num_samples);
	}

	soundStream->Update();
}

void DSPHLE::DSP_ClearAudioBuffer(bool mute)
{
	if (soundStream)
		soundStream->Clear(mute);
}

void DSPHLE::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock || unpauseOnUnlock)
		DSP_ClearAudioBuffer(doLock); 
}
