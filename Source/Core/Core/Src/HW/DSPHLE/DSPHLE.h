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

#ifndef _DSPHLE_H
#define _DSPHLE_H

#include "AudioCommon.h"
#include "SoundStream.h"
#include "MailHandler.h"
#include "../../DSPEmulator.h"

class IUCode;

class DSPHLE : public DSPEmulator {
public:
	DSPHLE();

	virtual bool Initialize(void *hWnd, bool bWii, bool bDSPThread);
	virtual void Shutdown();
	virtual bool IsLLE() { return false; }

	virtual void DoState(PointerWrap &p);
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true);

	virtual void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short);
	virtual void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short);
	virtual unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox);
	virtual unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox);
	virtual unsigned short DSP_ReadControlRegister();
	virtual unsigned short DSP_WriteControlRegister(unsigned short);
	virtual void DSP_SendAIBuffer(unsigned int address, unsigned int num_samples);
	virtual void DSP_Update(int cycles);
	virtual void DSP_StopSoundStream();
	virtual void DSP_ClearAudioBuffer(bool mute);

	CMailHandler& AccessMailHandler() { return m_MailHandler; }

	// Formerly DSPHandler
	IUCode *GetUCode();
	void SetUCode(u32 _crc);
	void SwapUCode(u32 _crc);

private:
	void SendMailToDSP(u32 _uMail);
	void InitMixer();

	// Declarations and definitions
	void *m_hWnd;
	bool m_bWii;
	
	bool m_InitMixer;

	// Fake mailbox utility
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
	DSPState m_dspState;

	IUCode* m_pUCode;
	IUCode* m_lastUCode;

	UDSPControl m_DSPControl;
	CMailHandler m_MailHandler;

	bool m_bHalt;
	bool m_bAssertInt;
};

#endif  // _DSPHLE_H
