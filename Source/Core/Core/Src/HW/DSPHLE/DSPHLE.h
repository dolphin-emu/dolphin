// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

	virtual bool Initialize(void *hWnd, bool bWii, bool bDSPThread) override;
	virtual void Shutdown() override;
	virtual bool IsLLE() override { return false ; }

	virtual void DoState(PointerWrap &p) override;
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;

	virtual void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short) override;
	virtual void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short) override;
	virtual unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox) override;
	virtual unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox) override;
	virtual unsigned short DSP_ReadControlRegister() override;
	virtual unsigned short DSP_WriteControlRegister(unsigned short) override;
	virtual void DSP_SendAIBuffer(unsigned int address, unsigned int num_samples) override;
	virtual void DSP_Update(int cycles) override;
	virtual void DSP_StopSoundStream() override;
	virtual void DSP_ClearAudioBuffer(bool mute) override;
	virtual u32 DSP_UpdateRate() override;

	CMailHandler& AccessMailHandler() { return m_MailHandler; }

	// Formerly DSPHandler
	IUCode *GetUCode();
	void SetUCode(u32 _crc);
	void SwapUCode(u32 _crc);

private:
	void SendMailToDSP(u32 _uMail);
	void InitMixer();

	// Declarations and definitions
	bool m_bWii;

	bool m_InitMixer;

	// Fake mailbox utility
	struct DSPState
	{
		u32 CPUMailbox;
		u32 DSPMailbox;

		void Reset()
		{
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
