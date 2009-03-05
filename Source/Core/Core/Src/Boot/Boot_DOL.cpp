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
#include "FileUtil.h"

#include "../HW/Memmap.h"


CDolLoader::CDolLoader(u8* _pBuffer, u32 _Size)
	: m_bInit(false)
{
	m_bInit = Initialize(_pBuffer, _Size);
}

CDolLoader::CDolLoader(const char* _szFilename) 
	: m_bInit(false)
{
	u64 size = File::GetSize(_szFilename);
	u8* tmpBuffer = new u8[(size_t)size];

	FILE* pStream = fopen(_szFilename, "rb");	
	fread(tmpBuffer, size_t(size), 1, pStream);
	fclose(pStream);

	m_bInit = Initialize(tmpBuffer, (u32)size);
	delete [] tmpBuffer;
}

bool CDolLoader::Initialize(u8* _pBuffer, u32 _Size)
{	
	memcpy(&m_dolheader, _pBuffer, sizeof(SDolHeader));

	// swap memory
	u32* p = (u32*)&m_dolheader;
	for (size_t i=0; i<(sizeof(SDolHeader)>>2); i++)	
		p[i] = Common::swap32(p[i]);

	// load all text (code) sections
	for(int i = 0; i < DOL_NUM_TEXT; i++)
	{
		if(m_dolheader.textOffset[i] != 0)
		{
			u8* pTemp = &_pBuffer[m_dolheader.textOffset[i]];
			for (u32 num = 0; num < m_dolheader.textSize[i]; num++)
				Memory::Write_U8(pTemp[num], m_dolheader.textAddress[i] + num);
		}
	}

	// load all data sections
	for(int i = 0; i < DOL_NUM_DATA; i++)
	{
		if(m_dolheader.dataOffset[i] != 0)
		{
			u8* pTemp = &_pBuffer[m_dolheader.dataOffset[i]];
			for (u32 num = 0; num < m_dolheader.dataSize[i]; num++)
				Memory::Write_U8(pTemp[num], m_dolheader.dataAddress[i] + num);
		}
	}

	return true;
}

u32 CDolLoader::GetEntryPoint()
{
	return m_dolheader.entryPoint;
}

bool CDolLoader::IsDolWii(const char* filename)
{
	// try to open file
	FILE* pStream = fopen(filename, "rb");
	if (pStream)
	{
		fseek(pStream, 0xe0, SEEK_SET);
		u32 entrypt = fgetc(pStream) << 24 | fgetc(pStream) << 16 |
			fgetc(pStream) << 8 | fgetc(pStream);

		fclose(pStream);
		return entrypt >= 0x80004000;
	}
	return 0;
}
