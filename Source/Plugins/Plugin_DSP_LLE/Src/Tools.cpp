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
#include <stdlib.h>

#include "Common.h"
#include "Globals.h"

#include "Tools.h"
#include "disassemble.h"
#include "gdsp_interpreter.h"

bool DumpDSPCode(const u8 *data, u32 _Length, u32 crc)
{
	char szFilename[MAX_PATH];
	sprintf(szFilename, "%sDSP_UC_%08X.bin", FULL_DSP_DUMP_DIR, crc);
	FILE* pFile = fopen(szFilename, "wb");

	if (pFile)
	{
		fwrite(data, _Length, 1, pFile);
		fclose(pFile);
	}
	else
	{
		PanicAlert("Cant open file (%s) to dump UCode!!", szFilename);
		return false;
	}

	if (!DisasmUCodeDump(crc))
	{
		PanicAlert("Failed to disasm UCode!!", szFilename);
		return false;
	}

	return true;
}

bool DisasmUCodeDump(u32 crc)
{
	char binFile[MAX_PATH];
	char txtFile[MAX_PATH];
	sprintf(binFile, "%sDSP_UC_%08X.bin", FULL_DSP_DUMP_DIR, crc);
	sprintf(txtFile, "%sDSP_UC_%08X.txt", FULL_DSP_DUMP_DIR, crc);
	FILE* t = fopen(txtFile, "wb");
	if (t != NULL)
	{
		AssemblerSettings settings;
		memset(&settings, 0, sizeof(settings));
		settings.show_hex = true;
		settings.show_pc = true;
		settings.ext_separator = '\t';
		settings.decode_names = true;
		settings.decode_registers = true;
		DSPDisassembler disasm(settings);
		disasm.gd_dis_file(binFile, t);
		fclose(t);
		return true;
	}
	else
		return false;
}

u32 GenerateCRC(const unsigned char* _pBuffer, int _pLength)
{
	unsigned long CRC = 0xFFFFFFFF;

	while (_pLength--)
	{
		unsigned long Temp = (unsigned long)((CRC & 0xFF) ^ *_pBuffer++);

		for (int j = 0; j < 8; j++)
		{
			if (Temp & 0x1)
				Temp = (Temp >> 1) ^ 0xEDB88320;
			else
				Temp >>= 1;
		}

		CRC = (CRC >> 8) ^ Temp;
	}

	return CRC ^ 0xFFFFFFFF;
}

// TODO make this useful :p
bool DumpCWCode(u32 _Address, u32 _Length)
{
	char filename[256];
	sprintf(filename, "%sDSP_UCode.bin", FULL_DSP_DUMP_DIR);
	FILE* pFile = fopen(filename, "wb");

	if (pFile != NULL)
	{
		for (size_t i = _Address; i < _Address + _Length; i++)
		{
			u16 val = g_dsp.iram[i];
			fprintf(pFile, "    cw 0x%04x \n", val);
		}

		fclose(pFile);
		return true;
	}

	return false;
}
