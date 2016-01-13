// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/DSPEmulator.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/MailHandler.h"

class PointerWrap;
class UCodeInterface;

class DSPHLE : public DSPEmulator {
public:
	DSPHLE();

	bool Initialize(bool bWii, bool bDSPThread) override;
	void Shutdown() override;
	bool IsLLE() override { return false ; }

	void DoState(PointerWrap &p) override;
	void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;

	void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short) override;
	void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short) override;
	unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox) override;
	unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox) override;
	unsigned short DSP_ReadControlRegister() override;
	unsigned short DSP_WriteControlRegister(unsigned short) override;
	void DSP_Update(int cycles) override;
	void DSP_StopSoundStream() override;
	u32 DSP_UpdateRate() override;

	CMailHandler& AccessMailHandler() { return m_MailHandler; }

	// Formerly DSPHandler
	UCodeInterface *GetUCode();
	void SetUCode(u32 _crc);
	void SwapUCode(u32 _crc);

private:
	void SendMailToDSP(u32 _uMail);

	// Declarations and definitions
	bool m_bWii;

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

	UCodeInterface* m_pUCode;
	UCodeInterface* m_lastUCode;

	DSP::UDSPControl m_DSPControl;
	CMailHandler m_MailHandler;

	bool m_bHalt;
	bool m_bAssertInt;
};
