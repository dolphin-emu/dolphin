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

#ifndef _UCODE_AX
#define _UCODE_AX

#include <iostream>
#include "pluginspecs_dsp.h"
#include "UCode_AXStructs.h"

enum
{
	NUMBER_OF_PBS = 128
};

class CUCode_AX	: public IUCode
{
public:
	CUCode_AX(CMailHandler& _rMailHandler);
	virtual ~CUCode_AX();

	void HandleMail(u32 _uMail);
	void MixAdd(short* _pBuffer, int _iSize);
	void Update(int cycles);

	// Logging
	//template<class ParamBlockType>
	//void Logging(short* _pBuffer, int _iSize, int a, bool Wii, ParamBlockType &PBs, int numberOfPBs);
	void Logging(short* _pBuffer, int _iSize, int a, bool Wii);
	void SaveLog_(bool Wii, const char* _fmt, va_list ap);
	void SaveMail(bool Wii, u32 _uMail);
	void SaveLogFile(std::string f, int resizeTo, bool type, bool Wii);
	std::string TmpMailLog;
	int saveNext;

	// PBs
	u8 numPBaddr;
	u32 PBaddr[8]; //2 needed for MP2
	u32 m_addressPBs;
	u32 _CRC;

private:
	enum
	{
		MAIL_AX_ALIST = 0xBABE0000,
		AXLIST_STUDIOADDR = 0x0000,
		AXLIST_PBADDR  = 0x0002,
		AXLIST_SBUFFER = 0x0007,
		AXLIST_COMPRESSORTABLE = 0x000A,
		AXLIST_END = 0x000F
	};

	int *templbuffer;
	int *temprbuffer;

	// ax task message handler
	bool AXTask(u32& _uMail);
	void SaveLog(const char* _fmt, ...);
	void SendMail(u32 _uMail);
};

#endif  // _UCODE_AX
