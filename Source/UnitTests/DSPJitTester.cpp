#include "DSPJitTester.h"

DSPJitTester::DSPJitTester(u16 opcode, u16 opcode_ext, bool verbose, bool only_failed)
	: be_verbose(verbose), failed_only(only_failed), run_count(0), fail_count(0)
{
	instruction = opcode | opcode_ext;
	opcode_template = GetOpTemplate(instruction);
	sprintf(instruction_name, "%s", opcode_template->name);
	if (opcode_template->extended) 
		sprintf(&instruction_name[strlen(instruction_name)], "'%s", 
			extOpTable[instruction & (((instruction >> 12) == 0x3) ? 0x7F : 0xFF)]->name);
}
bool DSPJitTester::Test(SDSP dsp_settings)
{
	if (be_verbose && !failed_only)
	{
		printf("Running %s: ", instruction_name);
		DumpRegs(dsp_settings);
	}
	
	last_input_dsp = dsp_settings;
	last_int_dsp = RunInterpreter(dsp_settings);
	last_jit_dsp = RunJit(dsp_settings);

	run_count++;
	bool success = AreEqual(last_int_dsp, last_jit_dsp);
	if (!success)
		fail_count++;
	return success;
}
SDSP DSPJitTester::RunInterpreter(SDSP dsp_settings)
{
	ResetInterpreter();
	memcpy(&g_dsp, &dsp_settings, sizeof(SDSP));
	ExecuteInstruction(instruction);

	return g_dsp;
}
SDSP DSPJitTester::RunJit(SDSP dsp_settings)
{
	ResetJit();
	memcpy(&g_dsp, &dsp_settings, sizeof(SDSP));
	const u8* code = jit.GetCodePtr();
	jit.EmitInstruction(instruction);
	jit.RET();
	((void(*)())code)();

	return g_dsp;
}
void DSPJitTester::ResetInterpreter()
{
	for (int i=0; i < WRITEBACKLOGSIZE; i++)
		writeBackLogIdx[i] = -1;
}
void DSPJitTester::ResetJit()
{
	jit.ClearCodeSpace();
}
bool DSPJitTester::AreEqual(SDSP& int_dsp, SDSP& jit_dsp)
{
	bool equal = true;
	for (int i = 0; i < DSP_REG_NUM; i++)
	{
		if (int_dsp.r[i] != jit_dsp.r[i])
		{
			if (equal)
			{
				if (failed_only)
				{
					printf("%s ", instruction_name);
					DumpRegs(last_input_dsp);
				}
				if (be_verbose || failed_only)
					printf("failed\n");
			}
			equal = false;
			if (be_verbose || failed_only)
				printf("\t%s: int = 0x%04x, jit = 0x%04x\n", regnames[i].name, int_dsp.r[i], jit_dsp.r[i]);
		}
	}

	//TODO: more sophisticated checks?
	if (!int_dsp.iram || !jit_dsp.iram)
	{
		if (be_verbose)
			printf("(IRAM null)");
	}
	else if (memcmp(int_dsp.iram, jit_dsp.iram, DSP_IRAM_BYTE_SIZE))
	{
		printf("\tIRAM: different\n");
		equal = false;
	}
	if (!int_dsp.dram || !jit_dsp.dram)
	{
		if (be_verbose)
			printf("(DRAM null)");
	}
	else if (memcmp(int_dsp.dram, jit_dsp.dram, DSP_DRAM_BYTE_SIZE))
	{
		printf("\tDRAM: different\n");
		equal = false;
	}

	if (equal && be_verbose && !failed_only)
		printf("passed\n");
	return equal;
}
void DSPJitTester::Report()
{
	printf("%s (0x%04x): Ran %d times, Failed %d times.\n", instruction_name, instruction, run_count, fail_count);
}
void DSPJitTester::DumpJittedCode()
{
	ResetJit();
	const u8* code = jit.GetCodePtr();
	jit.EmitInstruction(instruction);
	size_t code_size = jit.GetCodePtr() - code;

	printf("%s emitted: ", instruction_name);
	for (size_t i = 0; i < code_size; i++)
		printf("%02x ", code[i]);
	printf("\n");
}
void DSPJitTester::DumpRegs(SDSP& dsp)
{
	for (int i = 0; i < DSP_REG_NUM; i++)
		if (dsp.r[i])
			printf("%s=0x%04x ", regnames[i].name, dsp.r[i]);
}
void DSPJitTester::Initialize()
{
	//init int
	InitInstructionTable();
}

int DSPJitTester::TestOne(TestDataIterator it, SDSP& dsp)
{
	int failed = 0;
	if (it != test_values.end())
	{
		u8 reg = it->first;
		TestData& data = it->second;
		it++;
		for (TestData::size_type i = 0; i < data.size(); i++)
		{
			dsp.r[reg] = data.at(i);
			failed += TestOne(it, dsp);
		}
	}
	else
	{
		if (!Test(dsp))
			failed++;
	}
	return failed;
}
int DSPJitTester::TestAll(bool verbose_fail)
{
	int failed = 0;

	SDSP dsp;
	memset(&dsp, 0, sizeof(SDSP));
	//from DSPCore_Init
	dsp.irom = (u16*)AllocateMemoryPages(DSP_IROM_BYTE_SIZE);
	dsp.iram = (u16*)AllocateMemoryPages(DSP_IRAM_BYTE_SIZE);
	dsp.dram = (u16*)AllocateMemoryPages(DSP_DRAM_BYTE_SIZE);
	dsp.coef = (u16*)AllocateMemoryPages(DSP_COEF_BYTE_SIZE);

	// Fill roms with zeros. 
	memset(dsp.irom, 0, DSP_IROM_BYTE_SIZE);
	memset(dsp.coef, 0, DSP_COEF_BYTE_SIZE);
	memset(dsp.dram, 0, DSP_DRAM_BYTE_SIZE);
	// Fill IRAM with HALT opcodes.
	for (int i = 0; i < DSP_IRAM_SIZE; i++)
		dsp.iram[i] = 0x0021; // HALT opcode

	bool verbose = failed_only;
	failed_only = verbose_fail;
	failed += TestOne(test_values.begin(), dsp);
	failed_only = verbose;

	FreeMemoryPages(dsp.irom, DSP_IROM_BYTE_SIZE);
	FreeMemoryPages(dsp.iram, DSP_IRAM_BYTE_SIZE);
	FreeMemoryPages(dsp.dram, DSP_DRAM_BYTE_SIZE);
	FreeMemoryPages(dsp.coef, DSP_COEF_BYTE_SIZE);

	return failed;
}
void DSPJitTester::AddTestData(u8 reg)
{
	AddTestData(reg, 0);
	AddTestData(reg, 1);
	AddTestData(reg, 0x1fff);
	AddTestData(reg, 0x2000);
	AddTestData(reg, 0x2001);
	AddTestData(reg, 0x7fff);
	AddTestData(reg, 0x8000);
	AddTestData(reg, 0x8001);
	AddTestData(reg, 0xfffe);
	AddTestData(reg, 0xffff);
}
void DSPJitTester::AddTestData(u8 reg, u16 value)
{
	if (reg < DSP_REG_NUM)
		test_values[reg].push_back(value);
}

