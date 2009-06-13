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

//#include "dsp_test.h"

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
	if (!Disassemble(code1, false, text))
	{
		printf("RoundTrip: Disassembly failed.\n");
		return false;
	}
	if (!Assemble(text.c_str(), code2))
	{
		printf("RoundTrip: Assembly failed.\n");
		return false;
	}
	if (!Compare(code1, code2))
	{
		Disassemble(code1, true, text);
		printf("%s", text.c_str());
	}
	return true;
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Very convenient for testing. Then the two binaries are compared.
bool SuperTrip(const char *asm_code)
{
	std::vector<u16> code1, code2;
	std::string text;
	if (!Assemble(asm_code, code1))
	{
		printf("SuperTrip: First assembly failed\n");
		return false;
	}
	printf("First assembly: %i words\n", (int)code1.size());
	if (!Disassemble(code1, false, text))
	{
		printf("SuperTrip: Disassembly failed\n");
		return false;
	}
	else
	{
		printf("Disass:\n");
		printf("%s", text.c_str());
	}
	if (!Assemble(text.c_str(), code2))
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
	/*
	std::vector<u16> hermes;
	if (!LoadBinary("testdata/hermes.bin", &hermes))
		PanicAlert("Failed to load hermes rom");
	RoundTrip(hermes);
	*/
	/*
	std::vector<u16> code;
	std::string text_orig;
	File::ReadFileToString(false, "testdata/dsp_test.S", &text_orig);
	if (!Assemble(text_orig.c_str(), &code))
	{
		printf("SuperTrip: First assembly failed\n");
		return;
	}*/

	/*
	{
		std::vector<u16> code;
		code.clear();
		for (int i = 0; i < sizeof(dsp_test)/4; i++)
		{
			code.push_back(dsp_test[i] >> 16);
			code.push_back(dsp_test[i] & 0xFFFF);
		}

		SaveBinary(code, "dsp_test2.bin");
		RoundTrip(code);
	}*/
	//if (Compare(code, hermes))
	//	printf("Successs\n");
/*
	{
		std::vector<u16> code;
		std::string text;
		LoadBinary("testdata/dsp_test.bin", &code);
		Disassemble(code, true, &text);
		Assemble(text.c_str(), &code);
		Disassemble(code, true, &text);
		printf("%s", text.c_str());
	}*/
	/*
	puts("Insane Random Code Test\n");
	std::vector<u16> rand_code;
	GenRandomCode(30, &rand_code);
	std::string rand_code_text;
	Disassemble(rand_code, true, &rand_code_text);
	printf("%s", rand_code_text.c_str());
	RoundTrip(rand_code);


	if (File::ReadFileToString(true, "C:/devkitPro/examples/wii/asndlib/dsptest/dsp_test.ds", &dsp_test))
		SuperTrip(dsp_test.c_str());

	//.File::ReadFileToString(true, "C:/devkitPro/trunk/libogc/libasnd/dsp_mixer/dsp_mixer.s", &dsp_test);
	// This is CLOSE to working. Sorry about the local path btw. This is preliminary code.
*/
	
	std::string dsp_test;
	if (File::ReadFileToString(true, "Testdata/dsp_test.s", dsp_test))
		fail = fail || !SuperTrip(dsp_test.c_str());
	if (!fail)
		printf("All passed!\n");
}


// Usage:
// Run internal tests:
//   dsptool test
// Disassemble a file:
//   dsptool -d -o asdf.txt asdf.bin
// Disassemble a file, output to standard output:
//   dsptool -d asdf.bin
// Assemble a file:
//   dsptool -o asdf.bin asdf.txt
// Assemble a file, output header:
//   dsptool -h asdf.h asdf.txt

// So far, all this binary can do is test partially that itself works correctly.
int main(int argc, const char *argv[])
{
	if(argc == 1 || (argc == 2 && (!strcmp(argv[1], "--help") || (!strcmp(argv[1], "-?")))))
	{
		printf("USAGE: DSPTool [-?] [--help] [-d] [-m] [-o <FILE>] [-h <FILE>] <DSP ASSEMBLER FILE>\n");
		printf("-? / --help: Prints this message\n");
		printf("-d: Disassemble\n");
		printf("-m: Input file contains a list of files (Header assembly only)\n");
		printf("-o <OUTPUT FILE>: Results from stdout redirected to a file\n");
		printf("-h <HEADER FILE>: Output assembly results to a header\n");
		return 0;
	}

	if (argc == 2 && !strcmp(argv[1], "test"))
	{
		RunAsmTests();
		return 0;
	}

	std::string input_name;
	std::string output_header_name;
	std::string output_name;

	bool disassemble = false, compare = false, multiple = false;
	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-d"))
			disassemble = true;
		else if (!strcmp(argv[i], "-o"))
			output_name = argv[++i];
		else if (!strcmp(argv[i], "-h"))
			output_header_name = argv[++i];
		else if (!strcmp(argv[i], "-c"))
			compare = true;
		else if (!strcmp(argv[i], "-m"))
			multiple = true;
		else 
		{
			if (!input_name.empty())
			{
				printf("ERROR: Can only take one input file.\n");
				return 1;
			}
			input_name = argv[i];
			if (!File::Exists(input_name.c_str()))
			{
				printf("ERROR: Input path does not exist.\n");
				return 1;
			}
		}
	}

	if(multiple && (compare || disassemble || output_header_name.empty() ||
					input_name.empty())) {
		printf("ERROR: Multiple files can only be used with assembly "
			"and must compile a header file.\n");
		return 1;
	}

	if (compare)
	{
		// Two binary inputs, let's diff.
		std::string binary_code;
		std::vector<u16> code1, code2;
		File::ReadFileToString(false, input_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code1);
		File::ReadFileToString(false, output_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code2);
		Compare(code1, code2);
		return 0;
	}

	if (disassemble)
	{
		if (input_name.empty())
		{
			printf("Disassemble: Must specify input.\n");
			return 1;
		}
		std::string binary_code;
		std::vector<u16> code;
		File::ReadFileToString(false, input_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code);
		std::string text;
		Disassemble(code, true, text);
		if (!output_name.empty())
			File::WriteStringToFile(true, text, output_name.c_str());
		else
			printf("%s", text.c_str());
	}
	else 
	{
		if (input_name.empty())
		{
			printf("Assemble: Must specify input.\n");
			return 1;
		}
		std::string source;
		if (File::ReadFileToString(true, input_name.c_str(), source))
		{
			if(multiple) 
			{
				// When specifying a list of files we must compile a header
				// (we can't assemble multiple files to one binary)
				// since we checked it before, we assume output_header_name isn't empty
				int lines;
				std::vector<u16> *codes;
				std::vector<std::string> files;
				std::string header, currentSource;
				size_t lastPos = 0, pos = 0;

				source.append("\n");
				
				while((pos = source.find('\n', lastPos)) != std::string::npos) 
				{
					std::string temp = source.substr(lastPos, pos - lastPos);
					if(!temp.empty())
						files.push_back(temp);
					lastPos = pos + 1;
				}

				lines = (int)files.size();

				if(lines == 0) 
				{
					printf("ERROR: Must specify at least one file\n");
					return 1;
				}
				
				codes = new std::vector<u16>[lines];

				for(int i = 0; i < lines; i++) 
				{
					if (!File::ReadFileToString(true, files[i].c_str(), currentSource))
					{
						printf("ERROR reading %s, skipping...\n", files[i].c_str());
						lines--;
					}
					else 
					{
						if(!Assemble(currentSource.c_str(), codes[i])) 
						{
							printf("Assemble: Assembly of %s failed due to errors\n", 
									files[i].c_str());
							lines--;
						}
					}
				}
				

				CodesToHeader(codes, files, lines, output_header_name.c_str(), header);
				File::WriteStringToFile(true, header, (output_header_name + ".h").c_str());

				delete[] codes;
			}
			else
			{
				std::vector<u16> code;

				if(!Assemble(source.c_str(), code)) {
					printf("Assemble: Assembly failed due to errors\n");
					return 1;
				}

				if (!output_name.empty())
				{
					std::string binary_code;
					CodeToBinaryStringBE(code, binary_code);
					File::WriteStringToFile(false, binary_code, output_name.c_str());
				}
				if (!output_header_name.empty())
				{
					std::string header;
					CodeToHeader(code, input_name, output_header_name.c_str(), header);
					File::WriteStringToFile(true, header, (output_header_name + ".h").c_str());
				}
			}
		}
		source.clear();
	}

	printf("Assembly completed successfully!\n");

	return 0;
}
