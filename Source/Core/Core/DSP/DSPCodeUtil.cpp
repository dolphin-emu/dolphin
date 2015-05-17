// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/DSP/DSPAssembler.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPDisassembler.h"

bool Assemble(const std::string& text, std::vector<u16> &code, bool force)
{
	AssemblerSettings settings;
	// settings.pc = 0;
	// settings.decode_registers = false;
	// settings.decode_names = false;
	settings.force = force;
	// settings.print_tabs = false;
	// settings.ext_separator = '\'';

	// TODO: fix the terrible api of the assembler.
	DSPAssembler assembler(settings);
	if (!assembler.Assemble(text, code))
	{
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
	settings.show_hex = true;
	settings.show_pc = line_numbers;
	settings.ext_separator = '\'';
	settings.decode_names = true;
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
	const int min_size = std::min<int>((int)code1.size(), (int)code2.size());

	AssemblerSettings settings;
	DSPDisassembler disassembler(settings);
	for (int i = 0; i < min_size; i++)
	{
		if (code1[i] == code2[i])
		{
			count_equal++;
		}
		else
		{
			std::string line1, line2;
			u16 pc = i;
			disassembler.DisassembleOpcode(&code1[0], 0x0000, 2, &pc, line1);
			pc = i;
			disassembler.DisassembleOpcode(&code2[0], 0x0000, 2, &pc, line2);
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
			disassembler.DisassembleOpcode(&longest[0], 0x0000, 2, &pc, line);
			printf("!! %s\n", line.c_str());
		}
	}
	printf("Equal instruction words: %i / %i\n", count_equal, min_size);
	return code1.size() == code2.size() && code1.size() == count_equal;
}

void GenRandomCode(u32 size, std::vector<u16> &code)
{
	code.resize(size);
	for (u32 i = 0; i < size; i++)
	{
		code[i] = rand() ^ (rand() << 8);
	}
}

void CodeToHeader(const std::vector<u16> &code, std::string _filename,
				  const char *name, std::string &header)
{
	std::vector<u16> code_padded = code;
	// Pad with nops to 32byte boundary
	while (code_padded.size() & 0x7f)
		code_padded.push_back(0);
	header.clear();
	header.reserve(code_padded.size() * 4);
	header.append("#define NUM_UCODES 1\n\n");
	std::string filename;
	SplitPath(_filename, nullptr, &filename, nullptr);
	header.append(StringFromFormat("const char* UCODE_NAMES[NUM_UCODES] = {\"%s\"};\n\n", filename.c_str()));
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");

	header.append("\t{\n\t\t");
	for (u32 j = 0; j < code_padded.size(); j++)
	{
		if (j && ((j & 15) == 0))
			header.append("\n\t\t");
		header.append(StringFromFormat("0x%04x, ", code_padded[j]));
	}
	header.append("\n\t},\n");

	header.append("};\n");
}

void CodesToHeader(const std::vector<u16> *codes, const std::vector<std::string>* filenames,
				   u32 numCodes, const char *name, std::string &header)
{
	std::vector<std::vector<u16> > codes_padded;
	u32 reserveSize = 0;
	for (u32 i = 0; i < numCodes; i++)
	{
		codes_padded.push_back(codes[i]);
		// Pad with nops to 32byte boundary
		while (codes_padded.at(i).size() & 0x7f)
			codes_padded.at(i).push_back(0);

		reserveSize += (u32)codes_padded.at(i).size();
	}
	header.clear();
	header.reserve(reserveSize * 4);
	header.append(StringFromFormat("#define NUM_UCODES %u\n\n", numCodes));
	header.append("const char* UCODE_NAMES[NUM_UCODES] = {\n");
	for (u32 i = 0; i < numCodes; i++)
	{
		std::string filename;
		if (! SplitPath(filenames->at(i), nullptr, &filename, nullptr))
			filename = filenames->at(i);
		header.append(StringFromFormat("\t\"%s\",\n", filename.c_str()));
	}
	header.append("};\n\n");
	header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");

	for (u32 i = 0; i < numCodes; i++)
	{
		if (codes[i].size() == 0)
			continue;

		header.append("\t{\n\t\t");
		for (u32 j = 0; j < codes_padded.at(i).size(); j++)
		{
			if (j && ((j & 15) == 0))
				header.append("\n\t\t");
			header.append(StringFromFormat("0x%04x, ", codes_padded.at(i).at(j)));
		}
		header.append("\n\t},\n");
	}
	header.append("};\n");
}

void CodeToBinaryStringBE(const std::vector<u16> &code, std::string &str)
{
	str.resize(code.size() * 2);
	for (size_t i = 0; i < code.size(); i++)
	{
		str[i * 2 + 0] = code[i] >> 8;
		str[i * 2 + 1] = code[i] & 0xff;
	}
}

void BinaryStringBEToCode(const std::string &str, std::vector<u16> &code)
{
	code.resize(str.size() / 2);
	for (size_t i = 0; i < code.size(); i++)
	{
		code[i] = ((u16)(u8)str[i * 2 + 0] << 8) | ((u16)(u8)str[i * 2 + 1]);
	}
}

bool LoadBinary(const std::string& filename, std::vector<u16> &code)
{
	std::string buffer;
	if (!File::ReadFileToString(filename, buffer))
		return false;

	BinaryStringBEToCode(buffer, code);
	return true;
}

bool SaveBinary(const std::vector<u16> &code, const std::string& filename)
{
	std::string buffer;
	CodeToBinaryStringBE(code, buffer);
	if (!File::WriteStringToFile(buffer, filename))
		return false;
	return true;
}
