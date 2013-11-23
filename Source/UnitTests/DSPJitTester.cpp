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
	jit.ABI_PushAllCalleeSavedRegsAndAdjustStack();
	jit.EmitInstruction(instruction);
	jit.ABI_PopAllCalleeSavedRegsAndAdjustStack();
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

static u16 GetRegister(SDSP const &dsp, int reg) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		return dsp.r.ar[reg - DSP_REG_AR0];
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		return dsp.r.ix[reg - DSP_REG_IX0];
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		return dsp.r.wr[reg - DSP_REG_WR0];
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		return dsp.r.st[reg - DSP_REG_ST0];
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		return dsp.r.ac[reg - DSP_REG_ACH0].h;
	case DSP_REG_CR:     return dsp.r.cr;
	case DSP_REG_SR:     return dsp.r.sr;
	case DSP_REG_PRODL:  return dsp.r.prod.l;
	case DSP_REG_PRODM:  return dsp.r.prod.m;
	case DSP_REG_PRODH:  return dsp.r.prod.h;
	case DSP_REG_PRODM2: return dsp.r.prod.m2;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		return dsp.r.ax[reg - DSP_REG_AXL0].l;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		return dsp.r.ax[reg - DSP_REG_AXH0].h;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		return dsp.r.ac[reg - DSP_REG_ACL0].l;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		return dsp.r.ac[reg - DSP_REG_ACM0].m;
	default:
		_assert_msg_(DSP_CORE, 0, "cannot happen");
		return 0;
	}
}

static void SetRegister(SDSP &dsp, int reg, u16 val) {
	switch(reg) {
	case DSP_REG_AR0:
	case DSP_REG_AR1:
	case DSP_REG_AR2:
	case DSP_REG_AR3:
		dsp.r.ar[reg - DSP_REG_AR0] = val; break;
	case DSP_REG_IX0:
	case DSP_REG_IX1:
	case DSP_REG_IX2:
	case DSP_REG_IX3:
		dsp.r.ix[reg - DSP_REG_IX0] = val; break;
	case DSP_REG_WR0:
	case DSP_REG_WR1:
	case DSP_REG_WR2:
	case DSP_REG_WR3:
		dsp.r.wr[reg - DSP_REG_WR0] = val; break;
	case DSP_REG_ST0:
	case DSP_REG_ST1:
	case DSP_REG_ST2:
	case DSP_REG_ST3:
		dsp.r.st[reg - DSP_REG_ST0] = val; break;
	case DSP_REG_ACH0:
	case DSP_REG_ACH1:
		dsp.r.ac[reg - DSP_REG_ACH0].h = val; break;
	case DSP_REG_CR:     dsp.r.cr = val; break;
	case DSP_REG_SR:     dsp.r.sr = val; break;
	case DSP_REG_PRODL:  dsp.r.prod.l = val; break;
	case DSP_REG_PRODM:  dsp.r.prod.m = val; break;
	case DSP_REG_PRODH:  dsp.r.prod.h = val; break;
	case DSP_REG_PRODM2: dsp.r.prod.m2 = val; break;
	case DSP_REG_AXL0:
	case DSP_REG_AXL1:
		dsp.r.ax[reg - DSP_REG_AXL0].l = val; break;
	case DSP_REG_AXH0:
	case DSP_REG_AXH1:
		dsp.r.ax[reg - DSP_REG_AXH0].h = val; break;
	case DSP_REG_ACL0:
	case DSP_REG_ACL1:
		dsp.r.ac[reg - DSP_REG_ACL0].l = val; break;
	case DSP_REG_ACM0:
	case DSP_REG_ACM1:
		dsp.r.ac[reg - DSP_REG_ACM0].m = val; break;
	default:
		_assert_msg_(DSP_CORE, 0, "cannot happen");
	}
}

bool DSPJitTester::AreEqual(SDSP& int_dsp, SDSP& jit_dsp)
{
	bool equal = true;
	for (int i = 0; i < DSP_REG_NUM; i++)
	{
	    if (GetRegister(int_dsp,i) != GetRegister(jit_dsp, i))
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
				printf("\t%s: int = 0x%04x, jit = 0x%04x\n", regnames[i].name, GetRegister(int_dsp,i), GetRegister(jit_dsp, i));
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
		if (GetRegister(dsp,i))
			printf("%s=0x%04x ", regnames[i].name, GetRegister(dsp,i));
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
			SetRegister(dsp, reg, data.at(i));
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

	// Fill roms with distinct patterns.
	for (int i = 0; i < DSP_IROM_SIZE; i++)
	    dsp.irom[i] = (i & 0x3fff) | 0x4000;
	for (int i = 0; i < DSP_COEF_SIZE; i++)
	    dsp.coef[i] = (i & 0x3fff) | 0x8000;
	for (int i = 0; i < DSP_DRAM_SIZE; i++)
	    dsp.dram[i] = (i & 0x3fff) | 0xc000;
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

