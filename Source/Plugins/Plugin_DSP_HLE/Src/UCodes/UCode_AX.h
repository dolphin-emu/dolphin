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

#include <iostream>
#include "UCode_AXStructs.h"

enum
{
	NUMBER_OF_PBS = 64
};

class CUCode_AX	: public IUCode
{
public:
	CUCode_AX(CMailHandler& _rMailHandler);
	virtual ~CUCode_AX();

	void HandleMail(u32 _uMail);
	void MixAdd(short* _pBuffer, int _iSize);
	void Update();

	// Logging
	void Logging(short* _pBuffer, int _iSize, int a);
	void SaveLog_(bool Wii, const char* _fmt, ...);
	void SaveMail(bool Wii, u32 _uMail);
	void SaveLogFile(std::string f, int resizeTo, bool type, bool Wii);
	std::string TmpMailLog;
	int saveNext;

	// PBs
	u32 m_addressPBs;

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

int ReadOutPBs(u32 pbs_address, AXParamBlock* _pPBs, int _num);
void WriteBackPBs(u32 pbs_address, AXParamBlock* _pPBs, int _num);


#endif  // _UCODE_AX
