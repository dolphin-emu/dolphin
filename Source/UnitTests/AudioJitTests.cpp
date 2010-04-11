#include "DSPJitTester.h"

void nx_dr()
{
	SDSP test_dsp;
	DSPJitTester tester(0x40, 0x04);

	for (u16 input_reg = 0; input_reg < 50; input_reg++)
	for (u16 input_wr0 = 0; input_wr0 < 10; input_wr0++)
	{
		memset(&test_dsp, 0, sizeof(SDSP));
		test_dsp.r[DSP_REG_WR0] = input_wr0;
		test_dsp.r[0] = input_reg;
		if (!tester.Test(test_dsp))
		{
			printf("%s Test failed: in = 0x%04x, wr0 = 0x%04x > int = 0x%04x, jit = 0x%04x\n", 
				tester.GetInstructionName(),
				input_reg, input_wr0,
				tester.GetLastInterpreterDSP().r[0], tester.GetLastJitDSP().r[0]);
		}
	}
	tester.Report();
}

void nx_ir()
{
	SDSP test_dsp;
	DSPJitTester tester(0x40, 0x08);

	for (u16 input_reg = 0; input_reg < 50; input_reg++)
	for (u16 input_wr0 = 0; input_wr0 < 10; input_wr0++)
	{
		memset(&test_dsp, 0, sizeof(SDSP));
		test_dsp.r[DSP_REG_WR0] = input_wr0;
		test_dsp.r[0] = input_reg;
		if (!tester.Test(test_dsp))
		{
			printf("%s Test failed: in = 0x%04x, wr0 = 0x%04x > int = 0x%04x, jit = 0x%04x\n", 
				tester.GetInstructionName(),
				input_reg, input_wr0,
				tester.GetLastInterpreterDSP().r[0], tester.GetLastJitDSP().r[0]);
		}
	}
	tester.Report();
}

void nx_nr()
{
	SDSP test_dsp;
	DSPJitTester tester(0x40, 0x0c, true);

	for (u16 input_reg = 0; input_reg < 10; input_reg++)
	for (u16 input_wr0 = 0; input_wr0 < 10; input_wr0++)
	for (u16 input_ix0 = 0; input_ix0 < 10; input_ix0++)
	{
		memset(&test_dsp, 0, sizeof(SDSP));
		test_dsp.r[DSP_REG_IX0] = input_ix0;
		test_dsp.r[DSP_REG_WR0] = input_wr0;
		test_dsp.r[0] = input_reg;
		if (!tester.Test(test_dsp))
		{
			printf("%s Test failed: in = 0x%04x, wr0 = 0x%04x, ix0 = 0x%04x > int = 0x%04x, jit = 0x%04x\n", 
				tester.GetInstructionName(),
				input_reg, input_wr0, input_ix0,
				tester.GetLastInterpreterDSP().r[0], tester.GetLastJitDSP().r[0]);
		}
	}
	tester.Report();
}

void AudioJitTests()
{
	DSPJitTester::Initialize();

	nx_ir();
	nx_dr();
	//nx_nr();
}

//required to be able to link against DSPCore
void DSPHost_UpdateDebugger() { }
unsigned int DSPHost_CodeLoaded(unsigned const char*, int) { return 0; }
void DSPHost_InterruptRequest() { }
bool DSPHost_OnThread() { return false; }
void DSPHost_WriteHostMemory(unsigned char, unsigned int) { }
unsigned char DSPHost_ReadHostMemory(unsigned int) { return 0; }