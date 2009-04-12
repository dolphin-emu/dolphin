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

#include "Common.h"
#include "FileUtil.h"
#include "assemble.h"
#include "disassemble.h"

// Stub out the dsplib host stuff, since this is just a simple cmdline tools.
u8 DSPHost_ReadHostMemory(u32 addr) { return 0; }
bool DSPHost_OnThread() { return false; }
bool DSPHost_Running() { return true; }
u32 DSPHost_CodeLoaded(const u8 *ptr, int size) {return 0x1337c0de;}

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
	gd_ass_init_pass(1);
	if (!gd_ass_file(&gdg, fname, 1))
		return false;
	gd_ass_init_pass(2);
	if (!gd_ass_file(&gdg, fname, 2))
		return false;

	code->resize(gdg.buffer_size);
	for (int i = 0; i < gdg.buffer_size; i++) {
		(*code)[i] = *(u16 *)(gdg.buffer + i * 2);
	}
	return true;
}

bool Disassemble(const std::vector<u16> &code, std::string *text)
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
		gdg.show_pc = false;
		gdg.ext_separator = '\'';
		gdg.decode_names = false;
		gdg.decode_registers = true;
		bool success = gd_dis_file(&gdg, tmp1, t);
		fclose(t);

		File::ReadStringFromFile(true, tmp2, text);
		return success;
	}
	return false;
}

void Compare(const std::vector<u16> &code1, const std::vector<u16> &code2)
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
			printf("!! %04x : %04x vs %04x\n", i, code1[i], code2[i]);
	}
	printf("Equal instruction words: %i / %i\n", count_equal, min_size);
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

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Then the two binaries are compared.
bool RoundTrip(const std::vector<u16> &code1)
{
	std::vector<u16> code2;
	std::string text;
	Disassemble(code1, &text);
	Assemble(text.c_str(), &code2);
	Compare(code1, code2);	
	return true;
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Very convenient for testing. Then the two binaries are compared.
bool SuperTrip(const char *asm_code)
{
	std::vector<u16> code1, code2;
	std::string text;
	if (!Assemble(asm_code, &code1))
	{
		printf("SuperTrip: First assembly failed\n");
		return false;
	}
	printf("First assembly: %i words\n", (int)code1.size());
	if (!Disassemble(code1, &text))
	{
		printf("SuperTrip: Disassembly failed\n");
		return false;
	}
	else
	{
		//printf("Disass:\n");
		//printf("%s", text.c_str());
	}
	if (!Assemble(text.c_str(), &code2))
	{
		printf("SuperTrip: Second assembly failed\n");
		return false;
	}
	Compare(code1, code2);
	return true;
}

void RunAsmTests()
{
	bool fail = false;
#define CHK(a) if (!SuperTrip(a)) printf("FAIL\n%s\n", a), fail = true;

	// Let's start out easy - a trivial instruction..
#if 0
	CHK("	NOP\n");

	// Now let's do several.
	CHK("	NOP\n"
		"	NOP\n"
		"	NOP\n");

	// Turning it up a notch.
	CHK("	SET16\n"
		"	SET40\n"
		"	CLR15\n"
		"	M0\n"
		"	M2\n");

	// Time to try labels and parameters, and comments.
	CHK("DIRQ_TEST:	equ	0xfffb	; DSP Irq Request\n"
		"	si		@0xfffc, #0x8888\n"
		"	si		@0xfffd, #0xbeef\n"
		"	si		@DIRQ_TEST, #0x0001\n");

	// Let's see if registers roundtrip. Also try predefined labels.
	CHK("	si		@0xfffc, #0x8888\n"
		"	si		@0xfffd, #0xbeef\n"
		"	si		@DIRQ, #0x0001\n");

	// Let's get brutal. We generate random code bytes and make sure that they can
	// be roundtripped. We don't expect it to always succeed but it'll be sure to generate
	// interesting test cases.

	puts("Insane Random Code Test\n");
	std::vector<u16> rand_code;
	GenRandomCode(21, &rand_code);
	std::string rand_code_text;
	Disassemble(rand_code, &rand_code_text);
	printf("%s", rand_code_text.c_str());
	RoundTrip(rand_code);
#endif

	std::string dsp_test;
	//File::ReadStringFromFile(true, "C:/devkitPro/examples/wii/asndlib/dsptest/dsp_test.ds", &dsp_test);

	File::ReadStringFromFile(true, "C:/devkitPro/trunk/libogc/libasnd/dsp_mixer/dsp_mixer.s", &dsp_test);
	// This is CLOSE to working. Sorry about the local path btw. This is preliminary code.
	SuperTrip(dsp_test.c_str());

	if (!fail)
		printf("All passed!\n");
}

// So far, all this binary can do is test partially that itself works correctly.
int main(int argc, const char *argv[])
{
	if (argc == 2 && !strcmp(argv[1], "test"))
	{
		RunAsmTests();		
	}
	return 0;
}