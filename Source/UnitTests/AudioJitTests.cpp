#include "DSPJitTester.h"

void nx_dr()
{
	DSPJitTester tester(0x8000, 0x0004);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}

void nx_ir()
{
	DSPJitTester tester(0x8000, 0x0008);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}

void nx_nr()
{
	DSPJitTester tester(0x8000, 0x000c);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.TestAll(true);
	tester.Report();
}

void dar()
{
	DSPJitTester tester(0x0004);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}
void iar()
{
	DSPJitTester tester(0x0008);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}
void subarn()
{
	DSPJitTester tester(0x000c);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.TestAll();
	tester.Report();
}
void addarn()
{
	DSPJitTester tester(0x0010);
	tester.AddTestData(DSP_REG_ACC0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.TestAll();
	tester.Report();
}
void sbclr()
{
	DSPJitTester tester(0x1200);
	tester.AddTestData(DSP_REG_SR);
	tester.TestAll();
	tester.Report();
}
void sbset()
{
	DSPJitTester tester(0x1300);
	tester.AddTestData(DSP_REG_SR);
	tester.TestAll();
	tester.Report();
}

void AudioJitTests()
{
	DSPJitTester::Initialize();

	dar();
	iar();
	subarn();
	addarn();
	sbclr();
	sbset();

	nx_ir();
	nx_dr();
	nx_nr();
}

//required to be able to link against DSPCore
void DSPHost_UpdateDebugger() { }
unsigned int DSPHost_CodeLoaded(unsigned const char*, int) { return 0; }
void DSPHost_InterruptRequest() { }
bool DSPHost_OnThread() { return false; }
void DSPHost_WriteHostMemory(unsigned char, unsigned int) { }
unsigned char DSPHost_ReadHostMemory(unsigned int) { return 0; }
