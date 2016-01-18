// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

class DSPEmulator
{
public:
	virtual ~DSPEmulator() {}

	virtual bool IsLLE() = 0;

	virtual bool Initialize(bool bWii, bool bDSPThread) = 0;
	virtual void Shutdown() = 0;

	virtual void DoState(PointerWrap &p) = 0;
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) = 0;

	virtual void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short) = 0;
	virtual void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short) = 0;
	virtual unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox) = 0;
	virtual unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox) = 0;
	virtual unsigned short DSP_ReadControlRegister() = 0;
	virtual unsigned short DSP_WriteControlRegister(unsigned short) = 0;
	virtual void DSP_Update(int cycles) = 0;
	virtual void DSP_StopSoundStream() = 0;
	virtual u32 DSP_UpdateRate() = 0;
};

std::unique_ptr<DSPEmulator> CreateDSPEmulator(bool hle);
