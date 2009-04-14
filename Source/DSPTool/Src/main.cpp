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
#include "DSPCodeUtil.h"

// Stub out the dsplib host stuff, since this is just a simple cmdline tools.
u8 DSPHost_ReadHostMemory(u32 addr) { return 0; }
bool DSPHost_OnThread() { return false; }
bool DSPHost_Running() { return true; }
u32 DSPHost_CodeLoaded(const u8 *ptr, int size) {return 0x1337c0de;}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Then the two binaries are compared.
bool RoundTrip(const std::vector<u16> &code1)
{
	std::vector<u16> code2;
	std::string text;
	if (!Disassemble(code1, false, &text))
	{
		printf("RoundTrip: Disassembly failed.\n");
		return false;
	}
	if (!Assemble(text.c_str(), &code2))
	{
		printf("RoundTrip: Assembly failed.\n");
		return false;
	}
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
	if (!Disassemble(code1, false, &text))
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
	/*
	std::string text2;
	Disassemble(code1, true, &text1);
	Disassemble(code2, true, &text2);
	File::WriteStringToFile(true, text1, "code1.txt");
	File::WriteStringToFile(true, text2, "code2.txt");
	*/
	return true;
}

void RunAsmTests()
{
	bool fail = false;
#define CHK(a) if (!SuperTrip(a)) printf("FAIL\n%s\n", a), fail = true;

	// Let's start out easy - a trivial instruction..
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

	// Let's try some messy extended instructions.
	//CHK("   MULMV'SN    $AX0.L, $AX0.H, $ACC0 : @$AR2, $AC1.M\n");

		//"   ADDAXL'MV   $ACC1, $AX1.L : $AX1.H, $AC1.M\n");
	// Let's get brutal. We generate random code bytes and make sure that they can
	// be roundtripped. We don't expect it to always succeed but it'll be sure to generate
	// interesting test cases.

	puts("Insane Random Code Test\n");
	std::vector<u16> rand_code;
	GenRandomCode(30, &rand_code);
	std::string rand_code_text;
	Disassemble(rand_code, true, &rand_code_text);
	printf("%s", rand_code_text.c_str());
	RoundTrip(rand_code);

	std::string dsp_test;

	if (File::ReadFileToString(true, "C:/devkitPro/examples/wii/asndlib/dsptest/dsp_test.ds", &dsp_test))
		SuperTrip(dsp_test.c_str());

	//.File::ReadFileToString(true, "C:/devkitPro/trunk/libogc/libasnd/dsp_mixer/dsp_mixer.s", &dsp_test);
	// This is CLOSE to working. Sorry about the local path btw. This is preliminary code.

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