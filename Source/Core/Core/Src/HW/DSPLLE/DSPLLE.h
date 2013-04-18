// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSPLLE_H
#define _DSPLLE_H

#include "Thread.h"
#include "SoundStream.h"
#include "DSPLLEGlobals.h" // Local
#include "../../DSPEmulator.h"

class DSPLLE : public DSPEmulator {
public:
	DSPLLE();

	virtual bool Initialize(void *hWnd, bool bWii, bool bDSPThread);
	virtual void Shutdown();
	virtual bool IsLLE() { return true; }

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
	virtual u32 DSP_UpdateRate();

private:
	static void dsp_thread(DSPLLE* lpParameter);
	void InitMixer();

	std::thread m_hDSPThread;
	std::mutex m_csDSPThreadActive;
	bool m_InitMixer;
	void *m_hWnd;
	bool m_bWii;
	bool m_bDSPThread;
	bool m_bIsRunning;
	volatile u32 m_cycle_count;
};

#endif  // _DSPLLE_H
