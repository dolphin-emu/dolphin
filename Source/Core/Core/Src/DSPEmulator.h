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

#ifndef _DSPEMULATOR_H_
#define _DSPEMULATOR_H_

#include "ChunkFile.h"
#include "SoundStream.h"

class DSPEmulator
{
public:
	virtual ~DSPEmulator() {}

	virtual bool IsLLE() = 0;

	virtual bool Initialize(void *hWnd, bool bWii, bool bDSPThread) = 0;
	virtual void Shutdown() = 0;

	virtual void DoState(PointerWrap &p) = 0;
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) = 0;

	virtual void DSP_WriteMailBoxHigh(bool _CPUMailbox, unsigned short) = 0;
	virtual void DSP_WriteMailBoxLow(bool _CPUMailbox, unsigned short) = 0;
	virtual unsigned short DSP_ReadMailBoxHigh(bool _CPUMailbox) = 0;
	virtual unsigned short DSP_ReadMailBoxLow(bool _CPUMailbox) = 0;
	virtual unsigned short DSP_ReadControlRegister() = 0;
	virtual unsigned short DSP_WriteControlRegister(unsigned short) = 0;
	virtual void DSP_SendAIBuffer(unsigned int address, unsigned int num_samples) = 0;
	virtual void DSP_Update(int cycles) = 0;
	virtual void DSP_StopSoundStream() = 0;
	virtual void DSP_ClearAudioBuffer(bool mute) = 0;

protected:
	SoundStream *soundStream;
};

DSPEmulator *CreateDSPEmulator(bool LLE);

#endif // _DSPEMULATOR_H_
