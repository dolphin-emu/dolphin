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
#include <stdio.h>

#include "Common.h"
#include "Dump.h"

CDump::CDump(const char* _szFilename) :
	m_pData(NULL),
	m_bInit(false)
{
	FILE* pStream = fopen(_szFilename, "rb");
	if (pStream != NULL)
	{
		fseek(pStream, 0, SEEK_END);
		m_size = (size_t) ftell(pStream);
		fseek(pStream, 0, SEEK_SET);

		m_pData = new u8[m_size];

		fread(m_pData, m_size, 1, pStream);

		fclose(pStream);
	}
}

CDump::~CDump(void)
{
	if (m_pData != NULL)
	{
		delete [] m_pData;
		m_pData = NULL;
	}
}

int
CDump::GetNumberOfSteps(void)
{
	return (int)(m_size / STRUCTUR_SIZE);
}

u32 
CDump::GetGPR(int _step, int _gpr)
{
	u32 offset = _step * STRUCTUR_SIZE;

	if (offset >= m_size)
		return -1;

	return Read32(offset + OFFSET_GPR + (_gpr * 4));
}

u32 
CDump::GetPC(int _step)
{
	u32 offset = _step * STRUCTUR_SIZE;

	if (offset >= m_size)
		return -1;

	return Read32(offset + OFFSET_PC);
}

u32 
CDump::Read32(u32 _pos)
{
	u32 result = (m_pData[_pos+0] << 24) |
				  (m_pData[_pos+1] << 16) |
				  (m_pData[_pos+2] << 8)  |
				  (m_pData[_pos+3] << 0);

	return result;
}
