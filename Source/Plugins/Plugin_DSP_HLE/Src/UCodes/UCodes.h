// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _UCODES_H
#define _UCODES_H

#include "Common.h"

#define UCODE_ROM                   0x0000000
#define UCODE_INIT_AUDIO_SYSTEM     0x0000001

class CMailHandler;

class IUCode
{
public:
	IUCode(CMailHandler& _rMailHandler)
		: m_rMailHandler(_rMailHandler)
	{}

	virtual ~IUCode()
	{}

	virtual void HandleMail(u32 _uMail) = 0;

	// Cycles are out of the 81/121mhz the DSP runs at.
	virtual void Update(int cycles) = 0;
	virtual void MixAdd(short* buffer, int size) {}

protected:
	CMailHandler& m_rMailHandler;
};

extern IUCode* UCodeFactory(u32 _CRC, CMailHandler& _rMailHandler);

#endif
