#include "DSPJitTester.h"

void nx_dr()
{
	DSPJitTester tester(0x8000, 0x0004);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}

void nx_ir()
{
	DSPJitTester tester(0x8000, 0x0008);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}

void nx_nr()
{
	DSPJitTester tester(0x8000, 0x000c);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.TestAll(true);
	tester.Report();
}

void nx_mv()
{
	DSPJitTester tester(0x8000, 0x0010);
	tester.AddTestData(DSP_REG_ACL0);
	tester.AddTestData(DSP_REG_AXL0);
	tester.TestAll(true);
	tester.Report();
}

void dar()
{
	DSPJitTester tester(0x0004);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}
void iar()
{
	DSPJitTester tester(0x0008);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.TestAll();
	tester.Report();
}
void subarn()
{
	DSPJitTester tester(0x000c);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.TestAll();
	tester.Report();
}
void addarn()
{
	DSPJitTester tester(0x0010);
	tester.AddTestData(DSP_REG_AR0);
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

void nx_s() 
{
	DSPJitTester tester(0x8000, 0x0020);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_ACL0);
	tester.TestAll(true);
	tester.Report();
}

void nx_sn() 
{
	DSPJitTester tester(0x8000, 0x0024);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.AddTestData(DSP_REG_ACL0);
	tester.TestAll(true);
	tester.Report();
}

void nx_l() 
{

	DSPJitTester tester(0x8000, 0x0040);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_AXL0);
	tester.TestAll(true);
	tester.Report();
}

void set16_l() 
{

	DSPJitTester tester(0x8e00, 0x0070);
	tester.AddTestData(DSP_REG_SR, 0);
	tester.AddTestData(DSP_REG_SR, SR_40_MODE_BIT);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_ACM0);
	tester.TestAll(true);
	tester.Report();
}

void nx_ln() 
{
	DSPJitTester tester(0x8000, 0x0044);
	tester.AddTestData(DSP_REG_AR0);
	tester.AddTestData(DSP_REG_WR0);
	tester.AddTestData(DSP_REG_IX0);
	tester.AddTestData(DSP_REG_AXL0);
	tester.TestAll(true);
	tester.Report();
}

void nx_ls() 
{
	DSPJitTester tester1(0x8000, 0x0080);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x0080);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_lsn() 
{
	DSPJitTester tester1(0x8000, 0x0084);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.AddTestData(DSP_REG_IX0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x0084);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_lsm() 
{
	DSPJitTester tester1(0x8000, 0x0088);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x0088);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.AddTestData(DSP_REG_IX3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_lsnm() 
{
	DSPJitTester tester1(0x8000, 0x008c);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.AddTestData(DSP_REG_IX0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x008c);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.AddTestData(DSP_REG_IX3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_sl() 
{
	DSPJitTester tester1(0x8000, 0x0082);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x0082);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_sln() 
{
	DSPJitTester tester1(0x8000, 0x0086);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.AddTestData(DSP_REG_IX0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x0086);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_slm() 
{
	DSPJitTester tester1(0x8000, 0x008a);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x008a);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.AddTestData(DSP_REG_IX3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_slnm() 
{
	DSPJitTester tester1(0x8000, 0x008e);
	tester1.AddTestData(DSP_REG_ACM0);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.AddTestData(DSP_REG_IX0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x008e);
	tester2.AddTestData(DSP_REG_ACM0);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.AddTestData(DSP_REG_IX3);
	tester2.TestAll(true);
	tester2.Report();
}

void nx_ld() 
{
	DSPJitTester tester1(0x8000, 0x00c0);
	tester1.AddTestData(DSP_REG_AXL0,0xdead);
	tester1.AddTestData(DSP_REG_AXL1,0xbeef);
	tester1.AddTestData(DSP_REG_AR0);
	tester1.AddTestData(DSP_REG_WR0);
	tester1.AddTestData(DSP_REG_IX0);
	tester1.TestAll(true);
	tester1.Report();

	DSPJitTester tester2(0x8000, 0x00c0);
	tester2.AddTestData(DSP_REG_AXL0,0xdead);
	tester2.AddTestData(DSP_REG_AXL1,0xbeef);
	tester2.AddTestData(DSP_REG_AR3);
	tester2.AddTestData(DSP_REG_WR3);
	tester2.AddTestData(DSP_REG_IX3);
	tester2.TestAll(true);
	tester2.Report();
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
	nx_mv();

	set16_l();

	nx_s();
	nx_sn();
	nx_l();
	nx_ln();
	nx_ls();
	nx_lsn();
	nx_lsm();
	nx_lsnm();
	nx_sl();
	nx_sln();
	nx_slm();
	nx_slnm();
	nx_ld();
}

//required to be able to link against DSPCore
void DSPHost_UpdateDebugger() { }
void DSPHost_CodeLoaded(unsigned const char*, int) { }
void DSPHost_InterruptRequest() { }
bool DSPHost_OnThread() { return false; }
void DSPHost_WriteHostMemory(unsigned char, unsigned int) { }
unsigned char DSPHost_ReadHostMemory(unsigned int) { return 0; }
