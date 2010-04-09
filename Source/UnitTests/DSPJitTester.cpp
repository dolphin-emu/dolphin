#include "DSPJitTester.h"

bool DSPJitTester::Test(SDSP dsp_settings)
{
	if (be_verbose)
		printf("Running %s: ", instruction_name);
	
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
	jit.WriteCallInterpreter(instruction);
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
	for (int i = 0; i < 32; i++)
	{
		if (int_dsp.r[i] != jit_dsp.r[i])
		{
			if (equal && be_verbose)
				printf("failed\n");
			equal = false;
			if (be_verbose)
				printf("\t%s: int = 0x%04x, jit = 0x%04x\n", regnames[i].name, int_dsp.r[i], jit_dsp.r[i]);
		}
	}
	if (equal && be_verbose)
		printf("passed\n");
	return equal;
}
void DSPJitTester::Report()
{
	printf("%s (0x%04x): Ran %d times, Failed %d times.\n", instruction_name, instruction, run_count, fail_count);
}
void DSPJitTester::Initialize()
{
	//init int
	InitInstructionTable();
}