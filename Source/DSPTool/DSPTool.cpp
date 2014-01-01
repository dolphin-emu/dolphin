// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "DSP/DSPCodeUtil.h"
#include "DSP/DSPTables.h"

// Stub out the dsplib host stuff, since this is just a simple cmdline tools.
u8 DSPHost_ReadHostMemory(u32 addr) { return 0; }
void DSPHost_WriteHostMemory(u8 value, u32 addr) {}
void DSPHost_OSD_AddMessage(const std::string& str, u32 ms) {}
bool DSPHost_OnThread() { return false; }
bool DSPHost_Wii() { return false; }
void DSPHost_CodeLoaded(const u8 *ptr, int size) {}
void DSPHost_InterruptRequest() {}
void DSPHost_UpdateDebugger() {}

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
	File::WriteStringToFile(text1, "code1.txt");
	File::WriteStringToFile(text2, "code2.txt");
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
	File::ReadFileToString("testdata/dsp_test.S", &text_orig);
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


	if (File::ReadFileToString("C:/devkitPro/examples/wii/asndlib/dsptest/dsp_test.ds", &dsp_test))
		SuperTrip(dsp_test.c_str());

	//.File::ReadFileToString("C:/devkitPro/trunk/libogc/libasnd/dsp_mixer/dsp_mixer.s", &dsp_test);
	// This is CLOSE to working. Sorry about the local path btw. This is preliminary code.
*/

	std::string dsp_test;
	if (File::ReadFileToString("Testdata/dsp_test.s", dsp_test))
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
//   dsptool [-f] -o asdf.bin asdf.txt
// Assemble a file, output header:
//   dsptool [-f] -h asdf.h asdf.txt
// Print results from DSPSpy register dump
//   dsptool -p dsp_dump0.bin
// So far, all this binary can do is test partially that itself works correctly.
int main(int argc, const char *argv[])
{
	if(argc == 1 || (argc == 2 && (!strcmp(argv[1], "--help") || (!strcmp(argv[1], "-?")))))
	{
		printf("USAGE: DSPTool [-?] [--help] [-f] [-d] [-m] [-p <FILE>] [-o <FILE>] [-h <FILE>] <DSP ASSEMBLER FILE>\n");
		printf("-? / --help: Prints this message\n");
		printf("-d: Disassemble\n");
		printf("-m: Input file contains a list of files (Header assembly only)\n");
		printf("-s: Print the final size in bytes (only)\n");
		printf("-f: Force assembly (errors are not critical)\n");
		printf("-o <OUTPUT FILE>: Results from stdout redirected to a file\n");
		printf("-h <HEADER FILE>: Output assembly results to a header\n");
		printf("-p <DUMP FILE>: Print results of DSPSpy register dump\n");
		printf("-ps <DUMP FILE>: Print results of DSPSpy register dump (disable SR output)\n");
		printf("-pm <DUMP FILE>: Print results of DSPSpy register dump (convert PROD values)\n");
		printf("-psm <DUMP FILE>: Print results of DSPSpy register dump (convert PROD values/disable SR output)\n");

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

	bool disassemble = false, compare = false, multiple = false, outputSize = false,
		force = false, print_results = false, print_results_prodhack = false, print_results_srhack = false;
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
		else if (!strcmp(argv[i], "-s"))
			outputSize = true;
		else if (!strcmp(argv[i], "-m"))
			multiple = true;
		else if (!strcmp(argv[i], "-f"))
			force = true;
		else if (!strcmp(argv[i], "-p"))
			print_results = true;
		else if (!strcmp(argv[i], "-ps")) {
			print_results = true;
			print_results_srhack = true;
		}
		else if (!strcmp(argv[i], "-pm")) {
			print_results = true;
			print_results_prodhack = true;
		}
		else if (!strcmp(argv[i], "-psm")) {
			print_results = true;
			print_results_srhack = true;
			print_results_prodhack = true;
		}
		else
		{
			if (!input_name.empty())
			{
				printf("ERROR: Can only take one input file.\n");
				return 1;
			}
			input_name = argv[i];
			if (!File::Exists(input_name))
			{
				printf("ERROR: Input path does not exist.\n");
				return 1;
			}
		}
	}

	if(multiple && (compare || disassemble || !output_name.empty() ||
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
		File::ReadFileToString(input_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code1);
		File::ReadFileToString(output_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code2);
		Compare(code1, code2);
		return 0;
	}

	if (print_results)
	{
		std::string dumpfile, results;
		std::vector<u16> reg_vector;

		File::ReadFileToString(input_name.c_str(), dumpfile);
		BinaryStringBEToCode(dumpfile, reg_vector);

		results.append("Start:\n");
		for (int initial_reg = 0; initial_reg < 32; initial_reg++)
		{
			results.append(StringFromFormat("%02x %04x ", initial_reg, reg_vector.at(initial_reg)));
			if ((initial_reg + 1) % 8 == 0)
				results.append("\n");
		}
		results.append("\n");
		results.append("Step [number]:\n[Reg] [last value] [current value]\n\n");
		for (unsigned int step = 1; step < reg_vector.size()/32; step++)
		{
			bool changed = false;
			u16 current_reg;
			u16 last_reg;
			u32 htemp;
			//results.append(StringFromFormat("Step %3d: (CW 0x%04x) UC:%03d\n", step, 0x8fff+step, (step-1)/32));
			results.append(StringFromFormat("Step %3d:\n", step));
			for (int reg = 0; reg < 32; reg++)
			{
				if ((reg >= 0x0c) && (reg <= 0x0f)) continue;
				if (print_results_srhack && (reg == 0x13)) continue;

				if ((print_results_prodhack) && (reg >= 0x15) && (reg <= 0x17)) {
					switch (reg) {
						case 0x15: //DSP_REG_PRODM
							last_reg = reg_vector.at((step*32-32)+reg) + reg_vector.at((step*32-32)+reg+2);
							current_reg = reg_vector.at(step*32+reg) + reg_vector.at(step*32+reg+2);
							break;
						case 0x16: //DSP_REG_PRODH
							htemp = ((reg_vector.at(step*32+reg-1) + reg_vector.at(step*32+reg+1))&~0xffff)>>16;
							current_reg = (u8)(reg_vector.at(step*32+reg) + htemp);
							htemp = ((reg_vector.at(step*32-32+reg-1) + reg_vector.at(step*32-32+reg+1))&~0xffff)>>16;
							last_reg = (u8)(reg_vector.at(step*32-32+reg) + htemp);
							break;
						case 0x17: //DSP_REG_PRODM2
						default:
							current_reg = 0;
							last_reg = 0;
							break;
					}
				}
				else {
					current_reg = reg_vector.at(step*32+reg);
					last_reg = reg_vector.at((step*32-32)+reg);
				}
				if (last_reg != current_reg)
				{
					results.append(StringFromFormat("%02x %-7s: %04x %04x\n", reg, pdregname(reg), last_reg, current_reg));
					changed = true;
				}
			}
			if (!changed)
				results.append("No Change\n\n");
			else
				results.append("\n");
		}

		if (!output_name.empty())
			File::WriteStringToFile(results, output_name.c_str());
		else
			printf("%s", results.c_str());
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
		File::ReadFileToString(input_name.c_str(), binary_code);
		BinaryStringBEToCode(binary_code, code);
		std::string text;
		Disassemble(code, true, text);
		if (!output_name.empty())
			File::WriteStringToFile(text, output_name.c_str());
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
		if (File::ReadFileToString(input_name.c_str(), source))
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

				for (int i = 0; i < lines; i++)
				{
					if (!File::ReadFileToString(files[i].c_str(), currentSource))
					{
						printf("ERROR reading %s, skipping...\n", files[i].c_str());
						lines--;
					}
					else
					{
						if (!Assemble(currentSource.c_str(), codes[i], force))
						{
							printf("Assemble: Assembly of %s failed due to errors\n",
									files[i].c_str());
							lines--;
						}
						if (outputSize) {
							printf("%s: %d\n", files[i].c_str(), (int)codes[i].size());
						}
					}
				}


				CodesToHeader(codes, &files, lines, output_header_name.c_str(), header);
				File::WriteStringToFile(header, (output_header_name + ".h").c_str());

				delete[] codes;
			}
			else
			{
				std::vector<u16> code;

				if (!Assemble(source.c_str(), code, force)) {
					printf("Assemble: Assembly failed due to errors\n");
					return 1;
				}

				if (outputSize) {
					printf("%s: %d\n", input_name.c_str(), (int)code.size());
				}

				if (!output_name.empty())
				{
					std::string binary_code;
					CodeToBinaryStringBE(code, binary_code);
					File::WriteStringToFile(binary_code, output_name.c_str());
				}
				if (!output_header_name.empty())
				{
					std::string header;
					CodeToHeader(code, input_name, output_header_name.c_str(), header);
					File::WriteStringToFile(header, (output_header_name + ".h").c_str());
				}
			}
		}
		source.clear();
	}

	if(!outputSize)
		printf("Assembly completed successfully!\n");

	return 0;
}
