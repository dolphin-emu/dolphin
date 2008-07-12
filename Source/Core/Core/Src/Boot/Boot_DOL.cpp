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

#include "Boot_DOL.h"

#include "../HW/Memmap.h"

CDolLoader::CDolLoader(const char* _szFilename) : m_bInit(false)
{
	// try to open file
	FILE* pStream = fopen(_szFilename, "rb");
	if (pStream)
	{
		fread(&m_dolheader, 1, sizeof(SDolHeader), pStream);

		// swap memory
		u32* p = (u32*)&m_dolheader;
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

				for (u32 num = 0; num < m_dolheader.textSize[i]; num++)
					Memory::Write_U8(pTemp[num], m_dolheader.textAddress[i] + num);

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

				for (u32 num = 0; num < m_dolheader.dataSize[i]; num++)
					Memory::Write_U8(pTemp[num], m_dolheader.dataAddress[i] + num);

				delete [] pTemp;
			}
		}

		//TODO - we know where there is code, and where there is data
		//Make use of this!

		fclose(pStream);
		m_bInit = true;
	}
}

u32 CDolLoader::GetEntryPoint()
{
	return m_dolheader.entryPoint;
}
