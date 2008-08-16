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

#ifndef _UCODE_AX
#define _UCODE_AX

#include "UCode_AXStructs.h"

class CUCode_AX	: public IUCode
{
public:
	CUCode_AX(CMailHandler& _rMailHandler, bool wii = false);
	virtual ~CUCode_AX();

	void HandleMail(u32 _uMail);
	void MixAdd(short* _pBuffer, int _iSize);
	void Update();

private:

	enum
	{
		NUMBER_OF_PBS = 64
	};

	enum
	{
		MAIL_AX_ALIST = 0xBABE0000,
		AXLIST_STUDIOADDR = 0x0000,
		AXLIST_PBADDR  = 0x0002,
		AXLIST_SBUFFER = 0x0007,
		AXLIST_COMPRESSORTABLE = 0x000A,
		AXLIST_END = 0x000F
	};

	// PBs
	u32 m_addressPBs;

	int *templbuffer;
	int *temprbuffer;

	bool wii_mode;

	// ax task message handler
	bool AXTask(u32& _uMail);

	void SendMail(u32 _uMail);
	int ReadOutPBs(AXParamBlock *_pPBs, int _num);
	void WriteBackPBs(AXParamBlock *_pPBs, int _num);
	s16 ADPCM_Step(AXParamBlock& pb, u32& samplePos, u32 newSamplePos, u16 frac);
};

#endif  // _UCODE_AX
