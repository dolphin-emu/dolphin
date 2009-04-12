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

DSPAssembler::DSPAssembler()
:   include_dir(0),
    current_param(0),
	cur_addr(0),
	labels_count(0),
	cur_pass(0)
{
	include_dir = 0;
	current_param = 0;
}

DSPAssembler::~DSPAssembler()
{

}


bool Assemble(const char *text, std::vector<u16> *code)
{
	const char *fname = "tmp.asm";
	gd_globals_t gdg;
	memset(&gdg, 0, sizeof(gdg));
	gdg.pc = 0;
	// gdg.decode_registers = false;
	// gdg.decode_names = false;
	gdg.print_tabs = false;
	gdg.ext_separator = '\'';
	gdg.buffer = 0;

	if (!File::WriteStringToFile(true, text, fname))
		return false;

	// TODO: fix the terrible api of the assembler.
	DSPAssembler assembler;
	assembler.gd_ass_init_pass(1);
	if (!assembler.gd_ass_file(&gdg, fname, 1))
		return false;
	assembler.gd_ass_init_pass(2);
	if (!assembler.gd_ass_file(&gdg, fname, 2))
		return false;

	code->resize(gdg.buffer_size);
	for (int i = 0; i < gdg.buffer_size; i++) {
		(*code)[i] = *(u16 *)(gdg.buffer + i * 2);
	}
	return true;
}

bool Disassemble(const std::vector<u16> &code, bool line_numbers, std::string *text)
{
	if (code.empty())
		return false;
	const char *tmp1 = "tmp1.bin";
	const char *tmp2 = "tmp.txt";

	// First we have to dump the code to a bin file.
	FILE *f = fopen(tmp1, "wb");
	fwrite(&code[0], 1, code.size() * 2, f);
	fclose(f);

	FILE* t = fopen(tmp2, "w");
	if (t != NULL)
	{
		gd_globals_t gdg;
		memset(&gdg, 0, sizeof(gdg));

		// These two prevent roundtripping.
		gdg.show_hex = false;
		gdg.show_pc = line_numbers;
		gdg.ext_separator = '\'';
		gdg.decode_names = false;
		gdg.decode_registers = true;

		DSPDisassembler disasm;
		bool success = disasm.gd_dis_file(&gdg, tmp1, t);
		fclose(t);

		File::ReadFileToString(true, tmp2, text);
		return success;
	}
	return false;
}

bool Compare(const std::vector<u16> &code1, const std::vector<u16> &code2)
{		
	if (code1.size() != code2.size())
		printf("Size difference! 1=%i 2=%i\n", (int)code1.size(), (int)code2.size());
	int count_equal = 0;
	const int min_size = std::min(code1.size(), code2.size());
	for (int i = 0; i < min_size; i++)
	{
		if (code1[i] == code2[i])
			count_equal++;
		else
			printf("!! %i : %04x vs %04x\n", i, code1[i], code2[i]);
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

void CodeToHeader(std::vector<u16> *code, const char *name, std::string *header)
{
	char buffer[1024];
	header->clear();
	header->reserve(code->size() * 4);
	header->append("#ifndef _MSCVER\n");
	sprintf(buffer, "const __declspec(align:64) unsigned short %s = {\n");
	header->append(buffer);
	header->append("#else\n");
	sprintf(buffer, "const unsigned short %s __attribute__(aligned:64) = {\n");
	header->append(buffer);
	header->append("#endif\n\n    ");
	for (int i = 0; i < code->size(); i++) 
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
		(*code)[i] = (str[i * 2 + 0] << 8) | (str[i * 2 + 1]);
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
