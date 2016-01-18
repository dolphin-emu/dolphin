// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPCore.h"
#include "Core/DSP/DSPDisassembler.h"
#include "Core/HW/DSPLLE/DSPLLETools.h"

bool DumpDSPCode(const u8 *code_be, int size_in_bytes, u32 crc)
{
	const std::string binFile = StringFromFormat("%sDSP_UC_%08X.bin", File::GetUserPath(D_DUMPDSP_IDX).c_str(), crc);
	const std::string txtFile = StringFromFormat("%sDSP_UC_%08X.txt", File::GetUserPath(D_DUMPDSP_IDX).c_str(), crc);

	File::IOFile pFile(binFile, "wb");
	if (pFile)
	{
		pFile.WriteBytes(code_be, size_in_bytes);
		pFile.Close();
	}
	else
	{
		PanicAlert("Can't open file (%s) to dump UCode!!", binFile.c_str());
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

	return File::WriteStringToFile(text, txtFile);
}
