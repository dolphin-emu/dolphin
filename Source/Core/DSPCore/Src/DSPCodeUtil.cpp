// Copyright (C) 2003 Dolphin Project.

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

#include <iostream>
#include <vector>

#include "Common.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "DSPCodeUtil.h"
#include "assemble.h"
#include "disassemble.h"


bool Assemble(const char *text, std::vector<u16> &code, bool force)
{
	AssemblerSettings settings;
	//	settings.pc = 0;
	// settings.decode_registers = false;
	// settings.decode_names = false;
	settings.force = force;
	//	settings.print_tabs = false;
	//	settings.ext_separator = '\'';

	// TODO: fix the terrible api of the assembler.
	DSPAssembler assembler(settings);
	if (!assembler.Assemble(text, code)) {
		std::cerr << assembler.GetErrorString() << std::endl;
		return false;
	}

	return true;
}

bool Disassemble(const std::vector<u16> &code, bool line_numbers, std::string &text)
{
	if (code.empty())
		return false;

	AssemblerSettings settings;

	// These two prevent roundtripping.
	settings.show_hex = false;
	settings.show_pc = line_numbers;
	settings.ext_separator = '\'';
	settings.decode_names = false;
	settings.decode_registers = true;

	DSPDisassembler disasm(settings);
	bool success = disasm.Disassemble(0, code, 0x0000, text);
	return success;
}

bool Compare(const std::vector<u16> &code1, const std::vector<u16> &code2)
{		
	if (code1.size() != code2.size())
		printf("Size difference! 1=%i 2=%i\n", (int)code1.size(), (int)code2.size());
	u32 count_equal = 0;
	const int min_size = (int)std::min(code1.size(), code2.size());

	AssemblerSettings settings;
	DSPDisassembler disassembler(settings);
	for (int i = 0; i < min_size; i++)
	{
		if (code1[i] == code2[i])
			count_equal++;
		else
		{
			std::string line1, line2;
			u16 pc = i;
			disassembler.DisOpcode(&code1[0], 0x0000, 2, &pc, line1);
			pc = i;
			disassembler.DisOpcode(&code2[0], 0x0000, 2, &pc, line2);
			printf("!! %04x : %04x vs %04x - %s  vs  %s\n", i, code1[i], code2[i], line1.c_str(), line2.c_str());
		}
	}
	if (code2.size() != code1.size())
	{
		printf("Extra code words:\n");
		const std::vector<u16> &longest = code1.size() > code2.size() ? code1 : code2;
		for (int i = min_size; i < (int)longest.size(); i++)
		{
			u16 pc = i;
			std::string line;
			disassembler.DisOpcode(&longest[0], 0x0000, 2, &pc, line);
			printf("!! %s\n", line.c_str());
		}
	}
	printf("Equal instruction words: %i / %i\n", count_equal, min_size);
	return code1.size() == code2.size() && code1.size() == count_equal;
}

void GenRandomCode(int size, std::vector<u16> &code) 
{
	code.resize(size);
	for (int i = 0; i < size; i++)
	{
		code[i] = rand() ^ (rand() << 8);
	}
}

void CodeToHeader(const std::vector<u16> &code, std::string _filename,
				  const char *name, std::string &header)
{
	std::vector<u16> code_copy = code;
	// Add some nops at the end to align the size a bit.
	while (code_copy.size() & 7)
		code_copy.push_back(0);
	char buffer[1024];
	header.clear();
	header.reserve(code.size() * 4);
	header.append("#define NUM_UCODES 1\n\n");
	std::string filename;
	SplitPath(_filename, NULL, &filename, NULL);
	header.append(StringFromFormat("const char* UCODE_NAMES[NUM_UCODES] = {\"%s\"};\n\n", filename.c_str()));
	header.append("#ifndef _MSCVER\n");
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");
	header.append("#else\n");
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] __attribute__ ((aligned (64))) = {\n");
	header.append("#endif\n\n");
	
	header.append("\t{\n\t\t");
	for (u32 j = 0; j < code.size(); j++)
	{
		if (j && ((j & 15) == 0))
			header.append("\n\t\t");
		sprintf(buffer, "0x%04x, ", code[j]);
		header.append(buffer);
	}
	header.append("\n\t},\n");

	header.append("};\n");
}

void CodesToHeader(const std::vector<u16> *codes, const std::vector<std::string>* filenames,
				   int numCodes, const char *name, std::string &header)
{
	char buffer[1024];
	int reserveSize = 0;
	for(int i = 0; i < numCodes; i++)
		reserveSize += (int)codes[i].size();

	
	header.clear();
	header.reserve(reserveSize * 4);
	sprintf(buffer, "#define NUM_UCODES %d\n\n", numCodes);
	header.append(buffer);
	header.append("const char* UCODE_NAMES[NUM_UCODES] = {\n");
	for (int i = 0; i < numCodes; i++)
	{
		std::string filename;
		if (! SplitPath(filenames->at(i), NULL, &filename, NULL))
			filename = filenames->at(i);
		sprintf(buffer, "\t\"%s\",\n", filename.c_str());
		header.append(buffer);
	}
	header.append("};\n\n");
	header.append("#ifndef _MSCVER\n");
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");
	header.append("#else\n");
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] __attribute__ ((aligned (64))) = {\n");
	header.append("#endif\n\n");

	for(int i = 0; i < numCodes; i++) {
		if(codes[i].size() == 0)
			continue;

		std::vector<u16> code_copy = codes[i];
		// Add some nops at the end to align the size a bit.
		while (code_copy.size() & 7)
			code_copy.push_back(0);

		header.append("\t{\n\t\t");
		for (u32 j = 0; j < codes[i].size(); j++) 
		{
			if (j && ((j & 15) == 0))
				header.append("\n\t\t");
			sprintf(buffer, "0x%04x, ", codes[i][j]);
			header.append(buffer);
		}
		header.append("\n\t},\n");
	}
	header.append("};\n");
}

void CodeToBinaryStringBE(const std::vector<u16> &code, std::string &str)
{
	str.resize(code.size() * 2);
	for (int i = 0; i < (int)code.size(); i++)
	{
		str[i * 2 + 0] = code[i] >> 8;
		str[i * 2 + 1] = code[i] & 0xff;
	}
}

void BinaryStringBEToCode(const std::string &str, std::vector<u16> &code)
{
	code.resize(str.size() / 2);
	for (int i = 0; i < (int)code.size(); i++)
	{
		code[i] = ((u16)(u8)str[i * 2 + 0] << 8) | ((u16)(u8)str[i * 2 + 1]);
	}
}

bool LoadBinary(const char *filename, std::vector<u16> &code)
{
	std::string buffer;
	if (!File::ReadFileToString(false, filename, buffer))
		return false;

	BinaryStringBEToCode(buffer, code);
	return true;
}

bool SaveBinary(const std::vector<u16> &code, const char *filename)
{
	std::string buffer;
	CodeToBinaryStringBE(code, buffer);
	if (!File::WriteStringToFile(false, buffer, filename))
		return false;
	return true;
}
