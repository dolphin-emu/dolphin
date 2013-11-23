// How to use the DSPJitTester:
//
// == Before running ==
// Make sure to call Initialize to set initial stuff required by int and jit:
// DSPJitTester::Initialize();
//
// == Creation of a testcase ==
// Create a testcase for a normal operation:
// DSPJitTester tester(0x0004); //taken from DSPTables.cpp, opcodes[]
//
// Create a testcase for an extended operation:
// DSPJitTester tester(0x8000, 0x0004); //NX from opcodes, DR from opcodes_ext
//
// By default, no messages are written.
// To log all operations, set verbose to true:
// DSPJitTester tester(0x8000, 0x0004, true);
//
// You can also choose to only print failing tests:
// DSPJitTester tester(0x8000, 0x0004, verbosity_setting, true);
// verbose = true will give the same output as verbose,
// while verbose = false will only (really!) print failing tests.
//
// == Setting up values ==
// You can set the tester up with values for each DSP register:
// tester.AddTestData(DSP_REG_ACC0, 1);
// tester.AddTestData(DSP_REG_ACC0, 2);
// tester.AddTestData(DSP_REG_ACC0, 3);
//
// You can also choose to have a few predefined values added for a register:
// tester.AddTestData(DSP_REG_ACC0); //see the method body for the values added
//
// == Running the tests ==
// After setup, you can either run JIT against the interpreter
// using all predefined register values, pass your own set of
// registers or run either of the two independently from each other.
//
// int failed_tests = tester.TestAll(); //run jit against int, using values from AddTestData
// int failed_tests = tester.TestAll(true); //override the value for only_failed to show failure
//
// SDSP dsp = GetCustomSetOfRegisters();
// bool success = tester.Test(dsp); //run jit against int, using a custom set of register values
//
// SDSP result = tester.RunInterpreter(dsp); //run int alone
// SDSP result = tester.RunJit(dsp); //run jit alone
//
// == Examining results ==
// When either verbose or only_failed is set to true, the tester will automatically report
// failure to stdout, along with input registers and the differences in output registers.
//
// tester.Report(); //display a small report afterwards
//
// SDSP int = tester.GetLastInterpreterDSP(); //examine the DSP set left after running int
// SDSP jit = tester.GetLastJitDSP(); //same for jit
//
// int tests_run = tester.GetRunCount();
// int tests_failed = tester.GetFailCount();
// const char* tested_instruction = tester.GetInstructionName();
// printf("%s ran %d tests and failed %d times\n", tested_instruction, tests_run, tests_failed);
//
// tester.DumpJittedCode(); //prints the code bytes produced by jit (examine with udcli/udis86 or similar)

#ifndef __DSP_JIT_TESTER_
#define __DSP_JIT_TESTER_

#include "DSP/DSPCore.h"
#include "DSP/DSPInterpreter.h"
#include <map>
#include <vector>

typedef std::vector<u16> TestData;
typedef std::map<u8, TestData> TestDataList;
typedef TestDataList::iterator TestDataIterator;
#define DSP_REG_NUM 32

class DSPJitTester
{
	UDSPInstruction instruction;
	const DSPOPCTemplate *opcode_template;
	DSPEmitter jit;
	SDSP last_int_dsp;
	SDSP last_jit_dsp;
	SDSP last_input_dsp;
	bool be_verbose;
	bool failed_only;
	int run_count;
	int fail_count;
	char instruction_name[16];
	TestDataList test_values;

	bool AreEqual(SDSP&, SDSP&);
	int TestOne(TestDataIterator, SDSP&);
	void DumpRegs(SDSP&);
public:
	DSPJitTester(u16 opcode, u16 opcode_ext = 0, bool verbose = false, bool only_failed = false);
	bool Test(SDSP);
	int TestAll() { return TestAll(failed_only); }
	int TestAll(bool verbose_fail);
	void AddTestData(u8 reg);
	void AddTestData(u8 reg, u16 value);
	SDSP RunInterpreter(SDSP);
	SDSP RunJit(SDSP);
	void ResetInterpreter();
	void ResetJit();
	inline SDSP GetLastInterpreterDSP() { return last_int_dsp; }
	inline SDSP GetLastJitDSP() { return last_jit_dsp; }
	inline int GetRunCount() { return run_count; }
	inline int GetFailCount() { return fail_count; }
	inline const char* GetInstructionName() { return instruction_name; }
	void Report();
	void DumpJittedCode();

	static void Initialize();
};

#endif
