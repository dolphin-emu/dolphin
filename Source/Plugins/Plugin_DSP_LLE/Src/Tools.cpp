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

#include <stdio.h>
#include <stdlib.h>

#include "Common.h"
#include "Globals.h"

#include "gdsp_interpreter.h"

bool DumpDSPCode(uint32 _Address, uint32 _Length, uint32 crc)
{
	char szFilename[MAX_PATH];
	sprintf(szFilename, "c:\\_\\DSP_UC_%08X.bin", crc);
	FILE* pFile = fopen(szFilename, "wb");

	if (pFile != NULL)
	{
		fwrite(g_dspInitialize.pGetMemoryPointer(_Address), _Length, 1, pFile);
		fclose(pFile);
		return(true);
	}
	else
	{
		PanicAlert("Cant open file (%s) to dump UCode!!", szFilename);
	}

	return(false);
}


uint32 GenerateCRC(const unsigned char* _pBuffer, int _pLength)
{
	unsigned long CRC = 0xFFFFFFFF;

	while (_pLength--)
	{
		unsigned long Temp = (unsigned long)((CRC & 0xFF) ^ *_pBuffer++);

		for (int j = 0; j < 8; j++)
		{
			if (Temp & 0x1)
			{
				Temp = (Temp >> 1) ^ 0xEDB88320;
			}
			else
			{
				Temp >>= 1;
			}
		}

		CRC = (CRC >> 8) ^ Temp;
	}

	return(CRC ^ 0xFFFFFFFF);
}


bool DumpCWCode(uint32 _Address, uint32 _Length)
{
	FILE* pFile = fopen("d:\\DSP_UCode.bin", "wb");

	if (pFile != NULL)
	{
		for (size_t i = _Address; i < _Address + _Length; i++)
		{
			uint16 val = Common::swap16(g_dsp.iram[i]);
			fprintf(pFile, "    cw 0x%04x \n", val);
		}

		fclose(pFile);
		return(true);
	}

	return(false);
}


