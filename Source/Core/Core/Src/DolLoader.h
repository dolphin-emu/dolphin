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
/////////////////////////////////////////////////////////////////////////////////////////////////////
// M O D U L E  B E G I N ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// C L A S S/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

class CDolLoader
{
public:

	CDolLoader(const char* _szFilename) :
		m_bInit(false)
	{
		// try to open file
		FILE* pStream = NULL;
		fopen_s(&pStream, _szFilename, "rb");
		if (pStream)
		{
			fread(&m_dolheader, 1, sizeof(SDolHeader), pStream);

			// swap memory
			DWORD* p = (DWORD*)&m_dolheader;
			for (int i=0; i<(sizeof(SDolHeader)>>2); i++)	
				p[i] = Common::swap32(p[i]);

			// load all text (code) sections
			for(int i=0; i<DOL_NUM_TEXT; i++)
			{
				if(m_dolheader.textOffset[i] != 0)
				{
					u8* pTemp = new u8[m_dolheader.textSize[i]];

					fseek(pStream, m_dolheader.textOffset[i], SEEK_SET);
					fread(pTemp, 1, m_dolheader.textSize[i], pStream);

					for (size_t num=0; num<m_dolheader.textSize[i]; num++)
						CMemory::Write_U8(pTemp[num], m_dolheader.textAddress[i] + num);

					delete [] pTemp;
				}
			}

			// load all data sections
			for(int i=0; i<DOL_NUM_DATA; i++)
			{
				if(m_dolheader.dataOffset[i] != 0)
				{
					u8* pTemp = new u8[m_dolheader.dataSize[i]];

					fseek(pStream, m_dolheader.dataOffset[i], SEEK_SET);
					fread(pTemp, 1, m_dolheader.dataSize[i], pStream);

					for (size_t num=0; num<m_dolheader.dataSize[i]; num++)
						CMemory::Write_U8(pTemp[num], m_dolheader.dataAddress[i] + num);

					delete [] pTemp;
				}
			}

			fclose(pStream);
			m_bInit = true;
		}
	}

	_u32 GetEntryPoint(void)
	{
		return m_dolheader.entryPoint;
	}
	
private:

	enum
	{
		DOL_NUM_TEXT	= 7,
		DOL_NUM_DATA	= 11
	};

	struct SDolHeader
	{
		_u32     textOffset[DOL_NUM_TEXT];
		_u32     dataOffset[DOL_NUM_DATA];

		_u32     textAddress[DOL_NUM_TEXT];
		_u32     dataAddress[DOL_NUM_DATA];

		_u32     textSize[DOL_NUM_TEXT];
		_u32     dataSize[DOL_NUM_DATA];

		_u32     bssAddress;
		_u32     bssSize;
		_u32     entryPoint;
		_u32     padd[7];
	};

	SDolHeader m_dolheader;

	bool m_bInit;
};