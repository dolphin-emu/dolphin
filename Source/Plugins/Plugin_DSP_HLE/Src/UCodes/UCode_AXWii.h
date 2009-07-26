// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _UCODE_AXWII
#define _UCODE_AXWII

#include "UCode_AXStructs.h"

#define NUMBER_OF_PBS 128

class CUCode_AXWii : public IUCode
{
public:
	CUCode_AXWii(CMailHandler& _rMailHandler, u32 _CRC);
	virtual ~CUCode_AXWii();

	void HandleMail(u32 _uMail);
	void MixAdd(short* _pBuffer, int _iSize);
	template<class ParamBlockType>
	//void Logging(short* _pBuffer, int _iSize, int a, bool Wii, ParamBlockType &PBs, int numberOfPBs);
	void MixAdd_(short* _pBuffer, int _iSize, ParamBlockType &PBs);	
	void Update(int cycles);

	// The logging function for the debugger
	void Logging(short* _pBuffer, int _iSize, int a);
	CUCode_AX * lCUCode_AX; // we need the logging functions in there

private:
	enum
	{
		MAIL_AX_ALIST = 0xBABE0000,
	};

	// PBs
	u32 m_addressPBs;
	u32 _CRC;

	int *templbuffer;
	int *temprbuffer;

	// ax task message handler
	bool AXTask(u32& _uMail);
	void SaveLog(const char* _fmt, ...);
	void SendMail(u32 _uMail);
};

#endif  // _UCODE_AXWII
