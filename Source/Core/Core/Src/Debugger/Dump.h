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
// uncompress the dumps from costis GC-Debugger tool
//
//
#ifndef _DUMP_H
#define _DUMP_H

#include "Common.h"

class CDump
{
public:

	CDump(const char* _szFilename);
	~CDump();
	
	int GetNumberOfSteps();
	u32 GetGPR(int _step, int _gpr);
	u32 GetPC(int _step);

private:
	enum
	{
		OFFSET_GPR		= 0x4,
		OFFSET_PC		= 0x194,
		STRUCTUR_SIZE	= 0x2BC
	};

	u8 *m_pData;

	bool m_bInit;
	size_t m_size;

	u32 Read32(u32 _pos);
};

#endif
