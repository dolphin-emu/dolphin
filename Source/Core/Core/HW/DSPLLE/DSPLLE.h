// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "Common/Flag.h"
#include "Core/DSPEmulator.h"

class PointerWrap;

class DSPLLE : public DSPEmulator
{
public:
	DSPLLE();

	virtual bool Initialize(bool bWii, bool bDSPThread) override;
	virtual void Shutdown() override;
	virtual bool IsLLE() override { return true; }

	virtual void DoState(PointerWrap &p) override;
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) override;

	virtual void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short) override;
	virtual void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short) override;
	virtual unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox) override;
	virtual unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox) override;
	virtual unsigned short DSP_ReadControlRegister() override;
	virtual unsigned short DSP_WriteControlRegister(unsigned short) override;
	virtual void DSP_Update(int cycles) override;
	virtual void DSP_StopSoundStream() override;
	virtual u32 DSP_UpdateRate() override;

private:
	static void DSPThread(DSPLLE* lpParameter);

	std::thread m_hDSPThread;
	std::mutex m_csDSPThreadActive;
	bool m_bWii;
	bool m_bDSPThread;
	Common::Flag m_bIsRunning;
	std::atomic<u32> m_cycle_count;
};
