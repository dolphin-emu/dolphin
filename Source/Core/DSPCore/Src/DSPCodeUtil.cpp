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

#include <vector>

#include "Common.h"
#include "FileUtil.h"
#include "DSPCodeUtil.h"
#include "assemble.h"
#include "disassemble.h"


bool Assemble(const char *text, std::vector<u16> *code)
{
	AssemblerSettings settings;
	settings.pc = 0;
	// settings.decode_registers = false;
	// settings.decode_names = false;
	settings.print_tabs = false;
	settings.ext_separator = '\'';

	// TODO: fix the terrible api of the assembler.
	DSPAssembler assembler(settings);
	if (!assembler.Assemble(text, code))
		printf("%s", assembler.GetErrorString().c_str());

	return true;
}

bool Disassemble(const std::vector<u16> &code, bool line_numbers, std::string *text)
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
	bool success = disasm.Disassemble(0, code, text);
	return success;
}

bool Compare(const std::vector<u16> &code1, const std::vector<u16> &code2)
{		
	if (code1.size() != code2.size())
		printf("Size difference! 1=%i 2=%i\n", (int)code1.size(), (int)code2.size());
	int count_equal = 0;
	const int min_size = (int)std::min(code1.size(), code2.size());
	for (int i = 0; i < min_size; i++)
	{
		if (code1[i] == code2[i])
			count_equal++;
		else
			printf("!! %04x : %04x vs %04x\n", i, code1[i], code2[i]);
	}
	printf("Equal instruction words: %i / %i\n", count_equal, min_size);
	return code1.size() == code2.size() && code1.size() == count_equal;
}

void GenRandomCode(int size, std::vector<u16> *code) 
{
	code->resize(size);
	for (int i = 0; i < size; i++)
	{
		(*code)[i] = rand() ^ (rand() << 8);
	}
}

void CodeToHeader(const std::vector<u16> &code, const char *name, std::string *header)
{
	char buffer[1024];
	header->clear();
	header->reserve(code.size() * 4);
	header->append("#ifndef _MSCVER\n");
	sprintf(buffer, "const __declspec(align:64) unsigned short %s = {\n");
	header->append(buffer);
	header->append("#else\n");
	sprintf(buffer, "const unsigned short %s __attribute__(aligned:64) = {\n");
	header->append(buffer);
	header->append("#endif\n\n    ");
	for (int i = 0; i < code.size(); i++) 
	{
		if (((i + 1) & 15) == 0)
			header->append("\n    ");
		sprintf(buffer, "%02x, ", code[i]);
		header->append(buffer);
	}
	header->append("\n};\n");
}

void CodeToBinaryStringBE(const std::vector<u16> &code, std::string *str)
{
	str->resize(code.size() * 2);
	for (int i = 0; i < code.size(); i++)
	{
		(*str)[i * 2 + 0] = code[i] >> 8;
		(*str)[i * 2 + 1] = code[i] & 0xff;
	}
}

void BinaryStringBEToCode(const std::string &str, std::vector<u16> *code)
{
	code->resize(str.size() / 2);
	for (int i = 0; i < code->size(); i++)
	{
		(*code)[i] = ((u16)(u8)str[i * 2 + 0] << 8) | ((u16)(u8)str[i * 2 + 1]);
	}
}

bool LoadBinary(const char *filename, std::vector<u16> *code)
{
	std::string buffer;
	if (!File::ReadFileToString(false, filename, &buffer))
		return false;

	BinaryStringBEToCode(buffer, code);
	return true;
}

bool SaveBinary(const std::vector<u16> &code, const char *filename)
{
	std::string buffer;
	CodeToBinaryStringBE(code, &buffer);
	if (!File::WriteStringToFile(false, buffer, filename))
		return false;
	return true;
}
