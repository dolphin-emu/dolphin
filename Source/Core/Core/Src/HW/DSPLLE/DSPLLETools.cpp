// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <stdio.h>
#include <stdlib.h>

#include "Common.h"
#include "DSPLLEGlobals.h"

#include "FileUtil.h"
#include "DSP/DSPCore.h"
#include "DSP/DSPCodeUtil.h"
#include "DSPLLETools.h"
#include "DSP/disassemble.h"
#include "DSP/DSPInterpreter.h"

bool DumpDSPCode(const u8 *code_be, int size_in_bytes, u32 crc)
{
	char binFile[MAX_PATH];
	char txtFile[MAX_PATH];
	sprintf(binFile, "%sDSP_UC_%08X.bin", File::GetUserPath(D_DUMPDSP_IDX).c_str(), crc);
	sprintf(txtFile, "%sDSP_UC_%08X.txt", File::GetUserPath(D_DUMPDSP_IDX).c_str(), crc);

	File::IOFile pFile(binFile, "wb");
	if (pFile)
	{
		pFile.WriteBytes(code_be, size_in_bytes);
		pFile.Close();
	}
	else
	{
		PanicAlert("Can't open file (%s) to dump UCode!!", binFile);
		return false;
	}

	// Load the binary back in.
	std::vector<u16> code;
	LoadBinary(binFile, code);

	AssemblerSettings settings;
	settings.show_hex = true;
	settings.show_pc = true;
	settings.ext_separator = '\'';
	settings.decode_names = true;
	settings.decode_registers = true;

	std::string text;
	DSPDisassembler disasm(settings);

	if (!disasm.Disassemble(0, code, 0x0000, text))
		return false;

	return File::WriteStringToFile(true, text, txtFile);
}

// TODO make this useful :p
bool DumpCWCode(u32 _Address, u32 _Length)
{
	std::string filename = File::GetUserPath(D_DUMPDSP_IDX) + "DSP_UCode.bin";
	File::IOFile pFile(filename, "wb");

	if (pFile)
	{
		for (size_t i = _Address; i != _Address + _Length; ++i)
		{
			u16 val = g_dsp.iram[i];
			fprintf(pFile.GetHandle(), "    cw 0x%04x \n", val);
		}
		return true;
	}

	return false;
}
